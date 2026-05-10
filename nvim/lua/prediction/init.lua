local M = {}

M.cfg = {
    complete_bin   = "/Users/gaurav/Projects/prediction/complete",
    debounce_ms    = 300,
    max_tokens     = 40,
    temperature    = 0.2,
    max_ctx_lines  = 40,
    max_suffix     = 200,   -- chars after cursor sent to LLM + used for dedup
    min_suggestion = 1,     -- reject suggestions shorter than this
    max_suggestion = 200,   -- cap suggestion length
    enabled        = true,
}

local ns   = vim.api.nvim_create_namespace("prediction")
local tmr  = vim.loop.new_timer()
local job  = nil
local pend = nil   -- {buf, row, col, text}

-- ── Phase 1: Context Extraction ─────────────────────────────────────────────

-- Returns text before cursor (what gets sent to the LLM).
local function extract_prefix(buf, row, col)
    local start = math.max(0, row - M.cfg.max_ctx_lines)
    local lines = vim.api.nvim_buf_get_lines(buf, start, row + 1, false)
    if #lines > 0 then
        lines[#lines] = lines[#lines]:sub(1, col)
    end
    return table.concat(lines, "\n")
end

-- Returns text after cursor (used only for deduplication, never sent to LLM).
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

-- Strategy A: greedy longest exact match.
-- Finds the longest L such that generated[1..L] == suffix[1..L],
-- then returns generated[L+1..] (the part that isn't already there).
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

-- Splits s into a list of {tok, end_byte} pairs.
-- Tokens are either word-runs ([%w_]+) or single non-word characters.
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

-- Strategy B: token-based match.
-- Finds the longest k such that gen_tokens[1..k] == suf_tokens[1..k],
-- then returns generated from after the k-th token's byte position.
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

-- Try exact match first; fall back to token match.
local function dedup(generated, suffix)
    local after_exact = dedup_exact(generated, suffix)
    if after_exact ~= generated then return after_exact end
    return dedup_tokens(generated, suffix)
end

-- ── Phase 3b: Fence Stripping ────────────────────────────────────────────────

-- The LLM sometimes wraps output in markdown code fences despite the system
-- prompt saying not to. Strip them before showing ghost text.
local function strip_fences(text)
    text = text:gsub("^```[^\n]*\n", "")   -- opening ```[lang]\n
    text = text:gsub("\n```%s*$", "")       -- closing \n```
    text = text:gsub("^```%s*$", "")        -- bare ``` if that's all that's left
    return text
end

-- ── Phase 4: Validation & Filtering ─────────────────────────────────────────

local function validate(text, prefix)
    if #text < M.cfg.min_suggestion then return nil end
    if text:match("^%s*$") then return nil end
    -- Cap length.
    if #text > M.cfg.max_suggestion then
        text = text:sub(1, M.cfg.max_suggestion)
    end
    -- Reject if prefix already ends with this exact text (repetition).
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
    local mode = vim.api.nvim_get_mode().mode
    if mode:sub(1, 1) ~= "i" then return end
    if vim.bo.buftype ~= "" then return end

    local buf = vim.api.nvim_get_current_buf()
    local cur = vim.api.nvim_win_get_cursor(0)
    local row = cur[1] - 1   -- 0-indexed
    local col = cur[2]       -- 0-indexed byte offset

    -- Phase 1: extract prefix and suffix.
    -- Both are sent to the LLM (FIM format); suffix is also kept for dedup.
    local prefix = extract_prefix(buf, row, col)
    local suffix = extract_suffix(buf, row, col)
    if #prefix < 5 then return end

    -- Build FIM prompt: surround the cursor gap with a marker so the model
    -- knows exactly what it must fill rather than blindly continuing.
    local fim_prompt = prefix .. "<FILL_HERE>" .. suffix

    local tmpfile = os.tmpname()
    local f = io.open(tmpfile, "w")
    if not f then return end
    f:write(fim_prompt)
    f:close()

    local chunks = {}

    job = vim.fn.jobstart(
        { M.cfg.complete_bin, "-f", tmpfile,
          "-n", tostring(M.cfg.max_tokens),
          "-t", tostring(M.cfg.temperature) },
        {
            on_stdout = function(_, data)
                vim.list_extend(chunks, data)
            end,
            on_exit = function(_, code)
                job = nil
                os.remove(tmpfile)
                if code ~= 0 then return end

                -- Strip trailing empty lines that jobstart appends.
                while #chunks > 0 and chunks[#chunks] == "" do
                    chunks[#chunks] = nil
                end
                local generated = table.concat(chunks, "\n")
                if #generated == 0 then return end

                -- Phase 3: deduplicate against suffix.
                local deduped = dedup(generated, suffix)

                -- Phase 3b: strip markdown code fences the model left in.
                deduped = strip_fences(deduped)

                -- Phase 4: validate.
                local suggestion = validate(deduped, prefix)
                if not suggestion then return end

                vim.schedule(function()
                    if not M.cfg.enabled then return end
                    if vim.api.nvim_get_mode().mode:sub(1, 1) ~= "i" then return end
                    if vim.api.nvim_get_current_buf() ~= buf then return end

                    local now     = vim.api.nvim_win_get_cursor(0)
                    local now_row = now[1] - 1
                    local now_col = now[2]

                    -- Row changed (Enter pressed, etc.) → stale.
                    if now_row ~= row then return end

                    local display_text = suggestion
                    local display_col  = now_col   -- always anchor to current cursor

                    -- Step 1: cursor moved since trigger.
                    if now_col ~= col then
                        if now_col < col then return end  -- deleted backward → stale

                        -- User typed forward: valid only if typed chars match suggestion.
                        local cur_line = vim.api.nvim_get_current_line()
                        local typed    = cur_line:sub(col + 1, now_col)
                        if display_text:sub(1, #typed) ~= typed then
                            return  -- diverged → discard
                        end
                        display_text = display_text:sub(#typed + 1)
                    end

                    -- Step 2: strip any echo of already-typed text at the suggestion
                    -- head. Models often repeat the last few chars before the cursor
                    -- (e.g. suggestion="def function_name" when line ends with "def fun").
                    local before_cur = vim.api.nvim_get_current_line():sub(1, display_col)
                    for L = math.min(#display_text, #before_cur), 1, -1 do
                        if display_text:sub(1, L) == before_cur:sub(-L) then
                            display_text = display_text:sub(L + 1)
                            break
                        end
                    end

                    -- Re-validate after both trims.
                    display_text = validate(display_text, prefix)
                    if not display_text then return end

                    show_ghost(buf, now_row, display_col, display_text)
                end)
            end,
        }
    )
end

-- ── Event Handlers ───────────────────────────────────────────────────────────

local function on_changed()
    tmr:stop()
    if job then
        vim.fn.jobstop(job)
        job = nil
    end
    local buf = vim.api.nvim_get_current_buf()
    vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
    pend = nil
    tmr:start(M.cfg.debounce_ms, 0, vim.schedule_wrap(do_complete))
end

local function on_leave()
    tmr:stop()
    if job then
        vim.fn.jobstop(job)
        job = nil
    end
    local buf = vim.api.nvim_get_current_buf()
    vim.api.nvim_buf_clear_namespace(buf, ns, 0, -1)
    pend = nil
end

-- Returns "" when accepting ghost text; falls through to blink.cmp otherwise.
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

    -- expr=true, noremap=false: returned <Tab> falls through to blink.cmp.
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
