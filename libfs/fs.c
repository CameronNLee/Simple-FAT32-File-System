#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

struct superblock {
	uint8_t signature[8]; // ECS150FS
	uint16_t total_blocks;
	uint16_t root_dir_index;
	uint16_t data_block_index;
	uint16_t total_data_blocks;
	uint8_t total_fat_blocks;
	uint8_t padding[4079];
}__attribute__((__packed__));

struct fat_block {
	// we have an array of FAT blocks.
	// each FAT block has an array of entries
	// (2048 entries for a total of 4096 bytes)
	uint16_t **entries;
}__attribute__((__packed__));

struct root {
	uint8_t filename[FS_FILENAME_LEN];
	uint32_t filesize;
	uint16_t first_db_index;
	uint8_t padding[10];
}__attribute__((__packed__));

struct file {
	int id;
    int file_offset;
    int permission;
	int root_entry;
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
	if (block_read(0, sb) == -1) {
		return -1;
	}
	// testing for matching signature
	if (strncmp((char *)sb->signature, "ECS150FS", 8) != 0) {
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
		if (block_read(read_counter,
                       fat_array->entries[read_counter-1]) == -1) {
			return -1;
		}
		read_counter++;
		--total_fat_counter;
	}
	//Now we do the same thing for the root_global
	//almost exactly the same as what we did for the superblock
	// 32 bytes * 128 entries
	root_global = malloc(sizeof(struct root) * FS_FILE_MAX_COUNT);
	if (block_read((size_t)sb->root_dir_index, root_global) == -1) {
		return -1;
	}

	return 0;
}

int fs_umount(void){
	//First one is always the superblock
	if (block_write(0, sb) == -1) {
		return -1;
	}
	//Next is the FAT blocks
	for(size_t i = 0; i < sb->total_fat_blocks; i++){
		if(block_write((i+1), fat_array->entries[i]) == -1){
			return -1;
		}
	}
	//Afterwards is the root.
	if(block_write(sb->root_dir_index, root_global) == -1){
		return -1;
	}
	//We then finally close it
	if(block_disk_close() == -1){
		return -1;
	}
	// Free the globals
	free(sb);
	free(root_global);
	free(fat_array);
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
    // error checking if all root entries are
    // already populated. i.e. 128 files present; no more can be added.
	int file_counter = 0; // temporary variable
	for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (root_global[i].filename[0] != '\0') {
			++file_counter;
		}
	}
	if (file_counter == 128) {
		return -1;
	}

    // error checking for invalid filename
    // we define "invalid" to be filenames with 0 bytes (empty)
    // or above the 16 bytes specified
	if (strlen(filename) > FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

    // going through root entries seeing if filename already exists
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strncmp( (char*)root_global[i].filename,
                    filename, FS_FILENAME_LEN ) == 0) {
            return -1;
        }
    } // end of error checks

    // find first occurrence of an empty root entry
    // (add file if first filename char is NULL char)
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (root_global[i].filename[0] == '\0') {
            strcpy((char *)root_global[i].filename, filename);
            root_global[i].filesize = 0;
            root_global[i].first_db_index = 65535; // fat_EOC
            break; // don't
        }
    }

	return 0;
}

int fs_delete(const char *filename)
{

	bool filename_exists = 0;
    // Check if file name is invalid
    if (strlen(filename) > FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

	// checking if filename is inside the filesystem.
	// if it isn't, return -1 (can't delete file that doesn't exist)
	for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (strncmp((char *) root_global[i].filename,
					filename, FS_FILENAME_LEN) == 0) {
			filename_exists = 1;
			root_global[i].filename[0] = '\0';
			root_global[i].first_db_index = 0;
			root_global[i].filesize = 0;
			break;
		}
	}

	if (!filename_exists) {
		return -1; // checks for if filename is not found
	}
    // TODO free the data and free the FAT

	return 0;
}

int fs_ls(void)
{
	// sb being NULL implies nothing was mounted,
	// since sb gets populated in fs_mount()
	if (!sb) {
		return -1;
	}
	printf("FS Ls:\n");
	for (size_t i = 0; i < FS_FILE_MAX_COUNT; ++i) {
		if (root_global[i].filename[0] != '\0') {
			printf("file: %s, size: %d, data_blk: %d\n",
				   root_global[i].filename, root_global[i].filesize,
				   root_global[i].first_db_index);
		}
	}
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
