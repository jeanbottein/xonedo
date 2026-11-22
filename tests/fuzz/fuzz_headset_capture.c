// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * libFuzzer harness for the headset's microphone copy into the ALSA ring
 * buffer (gip_headset_copy_capture(), taken verbatim from driver/headset.c,
 * see Makefile). The headset chooses the packet length; ALSA chooses the
 * buffer and period sizes. Input: 2 bytes buffer size, 2 bytes period size,
 * then packets as [len lo, len hi, bytes...].
 */
#include "kshim.h"

struct snd_pcm_runtime {
	unsigned char *dma_area;
	size_t buffer_bytes;
	size_t period_bytes;
};

struct snd_pcm_substream {
	struct snd_pcm_runtime *runtime;
};

struct gip_headset_stream {
	struct snd_pcm_substream *substream;
	size_t pointer;
	size_t period;
};

static size_t snd_pcm_lib_buffer_bytes(struct snd_pcm_substream *s) { return s->runtime->buffer_bytes; }
static size_t snd_pcm_lib_period_bytes(struct snd_pcm_substream *s) { return s->runtime->period_bytes; }

#include "gen_headset_capture.c"

int LLVMFuzzerTestOneInput(const u8 *data, size_t size)
{
	struct snd_pcm_runtime runtime;
	struct snd_pcm_substream sub = { .runtime = &runtime };
	struct gip_headset_stream stream = { .substream = &sub };
	size_t pos = 4;

	if (size < 4)
		return 0;

	/* like gip_headset_pcm_open(): at least one 16 kHz mono period (256 B) */
	runtime.period_bytes = 256 + (data[2] | data[3] << 8) % 1024;
	runtime.buffer_bytes = runtime.period_bytes * (1 + (data[0] | data[1] << 8) % 8);
	runtime.dma_area = malloc(runtime.buffer_bytes);

	while (pos + 2 <= size) {
		size_t len = data[pos] | data[pos + 1] << 8;
		u8 *pkt;

		pos += 2;
		if (len > size - pos)
			len = size - pos;
		pkt = malloc(len ? len : 1);
		memcpy(pkt, data + pos, len);
		pos += len;
		gip_headset_copy_capture(&stream, pkt, len);
		free(pkt);
	}

	free(runtime.dma_area);
	return 0;
}
