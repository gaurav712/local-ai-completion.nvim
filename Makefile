CC      = cc
CFLAGS  = -std=c99 -Wall -Wextra -O2

all: complete test_runner

complete: complete.c
	$(CC) $(CFLAGS) -o complete complete.c

test_runner: test_runner.c
	$(CC) $(CFLAGS) -o test_runner test_runner.c

test: complete test_runner
	./test_runner

clean:
	rm -f complete test_runner /tmp/cc_test_*.in /tmp/cc_test_*.out

.PHONY: all test clean install-nvim

install-nvim:
	@echo "Add to ~/.config/nvim/init.lua:"
	@echo '  vim.opt.rtp:prepend("/Users/gaurav/Projects/prediction/nvim")'
	@echo '  require("prediction").setup()'
