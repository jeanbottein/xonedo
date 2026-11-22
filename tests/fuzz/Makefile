# Userspace fuzzing of the GIP parser and the headset capture copy.
# Needs a clang with libFuzzer (Linux: distro/Homebrew clang).
#   make -C tests/fuzz            build both fuzzers
#   make -C tests/fuzz run        fuzz the GIP parser for FUZZ_TIME seconds
CC ?= clang
FUZZ_TIME ?= 600
FLAGS = -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined \
	-Iinclude -I. -Wno-unused-parameter -Wno-gnu-variable-sized-type-not-at-end

all: fuzz_gip fuzz_headset_capture

gen_free_client_info.c: ../../bus/bus.c
	awk '/^void gip_free_client_info\(/,/^}/' $< > $@

fuzz_gip: fuzz_gip.c gen_free_client_info.c ../../bus/protocol.c kshim.h
	$(CC) $(FLAGS) -o $@ fuzz_gip.c ../../bus/protocol.c

gen_headset_capture.c: ../../driver/headset.c
	awk '/^static bool gip_headset_advance_pointer\(/,/^}/' $< > $@
	awk '/^static bool gip_headset_copy_capture\(/,/^}/' $< >> $@

fuzz_headset_capture: fuzz_headset_capture.c gen_headset_capture.c kshim.h
	$(CC) $(FLAGS) -o $@ fuzz_headset_capture.c

run: fuzz_gip
	mkdir -p corpus/gip
	./fuzz_gip -max_total_time=$(FUZZ_TIME) -max_len=4096 corpus/gip

clean:
	rm -f fuzz_gip fuzz_headset_capture gen_*.c crash-* leak-* oom-* timeout-*

.PHONY: all run clean
