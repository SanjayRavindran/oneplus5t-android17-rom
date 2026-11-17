#define _GNU_SOURCE
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <scsi/sg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/dma-heap.h>
#include <linux/qseecom.h>
#include <scsi/ufs/ioctl.h>

#define UPIU_QUERY_OPCODE_READ_DESC 0x1
#define QUERY_DESC_IDN_DEVICE       0x0
#define QUERY_DESC_IDN_UNIT         0x2
#define QUERY_DESC_IDN_GEOMETRY     0x7

static int read_file_into(const char *path, uint8_t *dst, size_t max_len, size_t *out_len)
{
	int fd = open(path, O_RDONLY);
	struct stat st;
	ssize_t n;
	size_t off = 0;

	if (fd < 0)
		return -errno;
	if (fstat(fd, &st) < 0) {
		int e = -errno;
		close(fd);
		return e;
	}
	if ((size_t)st.st_size > max_len) {
		close(fd);
		return -ENOSPC;
	}
	while (off < (size_t)st.st_size) {
		n = read(fd, dst + off, (size_t)st.st_size - off);
		if (n <= 0) {
			int e = (n < 0) ? -errno : -EIO;
			close(fd);
			return e;
		}
		off += (size_t)n;
	}
	close(fd);
	*out_len = off;
	return 0;
}

static void test_sg_ufs(void)
{
	int found_sg = 0;
	int query_ok = 0;

	printf("=== [1] Testing /dev/sg* + SG_IO + UFS_IOCTL_QUERY ===\n");
	for (int i = 0; i < 16; i++) {
		char dev[32];
		int fd;
		uint8_t cdb[6] = { 0x12, 0, 0, 0, 36, 0 }; /* INQUIRY */
		uint8_t inq[36] = { 0 };
		uint8_t sense[32] = { 0 };
		struct sg_io_hdr io_hdr;
		struct {
			struct ufs_ioctl_query_data hdr;
			uint8_t buf[256];
		} q;

		snprintf(dev, sizeof(dev), "/dev/sg%d", i);
		fd = open(dev, O_RDWR);
		if (fd < 0)
			continue;
		found_sg++;

		memset(&io_hdr, 0, sizeof(io_hdr));
		io_hdr.interface_id = 'S';
		io_hdr.cmd_len = sizeof(cdb);
		io_hdr.mx_sb_len = sizeof(sense);
		io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
		io_hdr.dxfer_len = sizeof(inq);
		io_hdr.dxferp = inq;
		io_hdr.cmdp = cdb;
		io_hdr.sbp = sense;
		io_hdr.timeout = 3000;

		int sgio_rc = ioctl(fd, SG_IO, &io_hdr);
		printf("  %s: SG_IO INQUIRY rc=%d status=0x%x vendor='%.8s' prod='%.16s'\n",
		       dev, sgio_rc, io_hdr.status,
		       (sgio_rc == 0) ? (char *)(inq + 8) : "?",
		       (sgio_rc == 0) ? (char *)(inq + 16) : "?");

		memset(&q, 0, sizeof(q));
		q.hdr.opcode = UPIU_QUERY_OPCODE_READ_DESC;
		q.hdr.idn = QUERY_DESC_IDN_GEOMETRY;
		q.hdr.buf_size = sizeof(q.buf);
		if (ioctl(fd, UFS_IOCTL_QUERY, &q) == 0) {
			query_ok++;
			printf("  %s: UFS_IOCTL_QUERY(GEOMETRY) OK buf_size=%u rpmb_rw_size=0x%02x (bytes[0..7]=%02x %02x %02x %02x %02x %02x %02x %02x)\n",
			       dev, q.hdr.buf_size, q.buf[0x17],
			       q.buf[0], q.buf[1], q.buf[2], q.buf[3],
			       q.buf[4], q.buf[5], q.buf[6], q.buf[7]);
		} else {
			printf("  %s: UFS_IOCTL_QUERY rc=-1 errno=%d (%s)\n",
			       dev, errno, strerror(errno));
		}
		close(fd);
	}
	printf("  Summary: found_sg=%d query_ok=%d\n", found_sg, query_ok);
}

static int test_load_ta(const char *fw_dir, const char *ta_name)
{
	char path[256];
	uint8_t mdt[65536];
	size_t mdt_len = 0, total_len = 0, seg_len = 0;
	uint8_t arch;
	uint16_t n_phdrs;
	int qfd, hfd, rc;

	printf("=== [3] Testing TA load via /dev/dma_heap/default_cma_region + /dev/qseecom (%s) ===\n", ta_name);
	snprintf(path, sizeof(path), "%s/%s.mdt", fw_dir, ta_name);
	rc = read_file_into(path, mdt, sizeof(mdt), &mdt_len);
	if (rc < 0) {
		printf("  SKIP: cannot read %s (%s)\n", path, strerror(-rc));
		return rc;
	}
	arch = mdt[EI_CLASS];
	if (arch == ELFCLASS32 && mdt_len >= sizeof(Elf32_Ehdr)) {
		n_phdrs = ((Elf32_Ehdr *)mdt)->e_phnum;
	} else if (arch == ELFCLASS64 && mdt_len >= sizeof(Elf64_Ehdr)) {
		n_phdrs = ((Elf64_Ehdr *)mdt)->e_phnum;
	} else {
		printf("  FAIL: bad ELF header in %s\n", path);
		return -EINVAL;
	}
	printf("  %s.mdt: mdt_len=%zu arch=%s n_phdrs=%u\n",
	       ta_name, mdt_len, (arch == ELFCLASS32) ? "ELF32" : "ELF64", n_phdrs);

	hfd = open("/dev/dma_heap/default_cma_region", O_RDWR);
	if (hfd < 0) {
		printf("  FAIL: open /dev/dma_heap/default_cma_region: %s\n", strerror(errno));
		return -errno;
	}

	struct dma_heap_allocation_data alloc = {
		.len = 2 * 1024 * 1024,
		.fd_flags = O_RDWR | O_CLOEXEC,
		.heap_flags = 0,
	};
	if (ioctl(hfd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
		printf("  FAIL: DMA_HEAP_IOCTL_ALLOC: %s\n", strerror(errno));
		close(hfd);
		return -errno;
	}
	uint8_t *map = mmap(NULL, alloc.len, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
	if (map == MAP_FAILED) {
		printf("  FAIL: mmap dmabuf fd=%u: %s\n", alloc.fd, strerror(errno));
		close(alloc.fd);
		close(hfd);
		return -errno;
	}

	memcpy(map, mdt, mdt_len);
	total_len = mdt_len;
	for (uint16_t i = 0; i < n_phdrs; i++) {
		snprintf(path, sizeof(path), "%s/%s.b%02u", fw_dir, ta_name, i);
		rc = read_file_into(path, map + total_len, alloc.len - total_len, &seg_len);
		if (rc < 0) {
			printf("  FAIL: read %s: %s\n", path, strerror(-rc));
			munmap(map, alloc.len);
			close(alloc.fd);
			close(hfd);
			return rc;
		}
		total_len += seg_len;
	}
	printf("  Staged %s image in DMA-BUF fd=%u total_len=%zu bytes\n", ta_name, alloc.fd, total_len);

	qfd = open("/dev/qseecom", O_RDWR);
	if (qfd < 0) {
		printf("  FAIL: open /dev/qseecom: %s\n", strerror(errno));
		munmap(map, alloc.len);
		close(alloc.fd);
		close(hfd);
		return -errno;
	}

	struct qseecom_load_img_req lreq = { 0 };
	lreq.mdt_len = (uint32_t)mdt_len;
	lreq.img_len = (uint32_t)total_len;
	lreq.ifd_data_fd = (int32_t)alloc.fd;
	snprintf(lreq.img_name, sizeof(lreq.img_name), "%s", ta_name);
	lreq.app_arch = arch;

	if (ioctl(qfd, QSEECOM_IOCTL_LOAD_APP_REQ, &lreq) < 0) {
		printf("  FAIL: QSEECOM_IOCTL_LOAD_APP_REQ(%s): %s\n", ta_name, strerror(errno));
		close(qfd);
		munmap(map, alloc.len);
		close(alloc.fd);
		close(hfd);
		return -errno;
	}
	printf("  PASS: Loaded %s! app_id=%u\n", ta_name, lreq.app_id);

	struct qseecom_qseos_app_load_query qreq = { 0 };
	snprintf(qreq.app_name, sizeof(qreq.app_name), "%s", ta_name);
	qreq.app_arch = arch;
	if (ioctl(qfd, QSEECOM_IOCTL_APP_LOADED_QUERY_REQ, &qreq) == 0) {
		printf("  PASS: APP_LOADED_QUERY_REQ(%s) confirmed app_id=%u\n", ta_name, qreq.app_id);
	} else {
		printf("  WARN: APP_LOADED_QUERY_REQ(%s): %s\n", ta_name, strerror(errno));
	}

	if (ioctl(qfd, QSEECOM_IOCTL_UNLOAD_APP_REQ) == 0) {
		printf("  PASS: QSEECOM_IOCTL_UNLOAD_APP_REQ(%s) OK\n", ta_name);
	} else {
		printf("  FAIL: UNLOAD_APP_REQ(%s): %s\n", ta_name, strerror(errno));
	}

	close(qfd);
	munmap(map, alloc.len);
	close(alloc.fd);
	close(hfd);
	return 0;
}

int main(int argc, char **argv)
{
	const char *fw_dir = (argc > 1) ? argv[1] : "/firmware_mnt/image";
	const char *ta_name = (argc > 2) ? argv[2] : "gfp5288";
	int qfd;

	test_sg_ufs();

	printf("=== [2] Testing /dev/qseecom version & loaded query ===\n");
	qfd = open("/dev/qseecom", O_RDWR);
	if (qfd < 0) {
		printf("  FAIL: open /dev/qseecom: %s\n", strerror(errno));
		return 1;
	}
	struct qseecom_qseos_version_req vreq = { 0 };
	if (ioctl(qfd, QSEECOM_IOCTL_GET_QSEOS_VERSION_REQ, &vreq) == 0)
		printf("  PASS: QSEOS version = 0x%08x\n", vreq.qseos_version);
	else
		printf("  FAIL: GET_QSEOS_VERSION: %s\n", strerror(errno));

	const char *probe_apps[] = { "keymaster", "keymaster64", "cmnlib", "nonexistent_ta" };
	for (size_t i = 0; i < sizeof(probe_apps) / sizeof(probe_apps[0]); i++) {
		struct qseecom_qseos_app_load_query qreq = { 0 };
		snprintf(qreq.app_name, sizeof(qreq.app_name), "%s", probe_apps[i]);
		int rc = ioctl(qfd, QSEECOM_IOCTL_APP_LOADED_QUERY_REQ, &qreq);
		printf("  QUERY('%s'): rc=%d errno=%d (%s) app_id=%u\n",
		       probe_apps[i], rc, rc < 0 ? errno : 0,
		       rc < 0 ? strerror(errno) : "OK", qreq.app_id);
	}
	close(qfd);

	return test_load_ta(fw_dir, ta_name) == 0 ? 0 : 1;
}
