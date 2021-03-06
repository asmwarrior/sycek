#
# Copyright 2018 Jiri Svoboda
#
# Permission is hereby granted, free of charge, to any person obtaining
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#

CC     = gcc
CFLAGS_common = -std=c99 -D_GNU_SOURCE -O0 -ggdb -Wall -Wextra -Wmissing-prototypes \
         -Werror -I src
CFLAGS = $(CFLAGS_common) -I src/hcompat
LIBS   =

CC_hos = helenos-cc
CFLAGS_hos = $(CFLAGS_common)
LD_hos = helenos-ld
LIBS_hos = $(LIBS)
PREFIX_hos = `helenos-bld-config --install-dir`
INSTALL = install

bkqual = $$(date '+%Y-%m-%d')

sources_common = \
    src/ast.c \
    src/checker.c \
    src/file_input.c \
    src/lexer.c \
    src/main.c \
    src/parser.c \
    src/src_pos.c \
    src/str_input.c \
    src/test/ast.c \
    src/test/checker.c \
    src/test/lexer.c \
    src/test/parser.c

sources = \
    $(sources_common) \
    src/hcompat/adt/list.c

sources_hos = \
    $(sources_common)

binary = ccheck
binary_hos = ccheck-hos
ccheck = ./$(binary)

objects = $(sources:.c=.o)
objects_hos = $(sources_hos:.c=.hos.o)
headers = $(wildcard *.h */*.h */*/*.h)

test_good_ins = $(wildcard test/good/*-in.c)
test_good_out_diffs = $(test_good_ins:-in.c=-out.txt.diff)
test_bad_ins = $(wildcard test/bad/*-in.c)
test_bad_errs = $(test_bad_ins:-in.c=-err-t.txt)
test_bad_err_diffs = $(test_bad_ins:-in.c=-err.txt.diff)
test_ugly_ins = $(wildcard test/ugly/*-in.c)
test_ugly_fixed_diffs = $(test_ugly_ins:-in.c=-fixed.c.diff)
test_ugly_out_diffs = $(test_ugly_ins:-in.c=-out.txt.diff)
test_ugly_h_ins = $(wildcard test/ugly/*-in.h)
test_ugly_h_fixed_diffs = $(test_ugly_h_ins:-in.h=-fixed.h.diff)
test_ugly_h_out_diffs = $(test_ugly_h_ins:-in.h=-out.txt.diff)
test_vg_outs = \
    $(test_good_ins:-in.c=-vg.txt) \
    $(test_ugly_ins:-in.c=-vg.txt) \
    $(test_ugly_h_ins:-in.h=-vg.txt)
test_outs = $(test_good_fixed_diffs) $(test_good_out_diffs) \
    $(test_bad_err_diffs) $(test_bad_errs) $(test_ugly_fixed_diffs) \
    $(test_ugly_h_fixed_diffs) $(test_ugly_err_diffs) $(test_ugly_out_diffs) \
    $(text_ugly_h_out_diffs) $(test_vg_outs) \
    test/all.diff test/test-int.out test/selfcheck.out

all: $(binary)

$(binary): $(objects)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(objects): $(headers)

hos: $(binary_hos)

$(binary_hos): $(objects_hos)
	$(LD_hos) $(CFLAGS_hos) -o $@ $^ $(LIBS_hos)

$(objects_hos): $(headers_hos)

%.hos.o: %.c
	$(CC_hos) -c $(CFLAGS_hos) -o $@ $^

install-hos: hos
	mkdir -p $(PREFIX_hos)/app
	$(INSTALL) -T $(binary_hos) $(PREFIX_hos)/app/ccheck

uninstall-hos:
	rm -f $(PREFIX_hos)/app/ccheck

test-hos: install-hos
	helenos-test

clean:
	rm -f $(objects) $(objects_hos) $(binary) $(binary_hos) $(test_outs)

test/good/%-out-t.txt: test/good/%-in.c $(ccheck)
	$(ccheck) $< >$@

test/good/%-out.txt.diff: /dev/null test/good/%-out-t.txt
	diff -u $^ >$@

test/good/%-vg.txt: test/good/%-in.c $(ccheck)
	valgrind $(ccheck) $^ 2>$@
	grep -q 'no leaks are possible' $@

test/bad/%-err-t.txt: test/bad/%-in.c $(ccheck)
	-$(ccheck) $< 2>$@

test/bad/%-err.txt.diff: test/bad/%-err.txt test/bad/%-err-t.txt
	diff -u $^ >$@

test/ugly/%-fixed-t.c: test/ugly/%-in.c $(ccheck)
	cp $< $@
	$(ccheck) --fix $@
	rm -f $@.orig

test/ugly/%-fixed-t.h: test/ugly/%-in.h $(ccheck)
	cp $< $@
	$(ccheck) --fix $@
	rm -f $@.orig

test/ugly/%-fixed.c.diff: test/ugly/%-fixed.c test/ugly/%-fixed-t.c
	diff -u $^ >$@

test/ugly/%-fixed.h.diff: test/ugly/%-fixed.h test/ugly/%-fixed-t.h
	diff -u $^ >$@

test/ugly/%-out-t.txt: test/ugly/%-in.c $(ccheck)
	$(ccheck) $< >$@

test/ugly/%-out-t.txt: test/ugly/%-in.h $(ccheck)
	$(ccheck) $< >$@

test/ugly/%-out.txt.diff: test/ugly/%-out.txt test/ugly/%-out-t.txt
	diff -u $^ >$@

test/ugly/%-vg.txt: test/ugly/%-in.c $(ccheck)
	valgrind $(ccheck) $^ >/dev/null 2>$@
	grep -q 'no leaks are possible' $@

test/ugly/%-vg.txt: test/ugly/%-in.h $(ccheck)
	valgrind $(ccheck) $^ >/dev/null 2>$@
	grep -q 'no leaks are possible' $@

test/all.diff: $(test_good_out_diffs) $(test_bad_err_diffs) \
    $(test_ugly_fixed_diffs) $(test_ugly_h_fixed_diffs) \
    $(test_ugly_out_diffs) $(test_ugly_h_out_diffs) \
    $(test_vg_out_diffs)
	cat $^ > $@

# Run internal unit tests
test/test-int.out: $(ccheck)
	$(ccheck) --test >test/test-int.out

selfcheck: test/selfcheck.out

test/selfcheck.out: $(ccheck)
	PATH=$$PATH:$$PWD ./ccheck-run.sh src > $@
	grep "^Ccheck passed." $@

#
# Note that if any of the diffs is not empty, that diff command will
# return non-zero exit code, failing the make
#
test: test/test-int.out test/all.diff $(test_vg_outs) test/selfcheck.out

backup: clean
	cd .. && tar czf sycek-$(bkqual).tar.gz trunk
	cd .. && rm -f sycek-latest.tar.gz && ln -s sycek-$(bkqual).tar.gz \
	    sycek-latest.tar.gz
