/*
 * test_runner.c — Runs code-completion test cases against ./complete
 *
 * Pass criteria (ALL must hold):
 *   1. exit code 0
 *   2. non-empty, non-garbage output  (no repeated-char runs, no pure whitespace)
 *   3. result contains t->expected substring (case-insensitive); NULL = skip check
 *
 * Compile: cc -O2 -std=c99 -o test_runner test_runner.c
 * Run:     ./test_runner [filter]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define COMPLETE_BIN "./complete"
#define MAX_RESULT   8192
#define MAX_TESTS    512

typedef struct {
    const char *lang;
    const char *name;
    const char *snippet;    /* partial code ending at fill point */
    int         max_tokens;
    const char *expected;   /* required substring in result (case-insensitive); NULL = any */
} Test;

/* Case-insensitive strstr */
static const char *ci_strstr(const char *hay, const char *needle) {
    if (!needle || !*needle) return hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n))
            h++, n++;
        if (!*n) return hay;
    }
    return NULL;
}

/* Returns 1 if output looks like model garbage (repeated char, pure whitespace) */
static int is_garbage(const char *s) {
    int len = (int)strlen(s);
    if (len == 0) return 1;
    /* All whitespace? */
    int allws = 1;
    for (int i = 0; s[i]; i++)
        if (!isspace((unsigned char)s[i])) { allws = 0; break; }
    if (allws) return 1;
    /* Run of 6+ identical non-space chars at start = model token-loop */
    if (len >= 6) {
        char c = s[0];
        int run = 1;
        for (int i = 1; i < len && i < 20; i++)
            if (s[i] == c) run++; else break;
        if (run >= 6) return 1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * Test table — 250 entries
 * -------------------------------------------------------------------------- */
static const Test TESTS[] = {

/* ── C ────────────────────────────────────────────────────────────────────── */
{"C", "hello_world",
 "#include <stdio.h>\nint main(void) {\n    printf(\"Hello", 24, "world"},

{"C", "for_loop",
 "#include <stdio.h>\nint main(void) {\n    for (int i = 0; i < 10; i++) {\n        printf(\"%d\\n\", i", 24, ")"},

{"C", "fibonacci",
 "int fib(int n) {\n    if (n <= 1) return n;\n    return", 24, "fib"},

{"C", "linked_list_node",
 "struct Node {\n    int data;\n    struct Node *next;\n};\nstruct Node *make_node(int v) {\n    struct Node *n = malloc(sizeof(*n));\n    n->data = v;\n    n->next =", 20, "null"},

{"C", "malloc_check",
 "int *arr = malloc(n * sizeof(int));\nif (arr == NULL) {\n    perror(\"malloc\");\n    return", 16, "-1"},

{"C", "string_copy",
 "void my_strcpy(char *dst, const char *src) {\n    while (*src) {\n        *dst++ =", 20, "src"},

{"C", "bubble_sort",
 "void bubble_sort(int *a, int n) {\n    for (int i = 0; i < n-1; i++)\n        for (int j = 0; j < n-i-1; j++)\n            if (a[j] > a[j+1]) {\n                int t = a[j]; a[j] = a[j+1]; a[j+1] =", 24, "t"},

{"C", "binary_search",
 "int bsearch_idx(int *a, int n, int key) {\n    int lo = 0, hi = n - 1;\n    while (lo <= hi) {\n        int mid = lo + (hi - lo) / 2;\n        if (a[mid] == key) return mid;\n        else if (a[mid] < key) lo =", 20, "mid"},

{"C", "stack_push",
 "typedef struct { int *data; int top; int cap; } Stack;\nvoid push(Stack *s, int v) {\n    if (s->top == s->cap) return;\n    s->data[s->top++] =", 16, "v"},

{"C", "file_open",
 "FILE *fp = fopen(\"data.txt\", \"r\");\nif (!fp) {\n    perror(\"fopen\");\n    return", 16, "-1"},

{"C", "factorial_recursive",
 "long factorial(int n) {\n    if (n == 0) return 1;\n    return n *", 16, "factorial"},

{"C", "swap_macro",
 "#define SWAP(a, b, T) do { T _tmp = (a); (a) = (b); (b) =", 16, "_tmp"},

{"C", "struct_init",
 "typedef struct { float x; float y; } Vec2;\nVec2 make_vec(float x, float y) {\n    return (Vec2){ .x = x, .y =", 16, "y"},

{"C", "strlen_impl",
 "size_t my_strlen(const char *s) {\n    size_t n = 0;\n    while (*s++) n++;\n    return", 12, "n"},

{"C", "gcd",
 "int gcd(int a, int b) {\n    while (b) { int t = b; b = a % b; a =", 16, "t"},

{"C", "hash_table_probe",
 "unsigned int hash_str(const char *s) {\n    unsigned int h = 5381;\n    while (*s) h = h * 33 ^",  16, "*s"},

{"C", "pthread_create",
 "#include <pthread.h>\nvoid *worker(void *arg) {\n    int id = *(int *)arg;\n    printf(\"Thread %d running\\n\", id);\n    return", 16, "null"},

{"C", "queue_ring",
 "typedef struct { int *buf; int head, tail, cap; } Queue;\nvoid enqueue(Queue *q, int v) {\n    q->buf[q->tail] = v;\n    q->tail = (q->tail + 1) %", 16, "cap"},

{"C", "volatile_memzero",
 "void secure_zero(void *ptr, size_t n) {\n    volatile unsigned char *p = ptr;\n    while (n--) *p++ =", 12, "0"},

/* ── C++ ──────────────────────────────────────────────────────────────────── */
{"C++", "class_constructor",
 "class Animal {\npublic:\n    std::string name;\n    int age;\n    Animal(const std::string &n, int a) : name(n), age(", 16, "a"},

{"C++", "vector_push",
 "#include <vector>\nvoid fill(std::vector<int> &v, int n) {\n    for (int i = 0; i < n; i++) v.push_back(", 16, "i"},

{"C++", "range_for",
 "#include <iostream>\n#include <vector>\nvoid print(const std::vector<int> &v) {\n    for (const auto &x : v) std::cout << x <<", 16, NULL},

{"C++", "lambda_sort",
 "#include <algorithm>\n#include <vector>\nvoid sort_desc(std::vector<int> &v) {\n    std::sort(v.begin(), v.end(), [](int a, int b) { return a >", 16, "b"},

{"C++", "unique_ptr",
 "#include <memory>\nstd::unique_ptr<int> make_int(int v) {\n    return std::make_unique<int>(", 12, "v"},

{"C++", "template_max",
 "template<typename T>\nT my_max(T a, T b) {\n    return a > b ? a :", 12, "b"},

{"C++", "map_insert",
 "#include <map>\nstd::map<std::string,int> freq;\nvoid count(const std::string &w) {\n    freq[w]", 16, "++"},

{"C++", "exception_throw",
 "#include <stdexcept>\nvoid check_age(int age) {\n    if (age < 0) throw std::invalid_argument(", 16, "age"},

{"C++", "copy_constructor",
 "class Buffer {\n    char *data; int size;\npublic:\n    Buffer(const Buffer &other) : size(other.size), data(new char[other.size]) {\n        memcpy(data, other.data,", 20, "size"},

{"C++", "override_method",
 "class Shape {\npublic:\n    virtual double area() const = 0;\n};\nclass Circle : public Shape {\n    double r;\npublic:\n    Circle(double r) : r(r) {}\n    double area() const override { return 3.14159265 *", 16, "r"},

{"C++", "move_semantics",
 "class Buffer {\n    std::vector<int> data;\npublic:\n    Buffer(Buffer &&other) noexcept : data(std::move(other.", 12, "data"},

{"C++", "variadic_fold",
 "template<typename... Args>\nauto sum(Args... args) {\n    return (args + ...);\n}\nauto result = sum(1, 2, 3,", 12, NULL},

{"C++", "shared_ptr_node",
 "#include <memory>\nstruct Node { int val; std::shared_ptr<Node> next; };\nstd::shared_ptr<Node> cons(int v, std::shared_ptr<Node> rest) {\n    return std::make_shared<Node>(Node{v,", 16, "rest"},

{"C++", "string_view_prefix",
 "#include <string_view>\nbool starts_with(std::string_view s, std::string_view pre) {\n    return s.size() >= pre.size() && s.substr(0, pre.size()) ==", 16, "pre"},

{"C++", "structured_binding",
 "#include <map>\nstd::map<std::string,int> scores{{\"Alice\",95},{\"Bob\",87}};\nvoid print_scores() {\n    for (const auto &[name, score] : scores)\n        printf(\"%s: %d\\n\", name.c_str(),", 16, "score"},

/* ── Python ───────────────────────────────────────────────────────────────── */
{"Python", "list_comprehension",
 "squares = [x**2 for x in range(10) if x %", 16, "0"},

{"Python", "class_definition",
 "class Stack:\n    def __init__(self):\n        self.items = []\n    def push(self, item):\n        self.items.append(", 16, "item"},

{"Python", "decorator",
 "import functools, time\ndef timer(func):\n    @functools.wraps(func)\n    def wrapper(*args, **kwargs):\n        start = time.time()\n        result = func(*args, **kwargs)\n        print(f'Elapsed: {time.time()-start:.3f}s')\n        return", 16, "result"},

{"Python", "generator",
 "def fibonacci():\n    a, b = 0, 1\n    while True:\n        yield a\n        a, b = b,", 12, "b"},

{"Python", "context_manager",
 "class ManagedFile:\n    def __init__(self, path): self.path = path\n    def __enter__(self):\n        self.f = open(self.path)\n        return self.f\n    def __exit__(self, *args):\n        self.f.", 16, "close"},

{"Python", "lambda_sort",
 "people = [{'name': 'Alice', 'age': 30}, {'name': 'Bob', 'age': 25}]\npeople.sort(key=lambda p: p[", 12, "age"},

{"Python", "dict_comprehension",
 "words = ['apple', 'banana', 'cherry']\nword_len = {w: len(w) for w in", 12, "words"},

{"Python", "exception_handling",
 "try:\n    result = 10 / int(input('num: '))\nexcept ZeroDivisionError:\n    print('divide by zero')\nexcept ValueError:\n    print(", 16, NULL},

{"Python", "dataclass",
 "from dataclasses import dataclass\n@dataclass\nclass Point:\n    x: float\n    y: float\n    def distance_from_origin(self):\n        return (self.x**2 + self.y**2) **", 16, "0.5"},

{"Python", "asyncio_coroutine",
 "import asyncio\nasync def fetch(url):\n    await asyncio.sleep(0.1)\n    return f'data from {", 16, "url"},

{"Python", "regex_match",
 "import re\nphone_re = re.compile(r'\\d{3}-\\d{4}')\ndef is_phone(s):\n    return bool(phone_re.", 16, "match"},

{"Python", "binary_search_py",
 "def binary_search(arr, target):\n    lo, hi = 0, len(arr) - 1\n    while lo <= hi:\n        mid = (lo + hi) // 2\n        if arr[mid] == target: return mid\n        elif arr[mid] < target: lo =", 16, "mid"},

{"Python", "merge_sort",
 "def merge_sort(arr):\n    if len(arr) <= 1: return arr\n    mid = len(arr) // 2\n    left  = merge_sort(arr[:mid])\n    right = merge_sort(arr[mid:])\n    return", 20, "merge"},

{"Python", "property_decorator",
 "class Temperature:\n    def __init__(self, celsius=0): self._c = celsius\n    @property\n    def fahrenheit(self):\n        return self._c *", 16, "9"},

{"Python", "enumerate_loop",
 "fruits = ['apple', 'banana', 'cherry']\nfor idx, fruit in enumerate(fruits):\n    print(f'{idx}:", 16, "fruit"},

{"Python", "defaultdict_graph",
 "from collections import defaultdict\ngraph = defaultdict(list)\nfor u, v in [(1,2),(1,3),(2,4)]:\n    graph[u].append(v)\n    graph[v].append(", 12, "u"},

{"Python", "namedtuple_point",
 "from collections import namedtuple\nPoint = namedtuple('Point', ['x', 'y'])\ndef distance(p1, p2):\n    return ((p1.x-p2.x)**2 + (p1.y-p2.y)**2) **", 16, "0.5"},

{"Python", "pathlib_glob",
 "from pathlib import Path\nfor csv in Path('data').glob('*.csv'):\n    print(csv.", 12, NULL},

{"Python", "match_statement",
 "def http_status(code):\n    match code:\n        case 200: return 'OK'\n        case 404: return 'Not Found'\n        case 500: return 'Server Error'\n        case _: return", 16, NULL},

{"Python", "slots_class",
 "class Point:\n    __slots__ = ['x', 'y']\n    def __init__(self, x, y):\n        self.x = x; self.y =", 12, "y"},

{"Python", "abstract_base",
 "from abc import ABC, abstractmethod\nclass Shape(ABC):\n    @abstractmethod\n    def area(self) -> float: ...\nclass Square(Shape):\n    def __init__(self, s): self.s = s\n    def area(self): return self.s **", 12, "2"},

{"Python", "walrus_operator",
 "import re\ntext = 'Phone: 555-1234'\nif m := re.search(r'\\d{3}-\\d{4}', text):\n    print('Found:', m.", 12, "group"},

{"Python", "partial_func",
 "from functools import partial\ndef power(base, exp): return base ** exp\nsquare = partial(power, exp=2)\nprint(square(", 12, NULL},

/* ── JavaScript ───────────────────────────────────────────────────────────── */
{"JavaScript", "arrow_function",
 "const add = (a, b) =>", 12, "b"},

{"JavaScript", "promise_chain",
 "fetch('/api/data')\n  .then(res => res.json())\n  .then(data => console.log(data))\n  .catch(err =>", 16, "err"},

{"JavaScript", "async_await",
 "async function getUser(id) {\n  const res = await fetch(`/users/${id}`);\n  if (!res.ok) throw new Error('Not found');\n  return res.", 16, "json"},

{"JavaScript", "class_es6",
 "class EventEmitter {\n  constructor() { this.listeners = {}; }\n  on(event, fn) {\n    if (!this.listeners[event]) this.listeners[event] = [];\n    this.listeners[event].", 16, "push"},

{"JavaScript", "array_map_filter",
 "const nums = [1,2,3,4,5,6];\nconst evenSquares = nums.filter(n => n % 2 === 0).map(n => n *", 12, "n"},

{"JavaScript", "destructuring",
 "const { name, age, address: { city } } = user;\nconsole.log(`${name} is ${age} in ${", 16, "city"},

{"JavaScript", "spread_reduce",
 "function sum(...nums) {\n  return nums.reduce((acc, n) => acc +", 16, "n"},

{"JavaScript", "closure_counter",
 "function makeCounter() {\n  let count = 0;\n  return () => ++", 12, "count"},

{"JavaScript", "prototype_last",
 "Array.prototype.last = function() {\n  return this[this.length -", 12, "1"},

{"JavaScript", "regex_test",
 "const emailRe = /^[\\w._%+\\-]+@[\\w.\\-]+\\.[a-z]{2,}$/i;\nconst isEmail = s => emailRe.", 16, "test"},

{"JavaScript", "generator_range",
 "function* range(start, end, step = 1) {\n  for (let i = start; i < end; i += step) yield", 12, "i"},

{"JavaScript", "debounce_fn",
 "function debounce(fn, ms) {\n  let t;\n  return (...args) => {\n    clearTimeout(t);\n    t = setTimeout(() => fn.apply(this,", 20, "args"},

{"JavaScript", "module_exports",
 "function add(a, b) { return a + b; }\nfunction mul(a, b) { return a * b; }\nmodule.exports = {", 16, "add"},

{"JavaScript", "optional_chaining",
 "const user = { profile: { address: { city: 'NYC' } } };\nconst city = user?.profile?.address?.city ??", 12, NULL},

{"JavaScript", "weakmap_cache",
 "const cache = new WeakMap();\nfunction memo(fn) {\n  return obj => {\n    if (!cache.has(obj)) cache.set(obj, fn(obj));\n    return cache.get(", 12, "obj"},

{"JavaScript", "object_spread",
 "const defaults = { color: 'red', size: 'md' };\nconst config = { ...defaults, color:",  12, NULL},

{"JavaScript", "proxy_trap",
 "const handler = {\n  get(target, prop) {\n    return prop in target ? target[prop] : `${prop} not found`;\n  },\n  set(target, prop, value) {\n    target[prop] =", 16, "value"},

{"JavaScript", "symbol_key",
 "const ID = Symbol('id');\nconst user = { name: 'Alice', [ID]: 42 };\nconsole.log(user[", 12, "ID"},

/* ── TypeScript ───────────────────────────────────────────────────────────── */
{"TypeScript", "interface_def",
 "interface User {\n  id: number;\n  name: string;\n  email?: string;\n  createdAt:", 16, "Date"},

{"TypeScript", "generic_first",
 "function first<T>(arr: T[]): T | undefined {\n  return arr.length > 0 ? arr[", 12, "0"},

{"TypeScript", "enum_direction",
 "enum Direction { Up = 'UP', Down = 'DOWN', Left = 'LEFT', Right =", 12, "RIGHT"},

{"TypeScript", "type_guard",
 "function isString(value: unknown): value is string {\n  return typeof value ===", 12, "string"},

{"TypeScript", "mapped_type",
 "type Readonly<T> = {\n  readonly [P in keyof T]:", 12, "T"},

{"TypeScript", "async_ts",
 "async function fetchUsers(): Promise<User[]> {\n  const res = await fetch('/api/users');\n  return res.", 16, "json"},

{"TypeScript", "discriminated_union",
 "type Shape = { kind: 'circle'; r: number } | { kind: 'rect'; w: number; h: number };\nfunction area(s: Shape): number {\n  if (s.kind === 'circle') return Math.PI * s.r ** 2;\n  return s.w *", 16, "h"},

{"TypeScript", "conditional_type",
 "type NonNullable<T> = T extends null | undefined ? never :", 12, "T"},

{"TypeScript", "infer_return",
 "type ReturnType<T> = T extends (...args: any[]) => infer R ? R :", 12, "R"},

{"TypeScript", "template_literal",
 "type EventName = 'click' | 'focus' | 'blur';\ntype OnEvent = `on${Capitalize<EventName>}`;\n// 'onClick' | 'onFocus' | 'on", 12, "Blur"},

{"TypeScript", "satisfies_op",
 "type Colors = 'red' | 'green' | 'blue';\nconst palette = { red: [255,0,0] } satisfies Record<Colors,", 12, NULL},

/* ── Java ─────────────────────────────────────────────────────────────────── */
{"Java", "main_method",
 "public class Hello {\n    public static void main(String[] args) {\n        System.out.println(\"Hello,", 16, "World"},

{"Java", "arraylist_loop",
 "List<String> names = new ArrayList<>();\nnames.add(\"Alice\"); names.add(\"Bob\");\nfor (String name : names) {\n    System.out.println(", 16, "name"},

{"Java", "interface_impl",
 "interface Drawable { void draw(); }\nclass Circle implements Drawable {\n    double r;\n    public void draw() {\n        System.out.println(\"Circle r=\" +", 16, "r"},

{"Java", "lambda_stream",
 "List<Integer> evens = IntStream.rangeClosed(1,10)\n    .filter(n -> n % 2 == 0)\n    .boxed()\n    .", 20, "collect"},

{"Java", "optional_java",
 "Optional<String> findName(int id) {\n    if (id == 1) return Optional.of(\"Alice\");\n    return Optional.", 12, "empty"},

{"Java", "try_resources",
 "try (BufferedReader br = new BufferedReader(new FileReader(\"data.txt\"))) {\n    String line;\n    while ((line = br.readLine()) != null) System.out.println(", 16, "line"},

{"Java", "generic_pair",
 "public class Pair<A, B> {\n    final A first; final B second;\n    Pair(A f, B s) { first = f; second =", 16, "s"},

{"Java", "enum_java",
 "enum Planet {\n    MERCURY(3.303e+23, 2.4397e6),\n    EARTH  (5.976e+24, 6.37814e6);\n    final double mass;\n    Planet(double mass, double r) { this.mass =", 16, "mass"},

{"Java", "record_java",
 "public record Point(double x, double y) {\n    double distanceTo(Point o) {\n        double dx = x - o.x(), dy = y - o.y();\n        return Math.sqrt(dx*dx +", 16, "dy"},

{"Java", "text_block",
 "String json = \"\"\"\n        {\n          \"name\": \"Alice\",\n          \"age\":", 12, "30"},

{"Java", "switch_expr",
 "String season = switch (month) {\n    case 12, 1, 2 -> \"Winter\";\n    case 3, 4, 5 -> \"Spring\";\n    case 6, 7, 8 -> \"Summer\";\n    default ->", 12, "Fall"},

/* ── Go ───────────────────────────────────────────────────────────────────── */
{"Go", "hello_world",
 "package main\nimport \"fmt\"\nfunc main() {\n    fmt.Println(\"Hello,", 12, "World"},

{"Go", "goroutine_channel",
 "func producer(ch chan<- int) {\n    for i := 0; i < 5; i++ {\n        ch <-", 12, "i"},

{"Go", "struct_method",
 "type Rectangle struct{ Width, Height float64 }\nfunc (r Rectangle) Area() float64 {\n    return r.Width *", 12, "Height"},

{"Go", "error_handling",
 "func divide(a, b float64) (float64, error) {\n    if b == 0 { return 0, fmt.Errorf(\"divide by zero\") }\n    return a /", 12, "b"},

{"Go", "interface_go",
 "type Stringer interface{ String() string }\ntype Point struct{ X, Y int }\nfunc (p Point) String() string {\n    return fmt.Sprintf(\"(%d,%d)\",", 16, "X"},

{"Go", "defer_close",
 "func withFile(path string) error {\n    f, err := os.Open(path)\n    if err != nil { return err }\n    defer f.", 12, "Close"},

{"Go", "slice_append",
 "func doubled(s []int) []int {\n    out := make([]int, 0, len(s))\n    for _, v := range s { out = append(out, v*", 12, "2"},

{"Go", "map_word_count",
 "wc := make(map[string]int)\nfor _, w := range strings.Fields(text) {\n    wc[w]", 16, "++"},

{"Go", "select_timeout",
 "select {\ncase msg := <-ch:\n    fmt.Println(msg)\ncase <-time.After(time.Second):\n    fmt.Println(", 16, NULL},

{"Go", "sync_mutex",
 "type SafeMap struct{ mu sync.Mutex; m map[string]int }\nfunc (s *SafeMap) Inc(key string) {\n    s.mu.Lock()\n    s.m[key]++\n    s.mu.", 12, "Unlock"},

{"Go", "generics_go",
 "func Map[T, U any](s []T, f func(T) U) []U {\n    out := make([]U, len(s))\n    for i, v := range s { out[i] = f(", 16, "v"},

{"Go", "json_marshal",
 "type User struct {\n    Name  string `json:\"name\"`\n    Email string `json:\"email\"`\n}\nu := User{Name:\"Alice\",Email:\"a@b.com\"}\nbytes, _ := json.Marshal(", 12, "u"},

{"Go", "http_handler",
 "func health(w http.ResponseWriter, r *http.Request) {\n    w.WriteHeader(http.StatusOK)\n    w.Write([]byte(", 12, "ok"},

/* ── Rust ─────────────────────────────────────────────────────────────────── */
{"Rust", "hello_world",
 "fn main() {\n    println!(\"Hello,", 12, "world"},

{"Rust", "match_expression",
 "fn describe(n: i32) -> &'static str {\n    match n {\n        0 => \"zero\",\n        1..=9 => \"single digit\",\n        _ =>", 16, "digit"},

{"Rust", "option_match",
 "fn first_even(v: &[i32]) -> Option<i32> {\n    v.iter().find(|&&x| x % 2 == 0).copied()\n}\nfn main() {\n    match first_even(&[1,3,4]) {\n        Some(n) => println!(\"{}\", n),\n        None =>", 16, "println"},

{"Rust", "struct_impl",
 "struct Counter { count: u32, max: u32 }\nimpl Counter {\n    fn new(max: u32) -> Self { Counter { count: 0, max } }\n    fn tick(&mut self) {\n        if self.count < self.max { self.count +=", 12, "1"},

{"Rust", "trait_area",
 "trait Area { fn area(&self) -> f64; }\nstruct Circle { r: f64 }\nimpl Area for Circle {\n    fn area(&self) -> f64 { std::f64::consts::PI * self.r *", 12, "r"},

{"Rust", "vec_iter_chain",
 "let a = vec![1,2,3]; let b = vec![4,5,6];\nlet doubled: Vec<i32> = a.iter().chain(b.iter()).map(|&x| x *", 16, "2"},

{"Rust", "result_qmark",
 "fn read_num(path: &str) -> Result<i64, Box<dyn std::error::Error>> {\n    let s = std::fs::read_to_string(path)?;\n    Ok(s.trim().parse()", 16, "parse"},

{"Rust", "enum_message",
 "enum Msg { Quit, Move{x:i32,y:i32}, Write(String) }\nfn process(m: Msg) {\n    match m {\n        Msg::Quit => println!(\"quit\"),\n        Msg::Move{x,y} => println!(\"{},{}\",x,", 16, "y"},

{"Rust", "impl_trait_fn",
 "fn make_adder(x: i32) -> impl Fn(i32) -> i32 {\n    move |y| x +", 12, "y"},

{"Rust", "box_dyn_trait",
 "trait Sound { fn sound(&self) -> &str; }\nstruct Dog;\nimpl Sound for Dog { fn sound(&self) -> &str { \"woof\" } }\nstruct Cat;\nimpl Sound for Cat { fn sound(&self) -> &str { \"meow\" } }\nfn animal(name: &str) -> Box<dyn Sound> {\n    match name {\n        \"dog\" => Box::new(Dog),\n        _ => Box::new(", 20, "Cat"},

{"Rust", "from_impl",
 "struct Celsius(f64);\nstruct Fahrenheit(f64);\nimpl From<Celsius> for Fahrenheit {\n    fn from(c: Celsius) -> Self { Fahrenheit(c.0 * 9.0/5.0 +", 12, "32"},

{"Rust", "lifetime_fn",
 "fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {\n    if x.len() > y.len() { x } else {", 12, "y"},

{"Rust", "ownership_move",
 "fn consume(s: String) { println!(\"{}\", s); }\nfn main() {\n    let s = String::from(\"hello\");\n    consume(s);\n    // s is moved; cannot use s after this\n    // println!(\"{}\"", 16, NULL},

/* ── SQL ──────────────────────────────────────────────────────────────────── */
{"SQL", "select_where",
 "SELECT id, name, email FROM users WHERE age > 18 AND status =", 16, "active"},

{"SQL", "join_query",
 "SELECT o.id, u.name, o.total\nFROM orders o JOIN users u ON o.user_id = u.id\nWHERE o.created_at >=", 16, NULL},

{"SQL", "group_by",
 "SELECT dept, COUNT(*) AS n, AVG(salary) AS avg\nFROM employees\nGROUP BY dept\nHAVING COUNT(*) >", 16, NULL},

{"SQL", "create_table",
 "CREATE TABLE products (\n    id SERIAL PRIMARY KEY,\n    name VARCHAR(255) NOT NULL,\n    price DECIMAL(10,2) NOT NULL,\n    stock INTEGER DEFAULT 0,\n    created_at TIMESTAMP DEFAULT", 20, "now"},

{"SQL", "subquery",
 "SELECT name FROM employees\nWHERE salary > (SELECT AVG(salary) FROM", 16, "employees"},

{"SQL", "window_rank",
 "SELECT name, salary,\n    RANK() OVER (PARTITION BY dept ORDER BY salary DESC) AS rnk\nFROM", 16, "employees"},

{"SQL", "insert_statement",
 "INSERT INTO orders (user_id, product_id, qty, total)\nVALUES (42, 7, 3,", 16, NULL},

{"SQL", "update_statement",
 "UPDATE products\nSET price = price * 1.1, updated_at = NOW()\nWHERE category =", 16, NULL},

{"SQL", "create_index",
 "CREATE INDEX CONCURRENTLY idx_orders_user\nON orders (user_id, created_at DESC)\nWHERE status =", 12, NULL},

{"SQL", "upsert_conflict",
 "INSERT INTO user_stats (user_id, logins, last_seen)\nVALUES ($1, 1, NOW())\nON CONFLICT (user_id) DO UPDATE\n  SET logins = user_stats.logins + 1,\n      last_seen =", 16, "now"},

{"SQL", "cte_recursive",
 "WITH RECURSIVE fib(n, a, b) AS (\n    SELECT 1, 0, 1\n    UNION ALL\n    SELECT n+1, b, a+b FROM fib WHERE n <", 16, NULL},

/* ── Shell/Bash ───────────────────────────────────────────────────────────── */
{"Bash", "for_loop",
 "#!/bin/bash\nfor f in *.txt; do\n    echo \"Processing $f\"\n    wc -l", 16, "$f"},

{"Bash", "function_def",
 "#!/bin/bash\ngreet() {\n    local name=\"$1\"\n    echo \"Hello,", 12, "name"},

{"Bash", "if_condition",
 "#!/bin/bash\nif [ -f \"$1\" ]; then\n    echo \"File exists\"\nelse\n    echo", 12, NULL},

{"Bash", "while_read",
 "#!/bin/bash\nwhile IFS= read -r line; do\n    echo \"Line: $line\"\ndone <", 12, NULL},

{"Bash", "array_bash",
 "#!/bin/bash\nfruits=(apple banana cherry)\nfor f in \"${fruits[@]}\"; do\n    echo", 12, "$f"},

{"Bash", "pipe_chain",
 "cat access.log | grep '404' | awk '{print $7}' | sort |", 16, "uniq"},

{"Bash", "trap_exit",
 "#!/bin/bash\ncleanup() { rm -f /tmp/app.lock; }\ntrap cleanup", 12, "EXIT"},

{"Bash", "getopts_loop",
 "#!/bin/bash\nwhile getopts 'v:o:h' opt; do\n  case $opt in\n    v) VERBOSE=$OPTARG ;;\n    o) OUT=$OPTARG ;;\n    h) echo 'Usage: script [-v n] [-o file]'; exit 0 ;;\n    *) exit 1 ;;\n  esac\ndone\necho \"Output: $", 12, "OUT"},

{"Bash", "heredoc",
 "#!/bin/bash\ncat > config.json <<'EOF'\n{\n  \"host\": \"localhost\",\n  \"port\":", 12, NULL},

{"Bash", "assoc_array",
 "#!/bin/bash\ndeclare -A colors\ncolors[red]='#FF0000'\ncolors[green]='#00FF00'\nfor k in \"${!colors[@]}\"; do echo \"$k =", 12, "colors"},

/* ── HTML/CSS ─────────────────────────────────────────────────────────────── */
{"HTML", "form_element",
 "<form action=\"/submit\" method=\"post\">\n  <label for=\"email\">Email:</label>\n  <input type=\"email\" id=\"email\" name=\"email\" required>\n  <button type=", 16, "submit"},

{"HTML", "nav_element",
 "<nav>\n  <ul>\n    <li><a href=\"/\">Home</a></li>\n    <li><a href=\"/about\">About</a></li>\n    <li><a href=", 16, NULL},

{"HTML", "table_element",
 "<table>\n  <thead><tr><th>Name</th><th>Age</th></tr></thead>\n  <tbody>\n    <tr><td>Alice</td><td>", 16, NULL},

{"CSS", "flexbox",
 ".container {\n  display: flex;\n  justify-content: space-between;\n  align-items:", 12, "center"},

{"CSS", "media_query",
 "@media (max-width: 768px) {\n  .sidebar {\n    display:", 12, "none"},

{"CSS", "animation",
 "@keyframes fadeIn {\n  from { opacity: 0; }\n  to   { opacity:", 12, "1"},

/* ── Ruby ─────────────────────────────────────────────────────────────────── */
{"Ruby", "class_def",
 "class BankAccount\n  attr_reader :balance\n  def initialize(balance = 0)\n    @balance =", 16, "balance"},

{"Ruby", "block_yield",
 "def repeat(n)\n  n.times { yield }\nend\nrepeat(3) { puts", 12, NULL},

{"Ruby", "hash_select",
 "ages = { alice: 30, bob: 25, carol: 35 }\nadults = ages.select { |_name, age| age >=", 16, "18"},

{"Ruby", "module_mixin",
 "module Greetable\n  def greet = \"Hello, I am #{name}\"\nend\nclass Person\n  include Greetable\n  attr_reader :name\n  def initialize(n) @name =", 16, "n"},

{"Ruby", "rescue_block",
 "def divide(a, b)\n  raise ArgumentError, 'zero' if b == 0\n  a /", 12, "b"},

/* ── PHP ──────────────────────────────────────────────────────────────────── */
{"PHP", "class_php",
 "<?php\nclass Product {\n    public function __construct(\n        private string $name,\n        private float $price\n    ) {}\n    public function getPrice(): float { return $this->", 16, "price"},

{"PHP", "array_functions",
 "<?php\n$numbers = [3,1,4,1,5,9,2,6];\n$evens = array_filter($numbers, fn($n) => $n % 2 === 0);\nsort(", 12, "evens"},

{"PHP", "pdo_query",
 "<?php\n$pdo = new PDO('mysql:host=localhost;dbname=mydb', $user, $pass);\n$stmt = $pdo->prepare('SELECT * FROM users WHERE id = ?');\n$stmt->execute([$id]);\n$user = $stmt->", 16, "fetch"},

{"PHP", "trait_php",
 "<?php\ntrait Timestamps {\n    private DateTime $updatedAt;\n    public function touch(): void {\n        $this->updatedAt = new DateTime(", 12, NULL},

/* ── Swift ────────────────────────────────────────────────────────────────── */
{"Swift", "struct_distance",
 "struct Point {\n    var x, y: Double\n    func distance(to o: Point) -> Double {\n        let dx = x-o.x, dy = y-o.y\n        return sqrt(dx*dx +", 12, "dy"},

{"Swift", "optional_binding",
 "let map = [1: \"Alice\", 2: \"Bob\"]\nif let name = map[1] {\n    print(\"Found:", 16, "name"},

{"Swift", "closure_sort",
 "let nums = [5,2,8,1,9,3]\nlet sorted = nums.sorted { $0 <", 12, "$1"},

{"Swift", "protocol_desc",
 "protocol Describable { var description: String { get } }\nstruct Car: Describable {\n    let make, model: String\n    var description: String { \"\\(make) \\(model)\"", 16, "model"},

/* ── Kotlin ───────────────────────────────────────────────────────────────── */
{"Kotlin", "data_class",
 "data class User(val id: Int, val name: String, val email: String)\nfun main() {\n    val u = User(1, \"Alice\", \"a@b.com\")\n    println(u.", 12, "name"},

{"Kotlin", "extension_palindrome",
 "fun String.isPalindrome(): Boolean {\n    val s = lowercase().filter { it.isLetterOrDigit() }\n    return s == s.", 16, "reversed"},

{"Kotlin", "coroutine_delay",
 "import kotlinx.coroutines.*\nsuspend fun fetchData(): String { delay(100); return \"ok\" }\nfun main() = runBlocking {\n    println(fetchData()", 12, NULL},

{"Kotlin", "when_grade",
 "fun grade(score: Int) = when {\n    score >= 90 -> \"A\"\n    score >= 80 -> \"B\"\n    score >= 70 -> \"C\"\n    else ->", 12, "D"},

{"Kotlin", "sealed_result",
 "sealed class Result<out T>\ndata class Ok<T>(val v: T) : Result<T>()\ndata class Err(val msg: String) : Result<Nothing>()\nfun handle(r: Result<String>) = when (r) {\n    is Ok -> println(r.v)\n    is Err ->", 16, "println"},

/* ── R ────────────────────────────────────────────────────────────────────── */
{"R", "vector_stats",
 "x <- c(1,2,3,4,5)\ncat(\"Mean:\", mean(x), \"SD:\",", 16, "sd"},

{"R", "dataframe_col",
 "df <- data.frame(name=c('Alice','Bob'), score=c(95,87))\ndf$grade <- ifelse(df$score >= 90, 'A',", 16, "B"},

{"R", "ggplot_scatter",
 "library(ggplot2)\nggplot(mtcars, aes(x=wt, y=mpg)) +\n  geom_point() +\n  labs(title='MPG vs Weight', x='Weight', y=", 16, "MPG"},

{"R", "apply_rows",
 "mat <- matrix(1:12, nrow=3)\nrow_sums <- apply(mat, 1,", 12, "sum"},

/* ── Julia ────────────────────────────────────────────────────────────────── */
{"Julia", "newton_sqrt",
 "function newton_sqrt(n::Float64, tol=1e-10)\n    x = n / 2.0\n    while abs(x^2 - n) > tol\n        x = (x + n/x) /", 12, "2"},

{"Julia", "comprehension",
 "primes = [n for n in 2:50 if all(n % d != 0 for d in 2:isqrt(", 16, "n"},

/* ── Haskell ──────────────────────────────────────────────────────────────── */
{"Haskell", "list_length",
 "myLen :: [a] -> Int\nmyLen [] = 0\nmyLen (_:xs) = 1 +", 12, "myLen"},

{"Haskell", "safe_divide",
 "safeDivide :: Int -> Int -> Maybe Int\nsafeDivide _ 0 = Nothing\nsafeDivide a b = Just (a `div`", 12, "b"},

{"Haskell", "apply_twice",
 "applyTwice :: (a -> a) -> a -> a\napplyTwice f x = f (f", 12, "x"},

/* ── Lua ──────────────────────────────────────────────────────────────────── */
{"Lua", "table_sum",
 "local t = {1,2,3,4,5}\nlocal sum = 0\nfor _, v in ipairs(t) do sum = sum +", 12, "v"},

{"Lua", "metatables",
 "Vector = {}\nVector.__index = Vector\nfunction Vector.new(x, y)\n    return setmetatable({x=x,y=y}, Vector)\nend\nfunction Vector:magnitude()\n    return math.sqrt(self.x^2 +", 12, "self.y"},

/* ── YAML/JSON ────────────────────────────────────────────────────────────── */
{"YAML", "kubernetes_pod",
 "apiVersion: v1\nkind: Pod\nmetadata:\n  name: my-app\nspec:\n  containers:\n  - name: app\n    image: nginx:1.21\n    ports:\n    - containerPort:", 16, "80"},

{"JSON", "package_json",
 "{\n  \"name\": \"my-app\",\n  \"version\": \"1.0.0\",\n  \"scripts\": {\n    \"start\": \"node index.js\",\n    \"test\":", 16, NULL},

/* ── Regex ────────────────────────────────────────────────────────────────── */
{"Regex", "email_pattern",
 "import re\nemail_re = re.compile(r'^[a-zA-Z0-9._%+\\-]+@[a-zA-Z0-9.\\-]+\\.", 16, NULL},

{"Regex", "url_pattern",
 "import re\nurl_re = re.compile(\n    r'https?://[a-zA-Z0-9\\-]+(?:\\.[a-zA-Z0-9\\-]+)+(?:/", 16, NULL},

/* ── Algorithms ───────────────────────────────────────────────────────────── */
{"Algorithm", "quicksort_c",
 "void qsort_int(int *a, int lo, int hi) {\n    if (lo >= hi) return;\n    int p = a[hi], i = lo-1;\n    for (int j=lo; j<hi; j++) if (a[j]<=p){i++;int t=a[i];a[i]=a[j];a[j]=t;}\n    int t=a[i+1];a[i+1]=a[hi];a[hi]=t;\n    int m=i+1;\n    qsort_int(a,lo,m-1);\n    qsort_int(a,", 16, "m"},

{"Algorithm", "dijkstra_py",
 "import heapq\ndef dijkstra(g, src):\n    dist = {n: float('inf') for n in g}; dist[src]=0\n    pq = [(0,src)]\n    while pq:\n        d,u = heapq.heappop(pq)\n        if d>dist[u]: continue\n        for v,w in g[u]:\n            if dist[u]+w < dist[v]:\n                dist[v]=dist[u]+w\n                heapq.heappush(pq,", 16, "dist"},

{"Algorithm", "lru_cache",
 "from collections import OrderedDict\nclass LRUCache:\n    def __init__(self, cap): self.cap=cap; self.cache=OrderedDict()\n    def get(self, k):\n        if k not in self.cache: return -1\n        self.cache.move_to_end(k); return self.cache[k]\n    def put(self, k, v):\n        if k in self.cache: self.cache.move_to_end(k)\n        self.cache[k]=v\n        if len(self.cache)>self.cap: self.cache.popitem(", 16, "False"},

/* ── C# ───────────────────────────────────────────────────────────────────── */
{"C#", "linq_query",
 "var nums = new[]{1,2,3,4,5,6,7,8,9,10};\nvar evenSquares = nums.Where(n => n%2==0).Select(n => n *", 16, "n"},

{"C#", "async_http",
 "public async Task<string> FetchAsync(string url) {\n    using var client = new HttpClient();\n    var resp = await client.GetAsync(url);\n    return await resp.Content.", 16, "string"},

{"C#", "extension_method",
 "public static class StringExt {\n    public static bool IsPalindrome(this string s) {\n        var c = s.ToLower().Where(char.IsLetterOrDigit).ToArray();\n        return c.SequenceEqual(c.", 16, "Reverse"},

{"C#", "property_validate",
 "public class Circle {\n    private double _r;\n    public double Radius {\n        get => _r;\n        set {\n            if (value < 0) throw new ArgumentException(\"negative\");\n            _r =", 16, "_r"},

{"C#", "event_handler",
 "public class Button {\n    public event EventHandler? Clicked;\n    public void Click() {\n        Clicked?.Invoke(this,", 12, "EventArgs"},

{"C#", "record_with",
 "public record Person(string Name, int Age);\nvar p = new Person(\"Alice\", 30);\nvar older = p with { Age =", 12, NULL},

{"C#", "switch_expr",
 "string desc = obj switch {\n    int n when n < 0 => \"negative\",\n    int n when n == 0 => \"zero\",\n    int => \"positive\",\n    string s => $\"string:{s}\",\n    _ =>", 16, NULL},

{"C#", "interface_generic",
 "public interface IRepo<T> {\n    Task<T?> GetAsync(int id);\n    Task<IEnumerable<T>> AllAsync();\n    Task AddAsync(T item);\n    Task DeleteAsync(", 12, "int"},

/* ── Scala ────────────────────────────────────────────────────────────────── */
{"Scala", "case_class_sort",
 "case class Person(name: String, age: Int)\nval people = List(Person(\"Alice\",30), Person(\"Bob\",25))\nval sorted = people.sortBy(_.", 12, "age"},

{"Scala", "pattern_match",
 "def describe(x: Any): String = x match {\n  case 0 => \"zero\"\n  case n: Int if n > 0 => s\"positive $n\"\n  case s: String => s\"str: $s\"\n  case _ =>", 16, "unknown"},

{"Scala", "for_comprehension",
 "val pairs = for {\n  x <- 1 to 3\n  y <- 1 to 3\n  if x != y\n} yield (x,", 12, "y"},

{"Scala", "option_map",
 "def toInt(s: String): Option[Int] = scala.util.Try(s.toInt).toOption\nval result = toInt(\"42\").map(_ *", 12, "2"},

{"Scala", "future_map",
 "import scala.concurrent.Future\nimport scala.concurrent.ExecutionContext.Implicits.global\nval f: Future[Int] = Future(42)\nf.map(n => println(\"Got:\",", 12, NULL},

/* ── Dart ─────────────────────────────────────────────────────────────────── */
{"Dart", "class_dart",
 "class Animal {\n  final String name;\n  Animal(this.name);\n  void speak() => print('$name says", 12, "name"},

{"Dart", "async_dart",
 "Future<String> fetchUser(int id) async {\n  final res = await http.get(Uri.parse('/users/$id'));\n  return res.", 12, "body"},

{"Dart", "list_where_map",
 "final nums = [1,2,3,4,5];\nfinal evens = nums.where((n) => n.isEven).map((n) => n *", 12, "2"},

{"Dart", "null_safety",
 "String? findName(Map<int,String> m, int id) => m[id];\nvoid main() {\n  final name = findName({1:'Alice'}, 1);\n  print(name ?.", 12, "length"},

/* ── Elixir ───────────────────────────────────────────────────────────────── */
{"Elixir", "module_def",
 "defmodule Math do\n  def add(a, b), do: a + b\n  def multiply(a, b), do: a *", 12, "b"},

{"Elixir", "pipe_operator",
 "result = 1..5\n  |> Enum.filter(&rem(&1, 2) == 0)\n  |> Enum.map(&(&1 *", 12, "2"},

{"Elixir", "pattern_match",
 "defmodule Shape do\n  def area({:circle, r}), do: :math.pi() * r * r\n  def area({:rect, w, h}), do: w *", 12, "h"},

{"Elixir", "genserver_init",
 "defmodule Counter do\n  use GenServer\n  def start_link(n), do: GenServer.start_link(__MODULE__, n)\n  def init(state), do: {:ok,", 12, "state"},

{"Elixir", "list_comprehension",
 "result = for x <- 1..4, y <- 1..4, x < y, do: {x,", 12, "y"},

/* ── Clojure ──────────────────────────────────────────────────────────────── */
{"Clojure", "factorial",
 "(defn factorial [n]\n  (if (<= n 1) 1\n    (* n (factorial (dec", 12, "n"},

{"Clojure", "let_binding",
 "(let [x 10\n      y 20\n      s (+ x y)]\n  (println \"Sum:\"", 12, "s"},

{"Clojure", "thread_last",
 "(def nums [1 2 3 4 5 6])\n(->> nums\n     (filter even?)\n     (map #(* % %)))\n; result:", 12, NULL},

/* ── OCaml ────────────────────────────────────────────────────────────────── */
{"OCaml", "factorial",
 "let rec fact n =\n  if n <= 1 then 1\n  else n *", 12, "fact"},

{"OCaml", "pattern_shapes",
 "type shape = Circle of float | Rect of float * float\nlet area = function\n  | Circle r -> Float.pi *. r *. r\n  | Rect (w, h) -> w *.", 12, "h"},

{"OCaml", "list_map",
 "let rec map f = function\n  | [] -> []\n  | x :: xs -> f x ::", 12, "map"},

/* ── Zig ──────────────────────────────────────────────────────────────────── */
{"Zig", "hello_world",
 "const std = @import(\"std\");\npub fn main() void {\n    std.debug.print(\"Hello,", 12, "world"},

{"Zig", "error_union",
 "fn divide(a: f64, b: f64) !f64 {\n    if (b == 0) return error.DivByZero;\n    return a /", 12, "b"},

{"Zig", "struct_method",
 "const Point = struct {\n    x: f32, y: f32,\n    pub fn len(self: Point) f32 {\n        return @sqrt(self.x * self.x +", 12, "self.y"},

/* ── Terraform ────────────────────────────────────────────────────────────── */
{"Terraform", "aws_instance",
 "resource \"aws_instance\" \"web\" {\n  ami           = \"ami-0c55b159cbfafe1f0\"\n  instance_type = \"t2.micro\"\n  tags = { Name =", 12, NULL},

{"Terraform", "variable_block",
 "variable \"env\" {\n  type        = string\n  description = \"Deployment environment\"\n  default     =", 12, NULL},

{"Terraform", "output_block",
 "output \"instance_ip\" {\n  value       = aws_instance.web.public_ip\n  description = \"Public IP of web", 12, NULL},

/* ── Assembly (NASM x86-64) ───────────────────────────────────────────────── */
{"Assembly", "hello_nasm",
 "section .data\n    msg db 'Hello, World!', 0xa\n    len equ $ - msg\nsection .text\nglobal _start\n_start:\n    mov rax, 1\n    mov rdi, 1\n    mov rsi, msg\n    mov rdx,", 12, "len"},

{"Assembly", "countdown_loop",
 "section .text\nglobal _start\n_start:\n    mov ecx, 10\n.loop:\n    dec ecx\n    jnz", 12, ".loop"},

{"Assembly", "add_func",
 "; int add(int a, int b) -> rax\nadd:\n    push rbp\n    mov rbp, rsp\n    mov eax, edi\n    add eax,", 12, "esi"},

/* ── PowerShell ───────────────────────────────────────────────────────────── */
{"PowerShell", "function_ps",
 "function Get-FileSize {\n    param([string]$Path)\n    $bytes = (Get-Item $Path).Length\n    [math]::Round($bytes / 1MB,", 12, "2"},

{"PowerShell", "foreach_service",
 "$running = Get-Service | Where-Object { $_.Status -eq 'Running' }\nforeach ($svc in $running) {\n    Write-Host $svc.", 12, "Name"},

{"PowerShell", "hashtable_ps",
 "$cfg = @{\n    Server   = 'localhost'\n    Port     = 5432\n    Database =", 12, NULL},

/* ── Makefile ─────────────────────────────────────────────────────────────── */
{"Makefile", "compile_rule",
 "CC = gcc\nCFLAGS = -Wall -O2\nOBJS = main.o utils.o\n\napp: $(OBJS)\n\t$(CC) $(CFLAGS) -o $@", 12, "^"},

{"Makefile", "phony_clean",
 ".PHONY: clean test\nclean:\n\trm -f $(OBJS) app\n\t@echo", 12, NULL},

/* ── More algorithms & misc ───────────────────────────────────────────────── */
{"C", "bitwise_pow2",
 "/* returns 1 if n is a power of 2 */\nint is_pow2(unsigned n) {\n    return n > 0 && (n &", 12, "n-1"},

{"C", "variadic_sum",
 "#include <stdarg.h>\nint sum_va(int count, ...) {\n    va_list ap; va_start(ap, count);\n    int total = 0;\n    for (int i = 0; i < count; i++) total += va_arg(ap,", 16, "int"},

{"Python", "type_hints_dict",
 "from typing import Dict, List\ndef group_by_first(words: List[str]) -> Dict[str, List[str]]:\n    out: Dict[str, List[str]] = {}\n    for w in words:\n        k = w[0].upper()\n        out.setdefault(k, []).append(", 16, "w"},

{"Rust", "vec_filter_sum",
 "let nums = vec![1,2,3,4,5,6,7,8];\nlet s: i32 = nums.iter().fold(0, |acc,&x| if x%2==0 { acc+x } else {", 16, "acc"},

{"Go", "context_cancel",
 "ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)\ndefer cancel()\nselect {\ncase res := <-doWork(ctx):\n    fmt.Println(res)\ncase <-ctx.Done():\n    fmt.Println(\"timeout:\",", 16, "ctx"},

{"Python", "singleton_meta",
 "class Singleton(type):\n    _inst = {}\n    def __call__(cls, *a, **kw):\n        if cls not in cls._inst:\n            cls._inst[cls] = super().__call__(*a, **kw)\n        return cls._inst[", 12, "cls"},

{"C++", "raii_file",
 "class FileGuard {\n    FILE *fp;\npublic:\n    FileGuard(const char *p, const char *m) : fp(fopen(p,m)) {}\n    ~FileGuard() { if (fp) fclose(", 12, "fp"},

{"Python", "zip_dict",
 "keys = ['a', 'b', 'c']\nvals = [1, 2, 3]\nd = dict(zip(keys,", 12, "vals"},

{"Python", "counter_most",
 "from collections import Counter\nwords = 'the cat sat on the mat'.split()\ntop = Counter(words).most_common(", 12, NULL},

{"JavaScript", "map_set",
 "const freq = new Map();\nfor (const ch of 'hello') {\n  freq.set(ch, (freq.get(ch) || 0) +", 12, "1"},

{"JavaScript", "array_flat_reduce",
 "const nested = [[1,2],[3,4],[5,6]];\nconst flat = nested.flat();\nconsole.log(flat.reduce((a,b) => a +", 12, "b"},

{"TypeScript", "partial_readonly",
 "type Config = { host: string; port: number; debug: boolean };\nconst defaults: Readonly<Partial<Config>> = {\n  host: 'localhost',\n  port:", 12, NULL},

{"Rust", "string_parse",
 "fn double_str(s: &str) -> Result<i32, std::num::ParseIntError> {\n    let n: i32 = s.trim().parse()?;\n    Ok(n *", 12, "2"},

{"Go", "string_builder",
 "import \"strings\"\nvar sb strings.Builder\nfor i, word := range words {\n    if i > 0 { sb.WriteString(\" \") }\n    sb.WriteString(", 12, "word"},

{"Java", "builder_pattern",
 "public class Builder {\n    private String name;\n    private int age;\n    public Builder name(String n) { this.name = n; return", 16, "this"},

{"SQL", "delete_old",
 "DELETE FROM sessions\nWHERE expires_at < NOW()\n  AND user_id IN (SELECT id FROM users WHERE active =", 16, NULL},

{"Bash", "check_command",
 "#!/bin/bash\nif ! command -v docker &>/dev/null; then\n  echo 'docker not found' >&2\n  exit", 12, "1"},

{"Ruby", "map_sum_squares",
 "nums = [1, 2, 3, 4, 5]\nsum_sq = nums.map { |n| n**2 }.reduce(:+)\nputs sum_sq\nevens = nums.select(&:", 12, "even"},

{"Kotlin", "list_filter_map",
 "val nums = listOf(1,2,3,4,5,6)\nval result = nums.filter { it % 2 == 0 }.map { it *", 12, "2"},

{"Scala", "implicit_class",
 "implicit class RichInt(val n: Int) extends AnyVal {\n  def factorial: Int = if (n <= 1) 1 else n *", 12, "factorial"},

{"Elixir", "map_update",
 "map = %{name: \"Alice\", age: 30}\nupdated = %{map | age:", 12, NULL},

{"Clojure", "threading_macro",
 "(defn process [coll]\n  (->> coll\n       (filter odd?)\n       (map inc)\n       (take", 12, NULL},

};

#define NUM_TESTS ((int)(sizeof(TESTS)/sizeof(TESTS[0])))


int main(int argc, char *argv[]) {
    const char *filter = argc > 1 ? argv[1] : "";
    int total = 0, passed = 0;

    printf("Code Completion Test Runner — %d test cases\n", NUM_TESTS);
    printf("Server: 127.0.0.1:8080  Binary: %s\n", COMPLETE_BIN);
    printf("Filter: %s\n\n", filter && *filter ? filter : "(none)");
    printf("%-5s %-14s %-28s %-6s %s\n",
           "Num", "Lang", "Name", "Result", "Completion (80 chars)");
    printf("%s\n",
           "-------------------------------------------------------------------------------------");

    for (int i = 0; i < NUM_TESTS; i++) {
        const Test *t = &TESTS[i];
        if (filter && *filter)
            if (!strstr(t->lang, filter) && !strstr(t->name, filter))
                continue;

        char infile[64], outfile[64];
        snprintf(infile,  sizeof(infile),  "/tmp/cc_test_%d.in",  i);
        snprintf(outfile, sizeof(outfile), "/tmp/cc_test_%d.out", i);

        /* Write snippet with <FILL_HERE> appended for FIM-style prompting */
        FILE *inf = fopen(infile, "w");
        if (!inf) {
            printf("[%3d] %-14s %-28s FAIL  (cannot create temp file)\n",
                   i+1, t->lang, t->name);
            continue;
        }
        fprintf(inf, "%s<FILL_HERE>", t->snippet);
        fclose(inf);

        char cmd[512];
        snprintf(cmd, sizeof(cmd), "%s -f %s -n %d -t 0.2 > %s 2>&1",
                 COMPLETE_BIN, infile, t->max_tokens, outfile);
        int rc = system(cmd);

        char result[MAX_RESULT] = "";
        FILE *out = fopen(outfile, "r");
        if (out) { fread(result, 1, MAX_RESULT-1, out); fclose(out); }
        remove(infile);
        remove(outfile);

        int rlen = (int)strlen(result);
        int pass = (rc == 0)
                && rlen > 0
                && strncmp(result, "connect to",    10) != 0
                && strncmp(result, "Server error",  12) != 0
                && strncmp(result, "gethostbyname", 13) != 0
                && !is_garbage(result)
                && (t->expected == NULL || ci_strstr(result, t->expected) != NULL);

        if (pass) passed++;
        total++;

        /* Build one-line display (collapse newlines) */
        char display[81] = "";
        strncpy(display, result, 80);
        display[80] = '\0';
        for (int j = 0; display[j]; j++)
            if (display[j] == '\n' || display[j] == '\r') display[j] = ' ';

        /* On failure, show what was expected */
        if (!pass && t->expected && !ci_strstr(result, t->expected)) {
            printf("[%3d] %-14s %-28s FAIL  (want:%s) got: %s\n",
                   i+1, t->lang, t->name, t->expected, display);
        } else {
            printf("[%3d] %-14s %-28s %s  %s\n",
                   i+1, t->lang, t->name, pass ? "PASS" : "FAIL", display);
        }
        fflush(stdout);
    }

    printf("\n%s\n",
           "-------------------------------------------------------------------------------------");
    printf("Results: %d / %d passed (%.0f%%)\n",
           passed, total, total ? 100.0*passed/total : 0.0);
    return passed == total ? 0 : 1;
}
