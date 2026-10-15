CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -g -Isrc -Ibuild
BUILD   = build
OBJS    = $(BUILD)/parser.o $(BUILD)/lexer.o $(BUILD)/main.o $(BUILD)/ast.o \
          $(BUILD)/table.o $(BUILD)/symtab.o $(BUILD)/semantic.o \
          $(BUILD)/interp.o $(BUILD)/errors.o

all: cringe

cringe: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD):
	mkdir -p $(BUILD)

# Bison genera el parser y la cabecera con los tokens
$(BUILD)/parser.c $(BUILD)/parser.h: src/parser.y | $(BUILD)
	bison -d -o $(BUILD)/parser.c src/parser.y

# Flex genera el lexer (necesita parser.h para conocer los tokens)
$(BUILD)/lexer.c: src/lexer.l $(BUILD)/parser.h | $(BUILD)
	flex -o $@ src/lexer.l

# El codigo generado por Flex y Bison puede dar avisos que no son errores
$(BUILD)/lexer.o: $(BUILD)/lexer.c
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

$(BUILD)/parser.o: $(BUILD)/parser.c
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

$(BUILD)/%.o: src/%.c $(BUILD)/parser.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

test: cringe
	./tests/run_tests.sh

clean:
	rm -rf $(BUILD) cringe salida

.PHONY: all test clean
