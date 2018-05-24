#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "disk.h"
#include "fs.h"

struct __attribute__((__packed__)) superblock {
    uint64_t signature; // ECS150FS
	uint16_t total_blocks;
	uint16_t root_dir_index;
	uint16_t data_block_index;
	uint16_t total_data_blocks;
	uint8_t total_fat_blocks;
};

struct __attribute__((__packed__)) fat {
	uint16_t *entry; // malloc(4096)?
};

struct __attribute__((__packed__)) root {
	uint8_t *filename;
	uint32_t filesize;
	uint16_t first_db_index;
};

static struct superblock* sb;

int fs_mount(const char *diskname)
{
	sb = malloc(sizeof(struct superblock));
	if (block_disk_open(diskname) == -1) {
		return -1;
	}

	void *buf = malloc(BLOCK_SIZE);
	// memset(buf, 0, BLOCK_SIZE);
	if (block_read(0, buf) == -1) {
		return -1;
	}
	memcpy(&sb->signature, buf, 8);
	memcpy(&sb->total_blocks, (buf+8), 2);
	memcpy(&sb->root_dir_index, (buf+10), 2);
	memcpy(&sb->data_block_index, (buf+12), 2);
	memcpy(&sb->total_data_blocks, (buf+14), 2);
	memcpy(&sb->total_fat_blocks, (buf+15), 1);

	free(buf);
	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	return 0;
}

