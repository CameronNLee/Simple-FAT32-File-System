#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "disk.h"
#include "fs.h"

struct __attribute__((__packed__)) superblock {
	uint8_t *signature; // ECS150FS
	uint16_t total_blocks;
	uint16_t root_dir_index;
	uint16_t data_block_index;
	uint16_t total_data_blocks;
	uint8_t total_fat_blocks;
};

struct __attribute__((__packed__)) fat_block {
	uint16_t **entries;
	// we have an array of FAT blocks.
	// each FAT block has an array of entries
	// (2048 entries for a total of 4096 bytes)
};

struct __attribute__((__packed__)) root {
	uint8_t **root_entries;
};

static struct superblock* sb = NULL;
static struct fat_block* fat_array = NULL;
static struct root* root_global = NULL;


int fs_mount(const char *diskname)
{
	sb = malloc(sizeof(struct superblock));
	if (block_disk_open(diskname) == -1) {
		return -1;
	}
	void *buf;
	buf = malloc(BLOCK_SIZE);
	sb->signature = malloc(sizeof(uint8_t)*8); // 8 bytes allocated
	if (block_read(0, buf) == -1) {
		return -1;
	}
	memcpy(sb->signature, buf, 8);
	memcpy(&sb->total_blocks, (buf+8), 2);
	memcpy(&sb->root_dir_index, (buf+10), 2);
	memcpy(&sb->data_block_index, (buf+12), 2);
	memcpy(&sb->total_data_blocks, (buf+14), 2);
	memcpy(&sb->total_fat_blocks, (buf+16), 1);

	// testing for matching signature
	if (strcmp((char *)sb->signature, "ECS150FS") != 0) {
		return -1;
	}

	// testing for matching block count
	if (block_disk_count() != sb->total_blocks) {
		return -1;
	}

	// begin loading metadata for the fat struct

	fat_array = malloc(sizeof(struct fat_block));
	fat_array->entries = malloc(sb->total_fat_blocks);

	// entries represents the array of fat blocks themselves
	// entries[i] however is the array of fat block entries,
	// per fat block. Hence, malloc 4096 bytes for
	// each i index in entries[i].
	for(int i = 0; i < sb->total_fat_blocks; i++){
		fat_array->entries[i] = malloc(BLOCK_SIZE);
	}


	int total_fat_counter = (int)sb->total_fat_blocks;
	size_t read_counter = 1;

	while (total_fat_counter != 0) {
		if (block_read(read_counter, fat_array->entries[read_counter-1]) == -1) {
			return -1;
		}
		read_counter++;
		--total_fat_counter;
	}

	//Now we do the same thing for the root_global
	//almost exactly the same as what we did for the superblock
    root_global = malloc(sizeof(struct root));
    root_global->root_entries = malloc(sizeof(uint8_t)*FS_FILE_MAX_COUNT); // 128 entries
    for (int i = 0; i < 128; ++i) {
        root_global->root_entries[i] = malloc(sizeof(uint8_t)*FS_OPEN_MAX_COUNT); // 32 bytes per entry
    }

    if(block_read((size_t)sb->root_dir_index-1, root_global->root_entries) == -1){
        return -1;
    }


/*	memcpy(root_global->filename, buf, 16);
	memcpy(&root_global->filesize, (buf+16), 4);
	memcpy(&root_global->first_db_index, (buf+20), 2);*/
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
	// sb being NULL means it never changed.
	// if sb never changed, then no virtual disk
	// was opened in the first place.
	if (!sb) {
		return -1;
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", sb->total_blocks);
	printf("fat_blk_count=%d\n", sb->total_fat_blocks);
	printf("rdir_blk=%d\n", sb->root_dir_index);
	printf("data_blk=%d\n", sb->data_block_index);
	printf("data_blk_count=%d\n", sb->total_data_blocks);

	printf("fat_free_ratio=%d/%d\n",
		   sb->total_data_blocks-1, sb->total_data_blocks);

	printf("rdir_free_ratio=%d/%d\n",
		   FS_FILE_MAX_COUNT, FS_FILE_MAX_COUNT); // consider not hardcoding

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
