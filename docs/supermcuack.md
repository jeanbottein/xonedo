# supermcuack: what's in this build and how it was checked

`supermcuack` is this fork's combined build: everything we found useful for the Xbox Wireless Adapter, in one
place, with the evidence for each change. It's an experiment in how far one user with a gaming PC, an AI coding
assistant (Claude) and the community's open pull requests can take a driver. Overview and discussion:
[issue #8](https://github.com/jeanbottein/xonedo/issues/8). Change list: [PR #7](https://github.com/jeanbottein/xonedo/pull/7).

Test hardware: one `045e:02e6` dongle, Elite Series 1 + Series controllers, Bazzite (Fedora) kernels 7.2–7.3.
Other dongles are untested: reports are welcome on issue #8.

## What's in it

| Area | Changes | How it was checked |
|---|---|---|
| Warm start (reboot, resume, reload with USB power kept) | WLAN reset, RX stop + drain before the firmware upload, RX stop on unload | measured: upload stalls 3 in 13 → 0 in 180+ warm loads; cold starts by replug |
| Controllers stuck pairing | no pairing scan on `02e6`; client-address read-back + self-reset | ~1 session in 10 had the firmware ignore the address write; caught and recovered every time since |
| Firmware commands | numbered, one at a time, wait for the answer, resend (as mainline `mt76x02u`) | 2,550+ commands answered in test runs; daily use counts resends (see below) |
| Driver-wide review | double free, leaks, races, use-after-free on rumble/headset, capture overflow, unaligned read, URB leak, key in log, unplug wait | code review, then built and played |
| Debug kernel | auth: heap overflow in the transcript export, sleeping allocation in the RX path | found by KASAN and lockdep on the first pad connections and fixed; the fixed build is built and played on the host, not yet re-run under the debug kernel |
| Install | `mt76x2u` blacklisted again (it took the old dongle), loaded only for `02fe`; `xpad` no longer blacklisted | reproduced and checked in a Fedora 45 VM |
| Fuzzing | `tests/fuzz`: GIP parser and headset capture, libFuzzer + ASan/UBSan | finds the old double free and overflow within minutes; found an unaligned read; then ~1.2 billion inputs clean |
| Community PRs | cherry-picked from dlundqvist/xone, see PR #7 | mostly compile-tested only, see PR #7 |

## Checks you can repeat

- **Fuzzing**: `make -C tests/fuzz run FUZZ_TIME=600` (Linux, clang with libFuzzer).
- **Debug-kernel run**: a Fedora 45 VM with `kernel-debug` (KASAN, lockdep, atomic-sleep and timer debugging,
  `kasan_multi_shot`) and the dongle passed through with qemu `usb-host`. Results below.
- **Logs**: build with `make debug`, then `journalctl -k | grep xone-fix` shows firmware loads, connection times,
  MCU stats (sent, answered, resent, failed) and every time a safety net acted.

## Known limits

- One dongle model, one household.
- Why the firmware sometimes ignores the client-address write is still unknown; the read-back + self-reset is a
  workaround. Confirmed commands are being tested as the real fix.
- `xone_mtnet` (new dongle `02fe` on mainline `mt76x2u`) has never run here: no `02fe` dongle.

## Debug-kernel results (2026-11-22)

Fedora 45 `kernel-debug` 7.3.4 in qemu, dongle passed through, Elite Series 1 controller.

| Run | Result |
|---|---|
| Boot with `mt76x2u` not blacklisted (stock `make install` before this branch's fix) | `mt76x2u` took the `02e6` as a Wi-Fi adapter with MediaTek's firmware; unbinding it dropped the dongle off USB until a replug |
| Boot with the blacklist | dongle left free, `xone_dongle` took it, firmware OK |
| First controller connection | lockdep: `GFP_KERNEL` allocation in softirq (`gip_auth_get_transcript`, RX path) → fixed `3c2d5a7` |
| Second connection | KASAN: slab-out-of-bounds write in `crypto_sha256_export` (buffer sized with `descsize`, not `statesize`) → fixed `b016a89` |

Caveat for anyone repeating this: handing the dongle between the host's driver and qemu (host loading firmware,
qemu resetting the device when it takes it) left the chip answering `-121` to everything until a physical
replug. Keep the host driver off the dongle (blacklist it for the test) and give qemu a freshly plugged dongle.
The rumble/switch-off and reload paths were not run under the debug kernel yet for that reason.
