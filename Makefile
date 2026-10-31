# Builds the tcc-wasm kit with TinyCC. See README.md.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/lexer.c src/front/parser.c src/front/fmt.c src/front/front.c src/front/eval.c src/front/check.c src/front/lower.c
TARGET = src/wasm.c
SRC = src/main.c src/json.c src/sql.c src/wasmbin.c src/oracle.c $(FRONT) $(TARGET)
FRONT_TOOL = test/fronttool.c $(filter-out src/front/front.c src/front/lower.c,$(FRONT))
CUBICAL_TOOL = test/cubical.c src/json.c src/sql.c $(filter-out src/front/lower.c,$(FRONT))
HEADERS = $(wildcard src/*.h src/front/*.h)

.PHONY: build check check-clang clean

build: build/langc build/fronttool

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c domain_source

build/prelude.c: src/front/prelude.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c src/front/prelude.lang build/prelude.c prelude_source

build/langc: $(SRC) $(HEADERS) build/domain.c build/prelude.c
	$(TCC) $(CFLAGS) -o build/langc $(SRC) build/domain.c build/prelude.c

build/fronttool: $(FRONT_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/fronttool $(FRONT_TOOL)

build/diagnostics: test/diagnostics.c $(SRC) $(HEADERS) build/domain.c build/prelude.c
	$(TCC) $(CFLAGS) -o build/diagnostics test/diagnostics.c $(filter-out src/main.c,$(SRC)) build/domain.c build/prelude.c

build/cubical: $(CUBICAL_TOOL) $(HEADERS) build/domain.c build/prelude.c
	$(TCC) $(CFLAGS) -o build/cubical $(CUBICAL_TOOL) build/domain.c build/prelude.c

build/sql-writer: test/sql-writer.c $(SRC) $(HEADERS) build/domain.c build/prelude.c
	$(TCC) $(CFLAGS) -o build/sql-writer test/sql-writer.c $(filter-out src/main.c,$(SRC)) build/domain.c build/prelude.c

build/oracle-writer: test/oracle-writer.c $(SRC) $(HEADERS) build/domain.c build/prelude.c
	$(TCC) $(CFLAGS) -o build/oracle-writer test/oracle-writer.c $(filter-out src/main.c,$(SRC)) build/domain.c build/prelude.c

check-clang: build/domain.c build/prelude.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c build/prelude.c
	$(CC) $(CLANG_FLAGS) $(FRONT_TOOL)
	$(CC) $(CLANG_FLAGS) test/diagnostics.c $(filter-out src/main.c,$(SRC)) build/domain.c build/prelude.c
	$(CC) $(CLANG_FLAGS) $(CUBICAL_TOOL) build/domain.c build/prelude.c
	$(CC) $(CLANG_FLAGS) test/sql-writer.c $(filter-out src/main.c,$(SRC)) build/domain.c build/prelude.c
	$(CC) $(CLANG_FLAGS) test/oracle-writer.c $(filter-out src/main.c,$(SRC)) build/domain.c build/prelude.c

check: build/langc build/fronttool build/diagnostics build/cubical build/sql-writer build/oracle-writer check-clang
	sh test/gate.sh
	sh test/review.sh
	build/fronttool
	build/diagnostics
	build/cubical
	build/sql-writer

clean:
	rm -rf build
