CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Inative/include -Inative/generated
BISON ?= bison
FLEX ?= flex
BIN = build/mpc
SOURCES = $(wildcard native/src/*.c) native/generated/parser.tab.c native/generated/lex.yy.c
HEADERS = $(wildcard native/include/*.h) native/generated/parser.tab.h

.PHONY: all clean regen test fuzz sanitize
all: $(BIN)
$(BIN): $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@
build:
	mkdir -p $@
# Generated sources are versioned; regeneration requires Bison 3+ and Flex.
regen:
	$(BISON) -d -o native/generated/parser.tab.c native/grammar/parser.y
	$(FLEX) -o native/generated/lex.yy.c native/grammar/lexer.l
test: $(BIN)
	python3 tests/native/test_pipeline.py ./$(BIN)
fuzz: $(BIN)
	python3 tests/native/fuzz_equiv.py ./$(BIN) --count 1500
sanitize: | build
	$(CC) $(CPPFLAGS) -std=c11 -g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o build/mpc-sanitized
	python3 tests/native/test_pipeline.py ./build/mpc-sanitized
clean:
	rm -f build/mpc build/mpc.exe build/mpc-sanitized
