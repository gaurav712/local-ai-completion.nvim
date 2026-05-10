-- Auto-sourced when nvim/ is in runtimepath.
-- Calls setup with defaults; no manual require needed.
-- Override defaults: require("prediction").setup({ debounce_ms = 500, ... })
if vim.g.prediction_loaded then return end
vim.g.prediction_loaded = true
require("prediction").setup()
