# Fuzzing

Userspace libFuzzer harnesses (ASan + UBSan) for the code that parses data a device sends over the air.

| Fuzzer | Code under test | What it feeds |
|---|---|---|
| `fuzz_gip` | `bus/protocol.c` (whole GIP parser: headers, chunking, identify, audio control, acks) | radio frames, host chunked sends, late driver binds; removed clients are freed at once (worst-case timing) |
| `fuzz_headset_capture` | `gip_headset_copy_capture()` from `driver/headset.c` | microphone packets of any length into ALSA buffers of any legal size |

`kshim.h` stands in for the few kernel APIs used. `gip_free_client_info()` and the headset copy functions are
extracted from the real sources at build time (see `Makefile`), so the fuzzers test the shipped code.

```
make -C tests/fuzz CC=clang          # needs a clang with libFuzzer (Linux)
make -C tests/fuzz run FUZZ_TIME=600
```

Sanity check: built against the code before the 2026-11 review fixes, `fuzz_gip` finds the
`firmware_versions` double free and `fuzz_headset_capture` the capture-buffer overflow within minutes.
The first run on the fixed code also found an unaligned 16-bit read in `gip_parse_classes()`.
