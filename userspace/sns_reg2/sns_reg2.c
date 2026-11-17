/*
 * Clean-Room SNS_REG2_SVC (0x10f, v2) QMI-over-QRTR Service for MSM8998 SLPI
 * Copyright (c) 2026, OnePlus 5T Mainline Project
 * License: GPL-2.0-only
 */
#include <errno.h>
#include <fcntl.h>
#include <libqrtr.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sns_reg2_tables.h"

#define SNS_REG2_QMI_SERVICE_ID   0x010f
#define SNS_REG2_QMI_VERSION      2
#define SNS_REG2_QMI_INSTANCE     0

#define QMI_MSG_TYPE_REQUEST      0x00
#define QMI_MSG_TYPE_RESPONSE     0x02

#define SNS_REG_CANCEL_REQ        0x0000
#define SNS_REG_VERSION_REQ       0x0001
#define SNS_REG_SINGLE_READ_REQ   0x0002
#define SNS_REG_SINGLE_WRITE_REQ  0x0003
#define SNS_REG_GROUP_READ_REQ    0x0004
#define SNS_REG_GROUP_WRITE_REQ   0x0005

#define SNS_REG_MAX_FILE_SIZE     25468
#define SNS_REG_INTERFACE_VER     0x2e
#define SNS_REG_MAX_MSG_ID        6

#ifndef SNS_REG2_SELFTEST
static volatile sig_atomic_t g_running = 1;

static void on_sig(int sig)
{
	(void)sig;
	g_running = 0;
}
#endif

static inline uint16_t get_le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void put_le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
}

static inline void put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff);
	p[3] = (uint8_t)((v >> 24) & 0xff);
}

static int cmp_item(const void *key, const void *elem)
{
	uint16_t id = *(const uint16_t *)key;
	const struct sns_reg_item_meta *m = elem;

	return (id > m->id) - (id < m->id);
}

static int cmp_group(const void *key, const void *elem)
{
	uint16_t id = *(const uint16_t *)key;
	const struct sns_reg_group_meta *m = elem;

	return (id > m->id) - (id < m->id);
}

static const uint8_t *find_tlv(const uint8_t *buf, uint16_t len, uint8_t tag,
			       uint16_t *out_len)
{
	uint16_t off = 0;

	while (off + 3 <= len) {
		uint8_t t = buf[off];
		uint16_t l = get_le16(buf + off + 1);
		off += 3;
		if (off + l > len)
			return NULL;
		if (t == tag) {
			*out_len = l;
			return buf + off;
		}
		off += l;
	}
	return NULL;
}

static uint16_t add_tlv(uint8_t *out, uint16_t pos, uint16_t cap, uint8_t tag,
			const void *data, uint16_t len)
{
	if (pos + 3 + len > cap)
		return pos;
	out[pos++] = tag;
	put_le16(out + pos, len);
	pos += 2;
	if (len && data)
		memcpy(out + pos, data, len);
	pos += len;
	return pos;
}

struct reg_ctx {
	uint8_t *buf;
	size_t size;
};

static int load_reg_file(struct reg_ctx *ctx, const char *path)
{
	static const char *const paths[] = {
		"/mnt/vendor/persist/sensors/sns.reg",
		"/persist/sensors/sns.reg",
	};
	int fd = -1;
	struct stat st;
	size_t alloc_sz = SNS_REG_MAX_FILE_SIZE;
	ssize_t n;

	if (path) {
		fd = open(path, O_RDONLY);
	} else {
		for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
			fd = open(paths[i], O_RDONLY);
			if (fd >= 0) {
				path = paths[i];
				break;
			}
		}
	}

	if (fd < 0) {
		fprintf(stderr, "[sns_reg2] warning: cannot open sns.reg (%s), using zero-filled shadow buffer\n",
			strerror(errno));
		ctx->buf = calloc(1, alloc_sz);
		ctx->size = ctx->buf ? alloc_sz : 0;
		return ctx->buf ? 0 : -ENOMEM;
	}

	if (fstat(fd, &st) == 0 && (size_t)st.st_size > alloc_sz)
		alloc_sz = (size_t)st.st_size;

	ctx->buf = calloc(1, alloc_sz);
	if (!ctx->buf) {
		close(fd);
		return -ENOMEM;
	}

	size_t total = 0;
	while (total < alloc_sz && (n = read(fd, ctx->buf + total, alloc_sz - total)) > 0)
		total += (size_t)n;
	close(fd);

	ctx->size = alloc_sz;
	printf("[sns_reg2] loaded %zu bytes from %s (shadow buf %zu bytes)\n",
	       total, path ? path : "(default)", ctx->size);
	fflush(stdout);
	return 0;
}

static size_t handle_qmi_request(struct reg_ctx *ctx, const uint8_t *in, size_t in_len,
				 uint8_t *out, size_t out_cap)
{
	if (in_len < 7)
		return 0;
	uint8_t type = in[0];
	uint16_t txn_id = get_le16(in + 1);
	uint16_t msg_id = get_le16(in + 3);
	uint16_t msg_len = get_le16(in + 5);

	if (type != QMI_MSG_TYPE_REQUEST || 7 + (size_t)msg_len > in_len)
		return 0;

	const uint8_t *pl = in + 7;
	uint8_t *resp_pl = out + 7;
	uint16_t cap = (out_cap > 7) ? (uint16_t)(out_cap - 7) : 0;
	uint16_t pos = 0;
	uint8_t common[2] = { 0, 0 };

	switch (msg_id) {
	case SNS_REG_CANCEL_REQ:
		pos = add_tlv(resp_pl, pos, cap, 0x02, common, 2);
		printf("[sns_reg2] CANCEL txn=%u -> ok\n", txn_id);
		break;

	case SNS_REG_VERSION_REQ: {
		uint8_t ver_le[4], max_id_le[2];
		put_le32(ver_le, SNS_REG_INTERFACE_VER);
		put_le16(max_id_le, SNS_REG_MAX_MSG_ID);
		pos = add_tlv(resp_pl, pos, cap, 0x02, common, 2);
		pos = add_tlv(resp_pl, pos, cap, 0x03, ver_le, 4);
		pos = add_tlv(resp_pl, pos, cap, 0x04, max_id_le, 2);
		printf("[sns_reg2] VERSION txn=%u -> ver=0x%x max_msg=%u\n",
		       txn_id, SNS_REG_INTERFACE_VER, SNS_REG_MAX_MSG_ID);
		break;
	}

	case SNS_REG_SINGLE_READ_REQ: {
		uint16_t tlen = 0, item_id = 0;
		const uint8_t *tv = find_tlv(pl, msg_len, 0x01, &tlen);
		uint8_t item_le[2];
		uint8_t data_tlv[9];
		uint16_t dlen = 1;

		if (!tv || tlen != 2) {
			common[0] = 1;
			common[1] = 4; /* MALFORMED / INVALID_ARG */
		} else {
			item_id = get_le16(tv);
			const struct sns_reg_item_meta *m = bsearch(
				&item_id, sns_reg_items,
				sizeof(sns_reg_items) / sizeof(sns_reg_items[0]),
				sizeof(sns_reg_items[0]), cmp_item);
			if (!m || m->size == 0 || m->size > 8 ||
			    (size_t)m->offset + m->size > ctx->size) {
				common[0] = 1;
				common[1] = 7; /* NOT_FOUND / INVALID_ID */
			} else {
				data_tlv[0] = (uint8_t)m->size;
				memcpy(data_tlv + 1, ctx->buf + m->offset, m->size);
				dlen = 1 + m->size;
			}
		}
		if (common[0] != 0)
			data_tlv[0] = 0;
		put_le16(item_le, item_id);
		pos = add_tlv(resp_pl, pos, cap, 0x02, common, 2);
		pos = add_tlv(resp_pl, pos, cap, 0x03, item_le, 2);
		pos = add_tlv(resp_pl, pos, cap, 0x04, data_tlv, dlen);
		printf("[sns_reg2] SINGLE_READ id=%u res=(%u,%u) sz=%u\n",
		       item_id, common[0], common[1], data_tlv[0]);
		break;
	}

	case SNS_REG_SINGLE_WRITE_REQ: {
		uint16_t tlen1 = 0, tlen2 = 0, item_id = 0;
		const uint8_t *tv1 = find_tlv(pl, msg_len, 0x01, &tlen1);
		const uint8_t *tv2 = find_tlv(pl, msg_len, 0x02, &tlen2);

		if (!tv1 || tlen1 != 2 || !tv2 || tlen2 < 1 || tlen2 != 1 + tv2[0]) {
			common[0] = 1;
			common[1] = 4;
		} else {
			item_id = get_le16(tv1);
			uint8_t dsz = tv2[0];
			const struct sns_reg_item_meta *m = bsearch(
				&item_id, sns_reg_items,
				sizeof(sns_reg_items) / sizeof(sns_reg_items[0]),
				sizeof(sns_reg_items[0]), cmp_item);
			if (!m || dsz != m->size ||
			    (size_t)m->offset + m->size > ctx->size) {
				common[0] = 1;
				common[1] = m ? 4 : 7;
			} else {
				memcpy(ctx->buf + m->offset, tv2 + 1, dsz);
			}
		}
		pos = add_tlv(resp_pl, pos, cap, 0x02, common, 2);
		printf("[sns_reg2] SINGLE_WRITE id=%u res=(%u,%u)\n",
		       item_id, common[0], common[1]);
		break;
	}

	case SNS_REG_GROUP_READ_REQ: {
		uint16_t tlen = 0, group_id = 0;
		const uint8_t *tv = find_tlv(pl, msg_len, 0x01, &tlen);
		uint8_t grp_le[2];
		uint8_t data_tlv[2 + 256];
		uint16_t dlen = 2;

		if (!tv || tlen != 2) {
			common[0] = 1;
			common[1] = 4;
		} else {
			group_id = get_le16(tv);
			const struct sns_reg_group_meta *m = bsearch(
				&group_id, sns_reg_groups,
				sizeof(sns_reg_groups) / sizeof(sns_reg_groups[0]),
				sizeof(sns_reg_groups[0]), cmp_group);
			if (!m || m->size > 256 ||
			    (size_t)m->offset + m->size > ctx->size) {
				common[0] = 1;
				common[1] = 7;
			} else {
				put_le16(data_tlv, m->size);
				memcpy(data_tlv + 2, ctx->buf + m->offset, m->size);
				dlen = 2 + m->size;
			}
		}
		if (common[0] != 0)
			put_le16(data_tlv, 0);
		put_le16(grp_le, group_id);
		pos = add_tlv(resp_pl, pos, cap, 0x02, common, 2);
		pos = add_tlv(resp_pl, pos, cap, 0x03, grp_le, 2);
		pos = add_tlv(resp_pl, pos, cap, 0x04, data_tlv, dlen);
		printf("[sns_reg2] GROUP_READ id=%u res=(%u,%u) sz=%u\n",
		       group_id, common[0], common[1], dlen - 2);
		break;
	}

	case SNS_REG_GROUP_WRITE_REQ: {
		uint16_t tlen1 = 0, tlen2 = 0, group_id = 0;
		const uint8_t *tv1 = find_tlv(pl, msg_len, 0x01, &tlen1);
		const uint8_t *tv2 = find_tlv(pl, msg_len, 0x02, &tlen2);

		if (!tv1 || tlen1 != 2 || !tv2 || tlen2 < 2) {
			common[0] = 1;
			common[1] = 4;
		} else {
			uint16_t dsz = get_le16(tv2);
			group_id = get_le16(tv1);
			if (tlen2 != 2 + dsz || dsz > 256) {
				common[0] = 1;
				common[1] = 4;
			} else {
				const struct sns_reg_group_meta *m = bsearch(
					&group_id, sns_reg_groups,
					sizeof(sns_reg_groups) / sizeof(sns_reg_groups[0]),
					sizeof(sns_reg_groups[0]), cmp_group);
				if (!m || dsz != m->size ||
				    (size_t)m->offset + m->size > ctx->size) {
					common[0] = 1;
					common[1] = m ? 4 : 7;
				} else {
					memcpy(ctx->buf + m->offset, tv2 + 2, dsz);
				}
			}
		}
		pos = add_tlv(resp_pl, pos, cap, 0x02, common, 2);
		printf("[sns_reg2] GROUP_WRITE id=%u res=(%u,%u)\n",
		       group_id, common[0], common[1]);
		break;
	}

	default:
		common[0] = 1;
		common[1] = 4;
		pos = add_tlv(resp_pl, pos, cap, 0x02, common, 2);
		printf("[sns_reg2] UNKNOWN msg_id=0x%04x txn=%u\n", msg_id, txn_id);
		break;
	}

	out[0] = QMI_MSG_TYPE_RESPONSE;
	put_le16(out + 1, txn_id);
	put_le16(out + 3, msg_id);
	put_le16(out + 5, pos);
	fflush(stdout);
	return 7 + pos;
}

#ifndef SNS_REG2_SELFTEST
int main(int argc, char **argv)
{
	struct reg_ctx ctx = { 0 };
	const char *reg_path = (argc > 1) ? argv[1] : NULL;
	uint8_t buf[2048];
	uint8_t resp[1024];
	int fd;

	signal(SIGINT, on_sig);
	signal(SIGTERM, on_sig);

	if (load_reg_file(&ctx, reg_path) < 0)
		return 1;

	fd = qrtr_open(0);
	if (fd < 0) {
		fprintf(stderr, "[sns_reg2] qrtr_open failed\n");
		free(ctx.buf);
		return 1;
	}

	if (qrtr_publish(fd, SNS_REG2_QMI_SERVICE_ID,
			 SNS_REG2_QMI_VERSION, SNS_REG2_QMI_INSTANCE) < 0) {
		fprintf(stderr, "[sns_reg2] qrtr_publish(0x%x, %u, %u) failed\n",
			SNS_REG2_QMI_SERVICE_ID, SNS_REG2_QMI_VERSION, SNS_REG2_QMI_INSTANCE);
		qrtr_close(fd);
		free(ctx.buf);
		return 1;
	}
	printf("[sns_reg2] published QMI service 0x%x ver=%u inst=%u\n",
	       SNS_REG2_QMI_SERVICE_ID, SNS_REG2_QMI_VERSION, SNS_REG2_QMI_INSTANCE);
	fflush(stdout);

	while (g_running) {
		struct sockaddr_qrtr sq;
		struct qrtr_packet pkt;
		socklen_t sl = sizeof(sq);
		int ret;

		ret = qrtr_poll(fd, 1000);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (ret == 0)
			continue;

		ret = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sq, &sl);
		if (ret < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			fprintf(stderr, "[sns_reg2] recvfrom failed: %s\n", strerror(errno));
			break;
		}

		if (qrtr_decode(&pkt, buf, (size_t)ret, &sq) < 0)
			continue;

		if (pkt.type == QRTR_TYPE_DATA) {
			size_t rlen = handle_qmi_request(&ctx, pkt.data, pkt.data_len,
							 resp, sizeof(resp));
			if (rlen > 0)
				qrtr_sendto(fd, pkt.node, pkt.port, resp, (unsigned int)rlen);
		}
	}

	qrtr_bye(fd, SNS_REG2_QMI_SERVICE_ID, SNS_REG2_QMI_VERSION, SNS_REG2_QMI_INSTANCE);
	qrtr_close(fd);
	free(ctx.buf);
	return 0;
}
#else
#include <assert.h>
int main(void)
{
	struct reg_ctx ctx = { 0 };
	uint8_t req[512], resp[512];
	size_t rlen;

	assert(load_reg_file(&ctx, "/home/zero/oneplus5t-android17/firmware_raw/persist/sensors/sns.reg") == 0);

	/* Test Version */
	memset(req, 0, sizeof(req));
	req[0] = 0x00; put_le16(req + 1, 1); put_le16(req + 3, 1); put_le16(req + 5, 0);
	rlen = handle_qmi_request(&ctx, req, 7, resp, sizeof(resp));
	assert(rlen == 7 + 17 && resp[0] == 0x02 && get_le16(resp + 5) == 17);

	/* Test Single Read item 0 (4 bytes at off 0) */
	req[0] = 0x00; put_le16(req + 1, 2); put_le16(req + 3, 2); put_le16(req + 5, 5);
	req[7] = 0x01; put_le16(req + 8, 2); put_le16(req + 10, 0);
	rlen = handle_qmi_request(&ctx, req, 12, resp, sizeof(resp));
	assert(rlen == 7 + 5 + 5 + 3 + 1 + 4 && resp[10] == 0 && resp[11] == 0);
	assert(memcmp(resp + 7 + 5 + 5 + 3 + 1, ctx.buf + 0, 4) == 0);

	/* Test Single Write item 0 + Read back */
	uint8_t val4[5] = { 4, 0x11, 0x22, 0x33, 0x44 };
	req[0] = 0x00; put_le16(req + 1, 3); put_le16(req + 3, 3); put_le16(req + 5, 13);
	req[7] = 0x01; put_le16(req + 8, 2); put_le16(req + 10, 0);
	req[12] = 0x02; put_le16(req + 13, 5); memcpy(req + 15, val4, 5);
	rlen = handle_qmi_request(&ctx, req, 20, resp, sizeof(resp));
	assert(rlen == 7 + 5 && resp[10] == 0 && resp[11] == 0);
	assert(memcmp(ctx.buf + 0, val4 + 1, 4) == 0);

	/* Test Group Read 1040 (128 bytes at off 256) */
	req[0] = 0x00; put_le16(req + 1, 4); put_le16(req + 3, 4); put_le16(req + 5, 5);
	req[7] = 0x01; put_le16(req + 8, 2); put_le16(req + 10, 1040);
	rlen = handle_qmi_request(&ctx, req, 12, resp, sizeof(resp));
	assert(rlen == 7 + 5 + 5 + 3 + 2 + 128 && resp[10] == 0 && resp[11] == 0);
	assert(get_le16(resp + 7 + 5 + 5 + 3) == 128);
	assert(memcmp(resp + 7 + 5 + 5 + 3 + 2, ctx.buf + 256, 128) == 0);

	/* Test Unknown Item Read -> result=1 err=7 */
	req[0] = 0x00; put_le16(req + 1, 5); put_le16(req + 3, 2); put_le16(req + 5, 5);
	req[7] = 0x01; put_le16(req + 8, 2); put_le16(req + 10, 65000);
	rlen = handle_qmi_request(&ctx, req, 12, resp, sizeof(resp));
	assert(resp[10] == 1 && resp[11] == 7);

	free(ctx.buf);
	printf("SELFTEST PASSED!\n");
	return 0;
}
#endif
