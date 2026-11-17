/*
 * Clean-Room AF_ALG test specifically targeting Qualcomm QCE drivers
 * (cbc-aes-qce + sha256-qce) on >= 4096-byte payloads (above aes_sw_max_len=512).
 */
#include <errno.h>
#include <linux/if_alg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef SOL_ALG
#define SOL_ALG 279
#endif

static int test_aes_cbc_qce(void)
{
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "skcipher",
		.salg_name = "cbc-aes-qce",
	};
	int tfm = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfm < 0) { perror("socket skcipher"); return -1; }
	if (bind(tfm, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind cbc-aes-qce");
		close(tfm);
		return -1;
	}
	uint8_t key[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
	if (setsockopt(tfm, SOL_ALG, ALG_SET_KEY, key, sizeof(key)) < 0) {
		perror("ALG_SET_KEY");
		close(tfm);
		return -1;
	}

	uint8_t plain[4096], ct[4096], dec[4096];
	for (size_t i = 0; i < sizeof(plain); i++)
		plain[i] = (uint8_t)(i * 37 + 11);

	int op = accept(tfm, NULL, 0);
	if (op < 0) { perror("accept enc"); close(tfm); return -1; }

	char cbuf[CMSG_SPACE(4) + CMSG_SPACE(20)] = {0};
	struct af_alg_iv *iv;
	struct iovec iov = { .iov_base = plain, .iov_len = sizeof(plain) };
	struct msghdr msg = { .msg_iov = &iov, .msg_iovlen = 1, .msg_control = cbuf, .msg_controllen = sizeof(cbuf) };
	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	cmsg->cmsg_len = CMSG_LEN(4);
	*(uint32_t *)CMSG_DATA(cmsg) = ALG_OP_ENCRYPT;
	cmsg = CMSG_NXTHDR(&msg, cmsg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_IV;
	cmsg->cmsg_len = CMSG_LEN(20);
	iv = (struct af_alg_iv *)CMSG_DATA(cmsg);
	iv->ivlen = 16;
	memset(iv->iv, 0x5a, 16);

	if (sendmsg(op, &msg, 0) != sizeof(plain) || read(op, ct, sizeof(ct)) != sizeof(ct)) {
		perror("encrypt");
		close(op); close(tfm);
		return -1;
	}
	close(op);

	if (memcmp(plain, ct, sizeof(plain)) == 0) {
		fprintf(stderr, "QCE AES-CBC ciphertext equals plaintext!\n");
		close(tfm);
		return -1;
	}

	op = accept(tfm, NULL, 0);
	if (op < 0) { perror("accept dec"); close(tfm); return -1; }
	memset(cbuf, 0, sizeof(cbuf));
	iov.iov_base = ct;
	msg.msg_controllen = sizeof(cbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_OP;
	cmsg->cmsg_len = CMSG_LEN(4);
	*(uint32_t *)CMSG_DATA(cmsg) = ALG_OP_DECRYPT;
	cmsg = CMSG_NXTHDR(&msg, cmsg);
	cmsg->cmsg_level = SOL_ALG;
	cmsg->cmsg_type = ALG_SET_IV;
	cmsg->cmsg_len = CMSG_LEN(20);
	iv = (struct af_alg_iv *)CMSG_DATA(cmsg);
	iv->ivlen = 16;
	memset(iv->iv, 0x5a, 16);

	if (sendmsg(op, &msg, 0) != sizeof(ct) || read(op, dec, sizeof(dec)) != sizeof(dec)) {
		perror("decrypt");
		close(op); close(tfm);
		return -1;
	}
	close(op);
	close(tfm);

	if (memcmp(plain, dec, sizeof(plain)) != 0) {
		fprintf(stderr, "QCE AES-CBC roundtrip mismatch!\n");
		return -1;
	}
	printf("QCE cbc-aes-qce 4096B encrypt+decrypt PASS\n");
	return 0;
}

static int test_sha256_qce(void)
{
	/* NIST ABC test + 4096B zero buffer (e3b0c4... is empty, SHA256(4096 x 0x00) = ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7) */
	struct sockaddr_alg sa = {
		.salg_family = AF_ALG,
		.salg_type = "hash",
		.salg_name = "sha256-qce",
	};
	int tfm = socket(AF_ALG, SOCK_SEQPACKET, 0);
	if (tfm < 0) { perror("socket hash"); return -1; }
	if (bind(tfm, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("bind sha256-qce");
		close(tfm);
		return -1;
	}
	int op = accept(tfm, NULL, 0);
	if (op < 0) { perror("accept hash"); close(tfm); return -1; }

	uint8_t buf[4096] = {0};
	uint8_t digest[32] = {0};
	static const uint8_t expected[32] = {
		0xad,0x7f,0xac,0xb2,0x58,0x6f,0xc6,0xe9,
		0x66,0xc0,0x04,0xd7,0xd1,0xd1,0x6b,0x02,
		0x4f,0x58,0x05,0xff,0x7c,0xb4,0x7c,0x7a,
		0x85,0xda,0xbd,0x8b,0x48,0x89,0x2c,0xa7,
	};
	if (write(op, buf, sizeof(buf)) != sizeof(buf) || read(op, digest, sizeof(digest)) != sizeof(digest)) {
		perror("sha256-qce rw");
		close(op); close(tfm);
		return -1;
	}
	close(op);
	close(tfm);
	if (memcmp(digest, expected, 32) != 0) {
		fprintf(stderr, "QCE sha256-qce mismatch!\n");
		return -1;
	}
	printf("QCE sha256-qce 4096B NIST vector PASS\n");
	return 0;
}

int main(void)
{
	int r1 = test_aes_cbc_qce();
	int r2 = test_sha256_qce();
	return (r1 == 0 && r2 == 0) ? 0 : 1;
}
