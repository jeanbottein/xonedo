// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * libFuzzer harness for the GIP parser (bus/protocol.c), compiled in
 * userspace with ASan/UBSan. Each input is a script of operations:
 *
 *   op % 4 == 0|1  len, bytes...  a radio frame for gip_process_buffer()
 *   op % 4 == 2    lo, hi         the host sends a (possibly chunked) packet
 *   op % 4 == 3                   pending clients get their driver bound
 *
 * A removed client is freed at once, i.e. as if its unregister work ran
 * right away on another CPU: the worst case for use-after-free.
 */
#include "kshim.h"
#include "../../bus/bus.h"

#define FUZZ_BUF_LEN 1556 /* tailroom of xone_dongle_get_buffer() */

static struct gip_adapter adapter;
static bool pending_bind[GIP_MAX_CLIENTS];
static volatile u8 sink;

static void consume(const void *data, u32 len)
{
	const u8 *p = data;

	for (u32 i = 0; i < len; i++)
		sink ^= p[i];
}

static int drv_consume(struct gip_client *c, void *d, u32 len) { (void)c; consume(d, len); return 0; }
static int drv_battery(struct gip_client *c, enum gip_battery_type t, enum gip_battery_level l) { (void)c; (void)t; (void)l; return 0; }
static int drv_simple(struct gip_client *c) { (void)c; return 0; }
static int drv_guide(struct gip_client *c, bool down) { (void)c; (void)down; return 0; }
static int drv_volume(struct gip_client *c, u8 in, u8 out, bool m) { (void)c; (void)in; (void)out; (void)m; return 0; }

static struct gip_driver fake_driver = {
	.ops = {
		.battery = drv_battery,
		.authenticate = drv_consume,
		.authenticated = drv_simple,
		.guide_button = drv_guide,
		.audio_ready = drv_simple,
		.audio_volume = drv_volume,
		.hid_report = drv_consume,
		.input = drv_consume,
		.firmware = drv_consume,
		.audio_samples = drv_consume,
	},
};

static int op_get_buffer(struct gip_adapter *a, struct gip_adapter_buffer *b)
{
	(void)a;
	b->data = malloc(FUZZ_BUF_LEN);
	b->context = b->data;
	b->length = FUZZ_BUF_LEN;
	return 0;
}

static int op_submit_buffer(struct gip_adapter *a, struct gip_adapter_buffer *b)
{
	(void)a;
	consume(b->data, b->length);
	free(b->context);
	return 0;
}

static int op_key(struct gip_adapter *a, u8 *k, int len) { (void)a; consume(k, len); return 0; }
static int op_simple(struct gip_adapter *a) { (void)a; return 0; }
static int op_audio_out(struct gip_adapter *a, int len) { (void)a; (void)len; return 0; }

static struct gip_adapter_ops ops = {
	.get_buffer = op_get_buffer,
	.submit_buffer = op_submit_buffer,
	.set_encryption_key = op_key,
	.enable_audio = op_simple,
	.init_audio_in = op_simple,
	.init_audio_out = op_audio_out,
	.disable_audio = op_simple,
};

/* bus.c stand-ins: same behaviour, minus the device model */
static void free_client(struct gip_client *client)
{
	gip_free_client_info(client);
	kfree(client->chunk_buf_out);
	kfree(client->chunk_buf_in);
	kfree(client);
}

struct gip_client *gip_get_client(struct gip_adapter *adap, u8 id)
{
	struct gip_client *client = adap->clients[id];

	if (client)
		return client;

	client = kzalloc(sizeof(*client), GFP_ATOMIC);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->id = id;
	client->adapter = adap;
	sema_init(&client->drv_lock, 1);
	spin_lock_init(&client->chunk_in_lock);
	adap->clients[id] = client;

	return client;
}

void gip_add_client(struct gip_client *client)
{
	pending_bind[client->id] = true;
}

void gip_remove_client(struct gip_client *client)
{
	client->adapter->clients[client->id] = NULL;
	pending_bind[client->id] = false;
	free_client(client);
}

/* gip_free_client_info() comes verbatim from bus/bus.c, see Makefile */
#include "gen_free_client_info.c"

int LLVMFuzzerTestOneInput(const u8 *data, size_t size)
{
	size_t pos = 0;

	memset(&adapter, 0, sizeof(adapter));
	memset(pending_bind, 0, sizeof(pending_bind));
	adapter.ops = &ops;
	adapter.audio_packet_count = 1;

	while (pos < size) {
		u8 op = data[pos++];

		switch (op % 4) {
		case 0:
		case 1: {
			size_t len;
			u8 *frame;

			if (pos >= size)
				break;
			len = data[pos++];
			if (len > size - pos)
				len = size - pos;
			/* exact-size copy so ASan catches any over-read */
			frame = malloc(len ? len : 1);
			memcpy(frame, data + pos, len);
			pos += len;
			gip_process_buffer(&adapter, frame, len);
			free(frame);
			break;
		}
		case 2: {
			struct gip_client *client = adapter.clients[(op >> 4) & 0xf];
			u8 payload[512];
			u32 len;

			if (pos + 2 > size)
				break;
			len = (data[pos] | data[pos + 1] << 8) % sizeof(payload);
			pos += 2;
			if (!client)
				break;
			memset(payload, op, len);
			gip_send_authenticate(client, payload, len, op & 0x80);
			break;
		}
		case 3:
			for (int i = 0; i < GIP_MAX_CLIENTS; i++) {
				if (pending_bind[i] && adapter.clients[i])
					adapter.clients[i]->drv = &fake_driver;
				pending_bind[i] = false;
			}
			break;
		}
	}

	for (int i = 0; i < GIP_MAX_CLIENTS; i++)
		if (adapter.clients[i])
			free_client(adapter.clients[i]);

	return 0;
}
