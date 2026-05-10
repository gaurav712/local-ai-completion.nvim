local M = {}

M.cfg = {
    server_host        = "127.0.0.1",
    server_port        = 8080,
    debounce_ms        = 300,
    request_timeout_ms = 5000,
    max_tokens         = 40,
    temperature        = 0.2,
    max_ctx_lines      = 40,
    max_suffix         = 200,
    min_suggestion     = 1,
    max_suggestion     = 200,
    enabled            = true,
}

local ns      = vim.api.nvim_create_namespace("prediction")
local tmr     = vim.loop.new_timer()
local tmr_out = vim.loop.new_timer()
local job     = nil
local pend    = nil   -- {buf, row, col, text}
local req_id  = 0     -- monotonic; incremented on every new request cycle

local SYSTEM = "You are a code completion engine. "
    .. "The user will provide code containing a <FILL_HERE> marker. "
    .. "Output ONLY the code that replaces <FILL_HERE>, at most 1-3 lines. "
    .. "Raw code only — no explanation, no markdown fences, no preamble."

-- ── Phase 1: Context Extraction ─────────────────────────────────────────────

local function extract_prefix(buf, row, col)
    local start = math.max(0, row - M.cfg.max_ctx_lines)
    local lines = vim.api.nvim_buf_get_lines(buf, start, row + 1, false)
    if #lines > 0 then
        lines[#lines] = lines[#lines]:sub(1, col)
    end
    return table.concat(lines, "\n")
end

local function extract_suffix(buf, row, col)
    local line = (vim.api.nvim_buf_get_lines(buf, row, row + 1, false))[1] or ""
    local after_cursor = line:sub(col + 1)
    local next_lines   = vim.api.nvim_buf_get_lines(buf, row + 1, row + 10, false)
    local parts = { after_cursor }
    for _, l in ipairs(next_lines) do
        parts[#parts + 1] = l
        if #table.concat(parts, "\n") >= M.cfg.max_suffix then break end
    end
    local suffix = table.concat(parts, "\n")
    return suffix:sub(1, M.cfg.max_suffix)
end

-- ── Phase 3: Suffix Deduplication ───────────────────────────────────────────

local function dedup_exact(generated, suffix)
    if #suffix == 0 then return generated end
    local max_len = math.min(#generated, #suffix)
    for len = max_len, 1, -1 do
        if generated:sub(1, len) == suffix:sub(1, len) then
            return generated:sub(len + 1)
        end
    end
    return generated
end

local function tokenize(s)
    local tokens = {}
    local i = 1
    while i <= #s do
        local j = s:find("[^%w_]", i)
        if not j then
            tokens[#tokens + 1] = { tok = s:sub(i), pos = #s }
            break
        end
        if j > i then
            tokens[#tokens + 1] = { tok = s:sub(i, j - 1), pos = j - 1 }
        end
        tokens[#tokens + 1] = { tok = s:sub(j, j), pos = j }
        i = j + 1
    end
    return tokens
end

local function dedup_tokens(generated, suffix)
    if #suffix == 0 then return generated end
    local gen_tok = tokenize(generated)
    local suf_tok = tokenize(suffix)
    local k = 0
    for i = 1, math.min(#gen_tok, #suf_tok) do
        if gen_tok[i].tok == suf_tok[i].tok then
            k = i
        else
            break
        end
    end
    if k == 0 then return generated end
    return generated:sub(gen_tok[k].pos + 1)
end

local function dedup(generated, suffix)
    local after_exact = dedup_exact(generated, suffix)
    if after_exact ~= generated then return after_exact end
    return dedup_tokens(generated, suffix)
end

-- ── Phase 3b: Fence Stripping ────────────────────────────────────────────────

local function strip_fences(text)
    text = text:gsub("^```[^\n]*\n", "")
    text = text:gsub("\n```%s*$", "")
    text = text:gsub("^```%s*$", "")
    return text
end

-- ── Phase 4: Validation & Filtering ─────────────────────────────────────────

local function validate(text, prefix)
    if #text < M.cfg.min_suggestion then return nil end
    if text:match("^%s*$") then return nil end
    if #text > M.cfg.max_suggestion then
        text = text:sub(1, M.cfg.max_suggestion)
    end
    if #text <= #prefix and prefix:sub(-#text) == text then return nil end
    return text
end

-- ── Phase 5: Display & Rendering ────────────────────────────────────────────

local function show_ghost(buf, row, col, text)
    vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
    local parts = vim.split(text, "\n", { plain = true })
    local virt_lines = {}
    for i = 2, #parts do
        virt_lines[#virt_lines + 1] = { { parts[i], "Comment" } }
    end
    vim.api.nvim_buf_set_extmark(buf, ns, row, col, {
        virt_text     = { { parts[1], "Comment" } },
        virt_text_pos = "inline",
        virt_lines    = (#virt_lines > 0) and virt_lines or nil,
        hl_mode       = "combine",
    })
    pend = { buf = buf, row = row, col = col, text = text }
end

-- ── Completion Request ───────────────────────────────────────────────────────

local function do_complete()
    if not M.cfg.enabled then return end
    if vim.api.nvim_get_mode().mode:sub(1, 1) ~= "i" then return end
    if vim.bo.buftype ~= "" then return end

    -- kill any lingering job before reading fresh buffer state
    if job then vim.fn.jobstop(job); job = nil end

    local my_req = req_id   -- capture before any yield point

    local buf = vim.api.nvim_get_current_buf()
    local cur = vim.api.nvim_win_get_cursor(0)
    local row = cur[1] - 1
    local col = cur[2]

    local prefix = extract_prefix(buf, row, col)
    local suffix = extract_suffix(buf, row, col)
    if #prefix < 5 then return end

    local fim_prompt = prefix .. "<FILL_HERE>" .. suffix

    local body = vim.json.encode({
        model       = "local",
        messages    = {
            { role = "system", content = SYSTEM },
            { role = "user",   content = fim_prompt },
        },
        max_tokens  = M.cfg.max_tokens,
        temperature = M.cfg.temperature,
        stop        = { "\n\n" },
    })

    local url = string.format("http://%s:%d/v1/chat/completions",
        M.cfg.server_host, M.cfg.server_port)

    local chunks = {}
    job = vim.fn.jobstart(
        { "curl", "-s", "-X", "POST", url,
          "-H", "Content-Type: application/json",
          "--data", "@-",
          "--max-time", tostring(math.ceil(M.cfg.request_timeout_ms / 1000)) },
        {
            stdin     = "pipe",
            on_stdout = function(_, data) vim.list_extend(chunks, data) end,
            on_exit   = function(_, code)
                if req_id ~= my_req then return end   -- newer request fired; discard
                job = nil
                tmr_out:stop()
                if code ~= 0 then return end

                local raw = table.concat(chunks, "\n")
                if #raw == 0 then return end

                local ok, decoded = pcall(vim.json.decode, raw)
                if not ok then return end

                local content = vim.tbl_get(decoded, "choices", 1, "message", "content")
                if type(content) ~= "string" or #content == 0 then return end

                local deduped    = dedup(content, suffix)
                deduped          = strip_fences(deduped)
                local suggestion = validate(deduped, prefix)
                if not suggestion then return end

                vim.schedule(function()
                    if req_id ~= my_req then return end
                    if not M.cfg.enabled then return end
                    if vim.api.nvim_get_mode().mode:sub(1, 1) ~= "i" then return end
                    if vim.api.nvim_get_current_buf() ~= buf then return end

                    local now     = vim.api.nvim_win_get_cursor(0)
                    local now_row = now[1] - 1
                    local now_col = now[2]

                    if now_row ~= row then return end

                    local display_text = suggestion
                    local display_col  = now_col

                    if now_col ~= col then
                        if now_col < col then return end
                        local cur_line = vim.api.nvim_get_current_line()
                        local typed    = cur_line:sub(col + 1, now_col)
                        if display_text:sub(1, #typed) ~= typed then return end
                        display_text = display_text:sub(#typed + 1)
                    end

                    local before_cur = vim.api.nvim_get_current_line():sub(1, display_col)
                    for L = math.min(#display_text, #before_cur), 1, -1 do
                        if display_text:sub(1, L) == before_cur:sub(-L) then
                            display_text = display_text:sub(L + 1)
                            break
                        end
                    end

                    display_text = validate(display_text, prefix)
                    if not display_text then return end
                    show_ghost(buf, now_row, display_col, display_text)
                end)
            end,
        }
    )

    vim.fn.chansend(job, body)
    vim.fn.chanclose(job, "stdin")

    -- Lua-side timeout (curl --max-time handles the network layer; this covers edge cases)
    tmr_out:stop()
    tmr_out:start(M.cfg.request_timeout_ms, 0, vim.schedule_wrap(function()
        if req_id == my_req and job ~= nil then
            vim.fn.jobstop(job)
            job = nil
        end
    end))
end

-- ── Event Handlers ───────────────────────────────────────────────────────────

local function on_changed()
    tmr:stop()
    tmr_out:stop()
    if job then vim.fn.jobstop(job); job = nil end
    req_id = req_id + 1
    local cur_req = req_id
    local buf = vim.api.nvim_get_current_buf()
    vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
    pend = nil
    tmr:start(M.cfg.debounce_ms, 0, vim.schedule_wrap(function()
        if req_id == cur_req then do_complete() end
    end))
end

local function on_leave()
    tmr:stop()
    tmr_out:stop()
    if job then vim.fn.jobstop(job); job = nil end
    req_id = req_id + 1
    local buf = vim.api.nvim_get_current_buf()
    vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
    pend = nil
end

local function tab_handler()
    if pend and vim.api.nvim_get_current_buf() == pend.buf then
        local cur = vim.api.nvim_win_get_cursor(0)
        if cur[1] - 1 == pend.row and cur[2] == pend.col then
            local buf  = pend.buf
            local text = pend.text
            local r    = pend.row
            local c    = pend.col
            vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
            pend = nil

            local parts = vim.split(text, "\n", { plain = true })
            local line  = vim.api.nvim_get_current_line()

            if #parts == 1 then
                local new = line:sub(1, c) .. parts[1] .. line:sub(c + 1)
                vim.api.nvim_buf_set_lines(buf, r, r + 1, false, { new })
                vim.api.nvim_win_set_cursor(0, { r + 1, c + #parts[1] })
            else
                local head      = line:sub(1, c) .. parts[1]
                local tail      = parts[#parts] .. line:sub(c + 1)
                local new_lines = { head }
                for i = 2, #parts - 1 do
                    new_lines[#new_lines + 1] = parts[i]
                end
                new_lines[#new_lines + 1] = tail
                vim.api.nvim_buf_set_lines(buf, r, r + 1, false, new_lines)
                vim.api.nvim_win_set_cursor(0, { r + #parts, #parts[#parts] })
            end
            return ""
        end
    end
    return vim.api.nvim_replace_termcodes("<Tab>", true, false, true)
end

-- ── Setup ────────────────────────────────────────────────────────────────────

function M.setup(opts)
    M.cfg = vim.tbl_deep_extend("force", M.cfg, opts or {})

    local grp = vim.api.nvim_create_augroup("prediction", { clear = true })

    vim.api.nvim_create_autocmd({ "TextChangedI", "TextChangedP" }, {
        group    = grp,
        callback = on_changed,
    })

    vim.api.nvim_create_autocmd({ "InsertLeave", "BufLeave" }, {
        group    = grp,
        callback = on_leave,
    })

    vim.keymap.set("i", "<Tab>", tab_handler,
        { expr = true, noremap = false, silent = true })

    vim.api.nvim_create_user_command("PredictionEnable", function()
        M.cfg.enabled = true
        vim.notify("Prediction enabled")
    end, {})

    vim.api.nvim_create_user_command("PredictionDisable", function()
        M.cfg.enabled = false
        vim.notify("Prediction disabled")
    end, {})

    vim.api.nvim_create_user_command("PredictionToggle", function()
        M.cfg.enabled = not M.cfg.enabled
        vim.notify("Prediction " .. (M.cfg.enabled and "enabled" or "disabled"))
    end, {})
end

return M
