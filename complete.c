/*
 * complete.c - Code completion utility using llama.cpp server
 * Sends partial code to local LLM at port 8080 and prints completion.
 *
 * Uses the IBM Granite / Qwen chat template format via /v1/chat/completions.
 *
 * Usage: ./complete [options]
 *   -p <prompt>   code snippet as argument
 *   -f <file>     read snippet from file
 *   -n <tokens>   max tokens to predict (default: 128)
 *   -t <temp>     temperature (default: 0.2)
 *   -h            show help
 *
 * Stdin is used if neither -p nor -f is given.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 8080
#define DEFAULT_MAX_TOKENS 40
#define DEFAULT_TEMPERATURE 0.2
#define BUF_SIZE (1024 * 256)
#define PROMPT_MAX (1024 * 32)

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

/* Minimal JSON string escaper — writes into dst, returns chars written. */
static int json_escape(const char *src, char *dst, int dstlen) {
    int n = 0;
    while (*src && n < dstlen - 8) {
        unsigned char c = (unsigned char)*src++;
        switch (c) {
            case '"':  dst[n++] = '\\'; dst[n++] = '"';  break;
            case '\\': dst[n++] = '\\'; dst[n++] = '\\'; break;
            case '\n': dst[n++] = '\\'; dst[n++] = 'n';  break;
            case '\r': dst[n++] = '\\'; dst[n++] = 'r';  break;
            case '\t': dst[n++] = '\\'; dst[n++] = 't';  break;
            default:
                if (c < 0x20) {
                    n += snprintf(dst + n, dstlen - n, "\\u%04x", c);
                } else {
                    dst[n++] = (char)c;
                }
        }
    }
    dst[n] = '\0';
    return n;
}

/*
 * Extract the value of a JSON string field named `key` from a blob.
 * Handles one level of nesting: finds the first occurrence.
 * Returns 1 on success.
 */
static int json_get_string(const char *json, const char *key, char *out, int outsz) {
    char needle[256];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *p = strstr(json, needle);
    if (!p) return 0;
    p += strlen(needle);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return 0;
    p++;
    int n = 0;
    while (*p && *p != '"' && n < outsz - 1) {
        if (*p == '\\' && *(p+1)) {
            p++;
            switch (*p) {
                case 'n': out[n++] = '\n'; break;
                case 'r': out[n++] = '\r'; break;
                case 't': out[n++] = '\t'; break;
                default:  out[n++] = *p;   break;
            }
        } else {
            out[n++] = *p;
        }
        p++;
    }
    out[n] = '\0';
    return 1;
}

static int tcp_connect(const char *host, int port) {
    struct hostent *he = gethostbyname(host);
    if (!he) die("gethostbyname failed for %s", host);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket: %s", strerror(errno));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((unsigned short)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        die("connect to %s:%d failed: %s", host, port, strerror(errno));

    return fd;
}

/*
 * POST to /v1/chat/completions (OpenAI-compatible endpoint).
 * Uses a system prompt to guide the model to output only code continuation.
 * Returns allocated string with the completion. Caller frees.
 */
static char *llm_complete(const char *code_snippet, int max_tokens, double temperature) {
    static const char *SYSTEM = "You are a code completion engine. "
        "The user will provide code containing a <FILL_HERE> marker. "
        "Output ONLY the code that replaces <FILL_HERE>, at most 1-3 lines. "
        "Raw code only — no explanation, no markdown fences, no preamble.";

    /* Escape user snippet and system prompt */
    int escbuf_sz = PROMPT_MAX * 6 + 64;
    char *esc_snippet = malloc(escbuf_sz);
    char *esc_system  = malloc(escbuf_sz);
    if (!esc_snippet || !esc_system) die("malloc");
    json_escape(code_snippet, esc_snippet, escbuf_sz);
    json_escape(SYSTEM, esc_system, escbuf_sz);

    /* Build JSON body for /v1/chat/completions */
    int body_sz = escbuf_sz * 2 + 512;
    char *body = malloc(body_sz);
    if (!body) die("malloc");
    int bodylen = snprintf(body, body_sz,
        "{"
          "\"model\":\"local\","
          "\"messages\":["
            "{\"role\":\"system\",\"content\":\"%s\"},"
            "{\"role\":\"user\",\"content\":\"%s\"}"
          "],"
          "\"max_tokens\":%d,"
          "\"temperature\":%.3f,"
          "\"stop\":[\"\\n\\n\"]"
        "}",
        esc_system, esc_snippet, max_tokens, temperature);
    free(esc_snippet);
    free(esc_system);

    /* Build HTTP request */
    char *req = malloc(bodylen + 512);
    if (!req) die("malloc");
    int reqlen = snprintf(req, bodylen + 512,
        "POST /v1/chat/completions HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        SERVER_HOST, SERVER_PORT, bodylen, body);
    free(body);

    int fd = tcp_connect(SERVER_HOST, SERVER_PORT);

    int sent = 0;
    while (sent < reqlen) {
        int n = (int)write(fd, req + sent, (size_t)(reqlen - sent));
        if (n <= 0) die("write: %s", strerror(errno));
        sent += n;
    }
    free(req);

    char *resp = malloc(BUF_SIZE);
    if (!resp) die("malloc");
    int total = 0;
    while (total < BUF_SIZE - 1) {
        int n = (int)read(fd, resp + total, (size_t)(BUF_SIZE - 1 - total));
        if (n <= 0) break;
        total += n;
    }
    resp[total] = '\0';
    close(fd);

    char *json_body = strstr(resp, "\r\n\r\n");
    if (!json_body) { free(resp); die("Invalid HTTP response (no body separator)"); }
    json_body += 4;

    /*
     * /v1/chat/completions response structure:
     * {"choices":[{"message":{"content":"..."}}], ...}
     * Find "content" inside the first choice's message object.
     */
    char *content = malloc(BUF_SIZE);
    if (!content) die("malloc");

    /* Locate "choices" then find the first "content" after it */
    const char *choices_pos = strstr(json_body, "\"choices\"");
    if (!choices_pos) {
        /* Fallback: check for error message */
        char errmsg[512] = "";
        json_get_string(json_body, "error", errmsg, sizeof(errmsg));
        free(resp);
        free(content);
        die("No 'choices' in response. Error: %s\nRaw: %.512s",
            errmsg[0] ? errmsg : "(none)", json_body);
    }

    if (!json_get_string(choices_pos, "content", content, BUF_SIZE)) {
        free(resp);
        free(content);
        die("Could not extract content from response.\nRaw: %.512s", json_body);
    }

    free(resp);
    return content;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) die("Cannot open %s: %s", path, strerror(errno));
    char *buf = malloc(PROMPT_MAX);
    if (!buf) die("malloc");
    int n = (int)fread(buf, 1, PROMPT_MAX - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static char *read_stdin(void) {
    char *buf = malloc(PROMPT_MAX);
    if (!buf) die("malloc");
    int n = (int)fread(buf, 1, PROMPT_MAX - 1, stdin);
    buf[n] = '\0';
    return buf;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "  -p <prompt>   code snippet as string\n"
        "  -f <file>     read snippet from file\n"
        "  -n <tokens>   max tokens to predict (default: %d)\n"
        "  -t <temp>     temperature 0.0-2.0 (default: %.1f)\n"
        "  -h            show this help\n"
        "\nReads from stdin if -p and -f are omitted.\n",
        prog, DEFAULT_MAX_TOKENS, DEFAULT_TEMPERATURE);
    exit(1);
}

int main(int argc, char *argv[]) {
    char *prompt = NULL;
    int max_tokens = DEFAULT_MAX_TOKENS;
    double temperature = DEFAULT_TEMPERATURE;
    int own_prompt = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
        } else if (strcmp(argv[i], "-p") == 0) {
            if (++i >= argc) die("-p requires argument");
            prompt = argv[i];
        } else if (strcmp(argv[i], "-f") == 0) {
            if (++i >= argc) die("-f requires argument");
            prompt = read_file(argv[i]);
            own_prompt = 1;
        } else if (strcmp(argv[i], "-n") == 0) {
            if (++i >= argc) die("-n requires argument");
            max_tokens = atoi(argv[i]);
            if (max_tokens <= 0) die("invalid -n value");
        } else if (strcmp(argv[i], "-t") == 0) {
            if (++i >= argc) die("-t requires argument");
            temperature = atof(argv[i]);
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
        }
    }

    if (!prompt) {
        prompt = read_stdin();
        own_prompt = 1;
    }

    char *completion = llm_complete(prompt, max_tokens, temperature);
    printf("%s", completion);
    int clen = (int)strlen(completion);
    if (clen == 0 || completion[clen-1] != '\n') putchar('\n');

    free(completion);
    if (own_prompt) free(prompt);
    return 0;
}
