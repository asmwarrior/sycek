CC     = gcc
CFLAGS = -std=c99 -D_GNU_SOURCE -O0 -ggdb -Wall -Wextra -Wmissing-prototypes \
         -Werror -I. 
LIBS   =

bkqual = $$(date '+%Y-%m-%d')

sources = \
    adt/list.c \
    ast.c \
    checker.c \
    file_input.c \
    lexer.c \
    main.c \
    parser.c \
    src_pos.c \
    str_input.c \
    test/ast.c \
    test/checker.c \
    test/lexer.c \
    test/parser.c

binary = ccheck

objects = $(sources:.c=.o)
headers = $(wildcard *.h */*.h */*/*.h)

all: $(binary)

$(binary): $(objects)
	gcc $(CFLAGS) -o $@ $^ $(LIBS)

$(objects): $(headers)

clean:
	rm -f $(objects) $(binary)

backup: clean
	cd .. && tar czf ccheck-$(bkqual).tar.gz trunk
	cd .. && rm -f ccheck-latest.tar.gz && ln -s ccheck-$(bkqual).tar.gz ccheck-latest.tar.gz
