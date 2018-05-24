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
	uint16_t **entries; // malloc(4096*total_fat_blocks)?
  //we have an array of entries.
  //So each entry in the array is a FAT block
};

struct __attribute__((__packed__)) fat_blocks {
    // Fill this in!
	 // struct fat_block* arr_fat;
};

struct __attribute__((__packed__)) root {
	uint8_t *filename;
	uint32_t filesize;
	uint16_t first_db_index;
};

static struct superblock* sb = NULL;
static struct fat_block* fat_array = NULL;
// static struct fat_blocks* fat_array = NULL;
//static struct root* root = NULL;


int fs_mount(const char *diskname)
{
	sb = malloc(sizeof(struct superblock));
	if (block_disk_open(diskname) == -1) {
		return -1;
	}

	void *buf = malloc(BLOCK_SIZE);
	sb->signature = malloc(sizeof(uint8_t)*8); // 8 bytes allocated
	// memset(buf, 0, BLOCK_SIZE);
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
    // fat = malloc(sizeof(struct fat_block));
    // fat->entries = malloc(BLOCK_SIZE);

    printf("seg fault here?\n");
    fat_array = malloc(sizeof(struct fat_block));
    fat_array->entries = malloc(sb->total_fat_blocks);
    printf("seg fault here?asdfsd\n");

    for(int i = 0; i < sb->total_fat_blocks; i++){
      fat_array->entries[i] = malloc(BLOCK_SIZE);
    }



    //fat_array->entries = malloc(BLOCK_SIZE * sb->total_fat_blocks);

    int total_fat_counter = (int)sb->total_fat_blocks;
    size_t read_counter = 1;
    while (total_fat_counter != 0) {
        if (block_read(read_counter, fat_array->entries[read_counter-1])){
            return -1;
        }
        read_counter++;
        --total_fat_counter;
    }




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
    printf("fat_free_ratio=%d/%d\n", sb->total_data_blocks-1, sb->total_data_blocks);
    printf("rdir_free_ratio=%d/%d\n", 128,128); // consider not hardcoding

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
