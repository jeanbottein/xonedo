/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Minimal userspace stand-ins for the kernel APIs bus/protocol.c uses, so the
 * GIP parser can be fuzzed with libFuzzer + ASan/UBSan. Locks are no-ops (the
 * harness is single-threaded); allocation is plain malloc so ASan sees it.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef uint16_t __le16;
typedef uint32_t __le32;
typedef uint16_t __be16;
typedef unsigned int gfp_t;

#define __packed __attribute__((packed))
#define GFP_ATOMIC 0
#define GFP_KERNEL 0
#define ENOTSUPP 524

#define BIT(n) (1UL << (n))
#define GENMASK(h, l) (((~0UL) << (l)) & (~0UL >> (sizeof(long) * 8 - 1 - (h))))
#define FIELD_GET(mask, reg) (((reg) & (mask)) >> __builtin_ctzl(mask))
#define FIELD_PREP(mask, val) (((typeof(mask))(val) << __builtin_ctzl(mask)) & (mask))
#define MSEC_PER_SEC 1000L
#define min_t(t, a, b) ((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define struct_size(p, member, n) (sizeof(*(p)) + sizeof((p)->member[0]) * (size_t)(n))

/* the targets are little-endian */
#define le16_to_cpu(x) ((u16)(x))
#define cpu_to_le16(x) ((u16)(x))
#define le32_to_cpu(x) ((u32)(x))
#define cpu_to_le32(x) ((u32)(x))
#define cpu_to_be16(x) __builtin_bswap16(x)
static inline u16 le16_to_cpup(const __le16 *p) { return *p; }
static inline u16 get_unaligned_le16(const void *p) { u16 v; memcpy(&v, p, 2); return v; }
#define LINUX_VERSION_CODE 0x070300
#define KERNEL_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))

#define ERR_PTR(e) ((void *)(intptr_t)(e))
#define PTR_ERR(p) ((long)(intptr_t)(p))
#define IS_ERR(p) ((uintptr_t)(p) >= (uintptr_t)-4095)

static inline void *kzalloc(size_t n, gfp_t f) { (void)f; return calloc(1, n ? n : 1); }
static inline void *kmalloc(size_t n, gfp_t f) { (void)f; return malloc(n ? n : 1); }
static inline void kfree(const void *p) { free((void *)p); }

typedef struct { int unused; } spinlock_t;
#define spin_lock_init(l) ((void)(l))
#define spin_lock_irqsave(l, f) ((void)(l), (f) = 0)
#define spin_unlock_irqrestore(l, f) ((void)(l), (void)(f))

struct semaphore { int count; };
static inline void sema_init(struct semaphore *s, int n) { s->count = n; }
static inline int down_trylock(struct semaphore *s) { if (s->count <= 0) return 1; s->count--; return 0; }
static inline void up(struct semaphore *s) { s->count++; }

typedef struct { u8 b[16]; } guid_t;
static inline bool guid_equal(const guid_t *a, const guid_t *b) { return !memcmp(a, b, sizeof(*a)); }

struct device { int unused; };
struct device_driver { int unused; };
struct work_struct { int unused; };
struct workqueue_struct;
struct module;

/* evaluate the format arguments so they are checked, print nothing */
static inline void kshim_log(const char *fmt, ...) { (void)fmt; }
#define dev_dbg(dev, ...) kshim_log(__VA_ARGS__)
#define dev_warn(dev, ...) kshim_log(__VA_ARGS__)
#define dev_err(dev, ...) kshim_log(__VA_ARGS__)
#define pr_debug(...) kshim_log(__VA_ARGS__)
#define EXPORT_SYMBOL_GPL(sym)
