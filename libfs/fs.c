#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 65535

/* HELPER FUNCTION PROTOTYPES */
int file_search(const char* filename);
int get_root_entry(const char* filename);
int get_fd_table_index(int fd);
size_t get_and_set_fat(size_t first_db_num);
int set_multi_fat (size_t *first_db_num, size_t offset_data_block);
int get_next_fat(int updated_i, int updated_j);
struct superblock {
    uint8_t signature[8]; // ECS150FS
    uint16_t total_blocks;
    uint16_t root_dir_index;
    uint16_t data_block_index;
    uint16_t total_data_blocks;
    uint8_t total_fat_blocks;
    uint8_t padding[4079]; // to prevent malloc errors
}__attribute__((__packed__));

// we will have an array of FAT blocks.
// each FAT block has an array of entries
// (2048 entries for a total of 4096 bytes)
struct fat_block {
    uint16_t entries[2048];
}__attribute__((__packed__));


// we will have an array of root entries.
// each root entry itself has an array
// with indices representing the struct members.
struct root {
    uint8_t filename[FS_FILENAME_LEN];
    uint32_t filesize;
    uint16_t first_db_num;
    uint8_t padding[10]; // to prevent malloc issues
}__attribute__((__packed__));

struct fd {
    int id;
    int offset;
    int root_entry;
}__attribute__((__packed__));

// we will use this fact that sb is init as NULL
// to check in the other functions if the disk has
// been mounted or not. sb will only be NULL if
// fs_mount() hasn't been called yet.
static struct superblock* sb = NULL;
static struct fat_block* fat_array = NULL;
static struct root root_entries[FS_FILE_MAX_COUNT];// 128 for 1 root block
static struct fd fd_table[FS_OPEN_MAX_COUNT]; // maximum 32 fd's open at a time

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
    fat_array = malloc(sb->total_fat_blocks * sizeof(struct fat_block));

    // fat_array represents the array of fat blocks themselves
    // entries[i] however is the array of fat block entries,
    // per fat block.
    int total_fat_counter = (int)sb->total_fat_blocks;
    size_t read_counter = 1;

    while (total_fat_counter != 0) {
        if (block_read(read_counter,
                       fat_array[read_counter-1].entries) == -1) {
            return -1;
        }
        read_counter++;
        --total_fat_counter;
    }
    // making sure the first entry loaded was 0xFFFF
    if (fat_array[0].entries[0] != FAT_EOC) {
        return -1;
    }

    // Now we do the same thing for the root_entries
    // (32 bytes * 128 entries = 1 whole root block)
    if (block_read((size_t)sb->root_dir_index, root_entries) == -1) {
        return -1;
    }

    // finally, assign initial values for the fd table
    // (maximum fd's it can hold at a time is 32)
    for(int i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
        fd_table[i].id = -1; // set all to -1, b/c none has been opened
        fd_table[i].offset = 0; //always init as 0
    }
    return 0;
}

int fs_umount(void)
{
    // error check if no disk was mounted to begin with
    if (!sb) {
        return -1;
    }

    // error check if there are still open file descriptors
    for (int i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
        if (fd_table[i].id != -1) {
            return -1;
        }
    }

    // First one is always the superblock
    if (block_write(0, sb) == -1) {
        return -1;
    }
    // Next is the FAT blocks
    for (size_t i = 0; i < sb->total_fat_blocks; i++) {
        if(block_write((i+1), fat_array[i].entries) == -1) {
            return -1;
        }
    }
    // Afterwards is the root.
    if (block_write(sb->root_dir_index, root_entries) == -1) {
        return -1;
    }
    // We then close the disk
    if (block_disk_close() == -1) {
        return -1;
    }
    // Finally, free/wipe clean the globals
    free(sb);
    free(fat_array);
    memset(root_entries, 0, BLOCK_SIZE);
    memset(fd_table, 0, sizeof(struct fd)*FS_OPEN_MAX_COUNT);
    sb = NULL;
    fat_array = NULL;

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

    // get the actual amount of occupied fat blocks
    // by iterating through the fat_array and
    // incrementing the fat_occupied_count by 1
    // each time it encounters a non-zero entry
    int fat_occupied_count = 0;

    for (int i = 0; i < sb->total_fat_blocks; ++i) {
        for (int j = 0; j < 2048; ++j) { // 2048 entries per FAT block
            if (fat_array[i].entries[j] != 0) {
                ++fat_occupied_count;
            }
        }
    }

    // now calculate the amount of occupied root entries
    // by iterating through the root_entries array and
    // incrementing occupied_root_count by 1 each time
    // it encounters a null character in each entry's
    // filename member.
    int root_entry_free_count = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (root_entries[i].filename[0] == '\0') {
            ++root_entry_free_count;
        }
    }

    printf("FS Info:\n");
    printf("total_blk_count=%d\n", sb->total_blocks);
    printf("fat_blk_count=%d\n", sb->total_fat_blocks);
    printf("rdir_blk=%d\n", sb->root_dir_index);
    printf("data_blk=%d\n", sb->data_block_index);
    printf("data_blk_count=%d\n", sb->total_data_blocks);

    printf("fat_free_ratio=%d/%d\n",
           sb->total_data_blocks - fat_occupied_count, sb->total_data_blocks);

    printf("rdir_free_ratio=%d/%d\n",
           root_entry_free_count, FS_FILE_MAX_COUNT);

    return 0;
}

int fs_create(const char *filename)
{
    if (!sb) {
        return -1;
    }

    // error checking for invalid filename
    // we define "invalid" to be filenames with 0 bytes (empty)
    // or >= the 16 bytes specified
    if (strlen(filename) >= FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

    // check if filename is null-terminated
    if (!strchr(filename, '\0')) {
        return -1;
    }

    // going through root entries seeing if filename already exists
    // if so, return -1 since we don't want to create a filename
    // that already exists.
    if (file_search(filename) == 0) {
        return -1;
    }

    // error checking if all root entries are
    // already populated. i.e. 128 files present; no more can be added.
    int file_counter = 0; // temporary variable
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (root_entries[i].filename[0] != '\0') {
            ++file_counter;
        }
    }
    if (file_counter == 128) {
        return -1;
    }

    // find first occurrence of an empty root entry
    // (add file if first filename char is NULL char)
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (root_entries[i].filename[0] == '\0') {
            strcpy((char *)root_entries[i].filename, filename);
            root_entries[i].filesize = 0;
            root_entries[i].first_db_num = FAT_EOC; // fat_EOC
            break;
        }
    }
    return 0;
}

int fs_delete(const char *filename)
{

    if (!sb) {
        return -1;
    }
    // Check if file name is invalid
    if (strlen(filename) >= FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

    // check if filename is null-terminated
    if (!strchr(filename, '\0')) {
        return -1;
    }

    // checks if filename is not found
    if (file_search(filename) != 0) {
        return -1;
    }

    int entry = get_root_entry(filename);

    // we want a copy of the first db number
    // for freeing the fat entry.
    size_t first_db_num = root_entries[entry].first_db_num;
    size_t db_index = first_db_num;
    db_index += sb->root_dir_index;
    ++db_index; // skip over DB #0

    // freeing the associated fat entry/entries
    // by replacing them with a 0 value

    size_t first_db_num_cpy = first_db_num; // for the while loop below
    int fat_block_index = 0;
    if (first_db_num_cpy >= 2048) {
        while (first_db_num_cpy != 0 && first_db_num_cpy != FAT_EOC) {
            ++fat_block_index;
            first_db_num_cpy /= 2048;
        }
    }

    int root_index = get_root_entry(filename);
    int amt_data_blocks = root_entries[root_index].filesize / BLOCK_SIZE + 1;
    if (amt_data_blocks > 1) {
        // freeing multiple fat entries
        // by setting them to 0
        size_t next_entry = first_db_num;
        size_t cur_entry = next_entry;
        while (amt_data_blocks != 0) {
            next_entry = fat_array[fat_block_index].entries[cur_entry];
            fat_array[fat_block_index].entries[cur_entry] = 0;
            cur_entry = next_entry;
            amt_data_blocks--;
        }
    }
    else {
        // freeing a single fat entry pointing to
        // a single data block
        if (fat_array[fat_block_index].entries[first_db_num] == FAT_EOC) {
            fat_array[fat_block_index].entries[first_db_num] = 0;
        }

    }

    // freeing the root entry
    memset((char *)root_entries[entry].filename, 0, 16);
    root_entries[entry].filename[0] = '\0';
    root_entries[entry].first_db_num = 0;
    root_entries[entry].filesize = 0;

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
        if (root_entries[i].filename[0] != '\0') {
            printf("file: %s, size: %d, data_blk: %d\n",
                   root_entries[i].filename, root_entries[i].filesize,
                   root_entries[i].first_db_num);
        }
    }
    return 0;
}

int fs_open(const char *filename) {
    if (!sb) {
        return -1;
    }
    // Check if file name is invalid
    if (strlen(filename) > FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

    // check if filename is null-terminated
    if (!strchr(filename, '\0')) {
        return -1;
    }

    // check if filename exists. If it does not, return error.
    if (file_search(filename) != 0) {
        return -1;
    }

    //we find the first fd that is not open, and the set a new ID to it.
    for(int i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
        if (fd_table[i].id == -1) {
            fd_table[i].id = i;
            fd_table[i].root_entry = get_root_entry(filename);
            return fd_table[i].id;
        }
    }
    return -1;
}

int fs_close(int fd)
{
    if (!sb) {
        return -1;
    }
    // out of bounds error. Accounts for
    // fd arg that is not currently open
    // since the struct fd member id is
    // assigned as -1 by default.
    if (fd < 0) {
        return -1;
    }
    int fd_index = get_fd_table_index(fd);
    if (fd_index == -1) {
        return -1; // fd isn't open to begin with
    }
    fd_table[fd_index].id = -1;
    return 0;
}

int fs_stat(int fd)
{
    if (!sb) {
        return -1;
    }
    if (fd < 0) {
        return -1;
    }
    int fd_index = get_fd_table_index(fd);
    if (fd_index == -1) {
        return -1;
    }

    // we get our current root entry in our fd_table, and use that
    // to get the file size in our root_entries array.
    return root_entries[fd_table[fd_index].root_entry].filesize;
}

int fs_lseek(int fd, size_t offset)
{
    if (!sb) {
        return -1;
    }

    if (fd < 0) {
        return -1;
    }

    // if the fd index doesn't exist, return -1
    int fd_index = get_fd_table_index(fd);
    if(fd_index == -1) {
        return -1;
    }

    // if offset is greater than the filesize, obviously an error
    if(offset > root_entries[fd_table[fd_index].root_entry].filesize) {
        return -1;
    }

    fd_table[fd_index].offset = (int)offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
    if (!sb) {
        return -1;
    }

    // if the fd index doesn't exist, return -1
    int fd_index = get_fd_table_index(fd);
    if (fd_index == -1) {
        return -1;
    }

    if (count == 0) { // don't bother writing anything
        return 0;
    }

    root_entries[fd_table[fd_index].root_entry].filesize += (uint32_t)count;

    //amount of data we're attempting to write
    size_t amnt_data_blocks = (count / BLOCK_SIZE) + 1; // due to truncation
    size_t buf_offset = 0;
    size_t multi_count = count;
    void *bounce_buf = malloc(BLOCK_SIZE); //used to hold a temp data block.

    //we have these vars b/c ugly to keep writing them out.
    size_t first_db_num =
            root_entries[fd_table[fd_index].root_entry].first_db_num;
    size_t fd_offset = (size_t)fd_table[fd_index].offset;
    uint32_t filesize = root_entries[fd_table[fd_index].root_entry].filesize;

    size_t first_db_num_cpy = first_db_num; // for the while loop below
    int fat_block_index = 0;
    if (first_db_num_cpy >= 2048 && first_db_num_cpy != FAT_EOC) {
        while (first_db_num_cpy != 0) {
            ++fat_block_index;
            first_db_num_cpy /= 2048;
        }
    }

    size_t block_offset = fd_offset/BLOCK_SIZE;
    size_t byte_offset = fd_offset - (block_offset*BLOCK_SIZE);
    size_t offset_data_block = amnt_data_blocks - block_offset;

    size_t offset_db_index = block_offset;


    // array that holds where fat entries are.
    uint16_t fat_location[2048 * sb->total_fat_blocks];

    // free_fat represents the first-fit fat entry (or entries)
    // index (or indices) for the file being added, depending upon the
    // filesize. in practice, this returns the jth entry of the fat_array.
    size_t free_fat = 0;
    if (amnt_data_blocks > 1) {
        if (set_multi_fat(&first_db_num, offset_data_block) == -1) {
            return 0; // no more free fat entries to write to
        }
        root_entries[fd_table[fd_index].root_entry].first_db_num
                = (uint16_t)first_db_num;

        fat_location[0] = (uint16_t) first_db_num;
    }
    else {
        free_fat = get_and_set_fat(first_db_num);
        fat_location[0] = (uint16_t) free_fat;
    }

    // subsequent data blocks are simply
    // the value in that fat array location
    for(int i = 1; i < amnt_data_blocks; i++){
        // we get the index of what the current one is "pointing" to
        fat_location[i] = fat_array[fat_block_index].entries[fat_location[i-1]];
    }


    if (amnt_data_blocks == 1 && first_db_num == FAT_EOC) {
        if (free_fat == 0) {
            return 0; // cannot write; FAT is completely full
        }
        // db_index will be the index of the DB we are writing to
        size_t db_index = free_fat + sb->root_dir_index;
        ++db_index; // skip over DB #0
        // update the first data block num
        root_entries[fd_table[fd_index].root_entry].first_db_num
                = (uint16_t) free_fat;
        block_write(db_index, buf);
    }
    // so in here we don't need to create new entries in fat array
    else if (count + fd_offset <= filesize) {
        while (offset_data_block != 0) {
            // This condition is if we just happen to write
            // all that's left of one block
            if(count <= BLOCK_SIZE - byte_offset){
                // in the case we offset to the middle of the block, we gotta
                // block read, then memcpy it,
                // then block write back the whole block
                if (block_read(fat_location[offset_db_index]
                               + sb->root_dir_index + 1,
                               bounce_buf) == -1) {
                    return -1;
                }
                memcpy(bounce_buf + byte_offset, buf,
                       BLOCK_SIZE - byte_offset);
                block_write(fat_location[offset_db_index]
                            + sb->root_dir_index + 1, bounce_buf);
                fd_table[fd_index].offset += count;
                free(bounce_buf);
                return (int)count;
            }

            else {
                if(byte_offset > 0) {
                    // finish writing to the remaining block. we read a block,
                    // modify the block after the offset, then write to it
                    if (block_read(fat_location[offset_db_index]
                                   + sb->root_dir_index + 1,
                                   bounce_buf) == -1) {
                        return -1;
                    }
                    memcpy(bounce_buf + byte_offset, buf,
                           BLOCK_SIZE - byte_offset);
                    block_write(fat_location[offset_db_index]
                                + sb->root_dir_index + 1, bounce_buf);
                    buf_offset = buf_offset + (BLOCK_SIZE - byte_offset);
                    multi_count = multi_count - (BLOCK_SIZE - byte_offset);
                    offset_db_index++;
                    byte_offset = 0; //we'll never use this again
                    // if multi_count 0, we just return.
                    if (multi_count == 0) {
                        fd_table[fd_index].offset += multi_count;
                        free(bounce_buf);
                        return (int)multi_count;
                    }
                }
                    // In here, we just write a whole block
                else if (multi_count > BLOCK_SIZE) {
                    // we write bounce_buf to block_write
                    memcpy(bounce_buf, buf+buf_offset, BLOCK_SIZE);
                    block_write(fat_location[offset_db_index]
                                + sb->root_dir_index + 1, bounce_buf);
                    buf_offset = buf_offset + BLOCK_SIZE;
                    multi_count = multi_count - BLOCK_SIZE;
                    offset_db_index++;
                }
                else {
                    // we just write what we can now.
                    // so take the curernt block,
                    // change the first few values, then write it back
                    if (block_read(fat_location[offset_db_index]
                                   + sb->root_dir_index + 1,
                                   bounce_buf) == -1) {
                        return -1;
                    }
                    memcpy(bounce_buf, buf+buf_offset, multi_count);
                    block_write(fat_location[offset_db_index]
                                + sb->root_dir_index + 1, bounce_buf);
                    fd_table[fd_index].offset += count;
                    free(bounce_buf);
                    return (int) count;
                }
            }
            offset_data_block--;
        }
    }
    fd_table[fd_index].offset += count;
    return (int)count;
}

int fs_read(int fd, void *buf, size_t count)
{
    if (!sb) {
        return -1;
    }
    // invalid fd check
    if (fd < 0) {
        return -1;
    }
    // if the fd index doesn't exist, return -1
    int fd_index = get_fd_table_index(fd);
    if (fd_index == -1) {
        return -1;
    }

    // array that holds where fat blocks are.
    uint16_t fat_location[2048];


    // We need to get the amount of data blocks
    // associated with @filename pointed at by fd_index,
    // Which is the file size/4096 + 1 (b/c div truncates the result).
    // Ex. if @filename's filesize was 4097 bytes, then
    // amnt_data_blocks would be 2.

    // Use fd_offset and filesize variables
    // because it looks ugly to keep on copy/pasting the RHS.
    size_t fd_offset = (size_t)fd_table[fd_index].offset;
    uint32_t filesize = root_entries[fd_table[fd_index].root_entry].filesize;

    // amnt_data_blocks meaning, the number of data blocks
    // associated with the file pointed to by fd, depending on the filesize.
    size_t amnt_data_blocks = (filesize/BLOCK_SIZE) + 1;
    void *bounce_buf = malloc(BLOCK_SIZE); //used to hold a temp data block.
    memset(bounce_buf, 0, BLOCK_SIZE);
    uint32_t bytes_in_file = filesize;

    // db_index describes all the data block indices associated with
    // the file pointed to by fd. db_index will change values as it
    // is processed in the loops below. At declaration, db_index will
    // point to the very first data block associated with the file.
    size_t db_index
            = root_entries[fd_table[fd_index].root_entry].first_db_num;
    ++db_index; // because db counts up from 0
    db_index = db_index + sb->root_dir_index;

    // We now populate our fat_location array.
    size_t first_db_num
            = root_entries[fd_table[fd_index].root_entry].first_db_num;
    size_t first_db_num_cpy = first_db_num; // for the while loop below
    int fat_block_index = 0; //get which FAT block DB is located in

    // we use a cpy of first_db_num since we don't want to modify
    // first_db_num itself; we will use first_db_num later
    if (first_db_num_cpy >= 2048) {
        while (first_db_num_cpy != 0) {
            ++fat_block_index;
            first_db_num_cpy /= 2048;
        }
    }

    //this is how we get the first data block
    fat_location[0] = (uint16_t)first_db_num;

    // subsequent data blocks are simply
    // the value in that fat array location
    for(int i = 1; i < amnt_data_blocks; i++){
        // we get the index of what the current one is "pointing" to
        fat_location[i] = fat_array[fat_block_index].entries[fat_location[i-1]];
    }

    // these two are if we read in multiple blocks.
    size_t buf_offset = 0;
    size_t multi_count = count;

    // There should be 3 overarching scenarios here.
    // Each scenario also changes if the offset of the file > 0.
    // count == size, count < size, or count > size.
    // If count == size, we just memcpy everything.
    // if count > size, we only read up to size.

    if (filesize == fd_offset) {
        return 0;
    }

    // If the file offset > 0:
    // Calculate what block to start reading from, and where
    // in the block to read. Ex. if filesize was 10,000 bytes
    // and count = 2000, but the offset was 5000, then
    // block_offset would be 1 and byte_offset would be 4.
    size_t block_offset = fd_offset/BLOCK_SIZE;
    size_t byte_offset = fd_offset - (block_offset*BLOCK_SIZE);
    size_t offset_data_block = amnt_data_blocks - block_offset;
    size_t offset_db_index = block_offset;

    if (fd_offset != 0) {
        // we reset bytes_in_file to where it's offset to.
        // in other words, bytes_in_file from here on will
        // describe the remaining amount of bytes available
        // to read from the starting point fd_offset.
        bytes_in_file = bytes_in_file - (uint32_t)fd_offset;

        while (offset_data_block != 0) {
            if (buf_offset == count) {
                break;
            }

            if (block_read(fat_location[offset_db_index]
                           + sb->root_dir_index + 1,
                           bounce_buf) == -1) {
                return -1;
            }
            // Now that we have offset'd all the blocks,
            // we should offset bytes in the block.

            // This condition is if we just happen to read
            // all that's left of one block
            if (count <= BLOCK_SIZE - byte_offset) {
                if (bytes_in_file > count) {
                    memcpy(buf + buf_offset,
                           bounce_buf + byte_offset,multi_count);
                    fd_table[fd_index].offset += count;
                    free(bounce_buf);
                    return (int)count;
                }
                else {
                    memcpy(buf, bounce_buf + byte_offset, bytes_in_file);
                    fd_table[fd_index].offset += bytes_in_file;
                    free(bounce_buf);
                    return (int)bytes_in_file;
                }
            }
            else {
                // If we've offsetted into middle of block,
                // finish reading that block
                if (byte_offset > 0) { //should only run this fxn at most ONCE.
                    memcpy(buf, bounce_buf + byte_offset,
                           (BLOCK_SIZE- byte_offset));
                    buf_offset = buf_offset + (BLOCK_SIZE - byte_offset);
                    multi_count = multi_count - (BLOCK_SIZE - byte_offset);
                    offset_db_index++;
                    byte_offset = 0;
                    //safe to do this now, since we'll never use it again

                    // if multi_count 0, we just return.
                    if (multi_count == 0) {
                        fd_table[fd_index].offset += multi_count;
                        free(bounce_buf);
                        return (int)multi_count;
                    }
                }
                    // so if multi_count is still greater than BLOCK_SIZE,
                    // we just memcpy the whole block into buf.
                else if(multi_count > BLOCK_SIZE) {
                    memcpy(buf + buf_offset, bounce_buf, BLOCK_SIZE);
                    buf_offset = buf_offset + BLOCK_SIZE;
                    multi_count = multi_count - BLOCK_SIZE;
                    offset_db_index++;
                }
                    // multi_count less than BLOCK_SIZE
                    // we read what's left of the block.
                else {
                    memcpy(buf + buf_offset, bounce_buf, multi_count);
                    fd_table[fd_index].offset += count;
                    free(bounce_buf);
                    return (int)count;
                }
            }
            offset_data_block--;
        } // end of while loop
        if (fd_offset + count > filesize) {
            fd_table[fd_index].offset += bytes_in_file;
            free(bounce_buf);
            return (int)bytes_in_file;
        }
        else {
            fd_table[fd_index].offset += count;
            free(bounce_buf);
            return (int)count;
        }
    } // end of if

    // so we keep on reading in data blocks into bounce_buf
    // then we write the bounce_buf into buf,
    // and then increment the offset for buf.
    int iterator = 0; //used for iterating thru the fat_location
    while (amnt_data_blocks != 0) {
        if (buf_offset == count) {
            break;
        }

        if (block_read((fat_location[iterator] + sb->root_dir_index + 1),
                       bounce_buf) == -1) {
            free(bounce_buf);
            return -1;
        }
        // if count < or = BLOCK_SIZE, we just read in everything to buf.
        if (count <= BLOCK_SIZE) {
            if (count > bytes_in_file) {
                // here is if count > file size.
                memcpy(buf, bounce_buf, bytes_in_file);
                fd_table[fd_index].offset +=  bytes_in_file;
                free(bounce_buf);
                return (int) bytes_in_file;
            }
            else {
                memcpy(buf, bounce_buf, count);
                //we modify the offset before returning.
                fd_table[fd_index].offset += count;
                free(bounce_buf);
                return (int)count;
            }
        }
        // if we're reading in multiple blocks, we write to the offset of
        // buf each time. We still memcpy the BLOCK_SIZE every time.
        else {
            // So multicount is for when the amnt of bytes
            // to be read is for example 4097, so we read in 4096,
            // decrement multicount by 4096, then we read in the last
            // byte in the else statement.
            if (multi_count > BLOCK_SIZE) {
                if (multi_count > bytes_in_file
                    && bytes_in_file < BLOCK_SIZE) {
                    // here, we've read the maximum amount of blocks
                    // we then read what we can of the last block,
                    // and then finally return.
                    memcpy(buf+buf_offset, bounce_buf, bytes_in_file);
                    buf_offset = buf_offset + bytes_in_file;
                    fd_table[fd_index].offset += filesize;
                    free(bounce_buf);
                    return (int)filesize;
                }
                else {
                    memcpy(buf+buf_offset, bounce_buf, BLOCK_SIZE);
                    buf_offset = buf_offset + BLOCK_SIZE;
                    multi_count = multi_count - BLOCK_SIZE;
                    bytes_in_file = bytes_in_file - BLOCK_SIZE;
                    db_index++;
                }
            }
            else {
                // There are basically two scenarios here.
                // if multi count is the limiter,
                // or if filesize is the limiter.
                if (multi_count < bytes_in_file) {
                    memcpy(buf+buf_offset, bounce_buf, multi_count);
                    buf_offset = buf_offset + multi_count;
                    db_index++;
                }
                else if (bytes_in_file <= multi_count) {
                    memcpy(buf+buf_offset, bounce_buf, bytes_in_file);
                    fd_table[fd_index].offset += filesize;
                    free(bounce_buf);
                    return (int)filesize;
                }
            }
        }
        --amnt_data_blocks;
        iterator++;
    } // end of while loop

    // we modify the offset before returning.
    fd_table[fd_index].offset += count;
    free(bounce_buf);
    return (int)count;
}

/* HELPER FUNCTIONS */

 // Find a file named filename that exists inside the root entries.
 // Return: -1 if filename was not found in the root entries.
 // Otherwise return 0 to indicate file was found.
int file_search(const char* filename) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strncmp( (char*)root_entries[i].filename,
                     filename, FS_FILENAME_LEN ) == 0) {
            return 0; // found a match
        }
    }
    return -1; // fail state: could not find file
}

 // Find a file named @filename that exists inside the root entries.
 // Return: -1 if @filename was not found in the root entries.
 // Otherwise return the root entry index of where @filename
 // was located in.
int get_root_entry(const char* filename) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strncmp( (char*)root_entries[i].filename,
                     filename, FS_FILENAME_LEN ) == 0) {
            return i; // found a match
        }
    }
    return -1; // fail state: could not find file
}



 // Find a file descriptor named @fd that exists inside
 // the fd table array.

 // Return: -1 if @fd was not found in the fd table.
 // Otherwise return the index of where @fd was
 // found in the fd table.
int get_fd_table_index(int fd) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (fd_table[i].id == fd) {
            return i; // found index
        }
    }
    return -1; // fail state: could not find opened fd
}

// sets the FAT entry for a file that is only 1 data block big.
// returns 0 if it reaches the complete end of the double for loops,
// meaning no more free fat entries available.
size_t get_and_set_fat(size_t first_db_num) {
    for (int i = 0; i < sb->total_fat_blocks; ++i) {
        for (int j = 0; j < 2048; ++j) { // 2048 entries per FAT block

            if (first_db_num != FAT_EOC
                && fat_array[i].entries[first_db_num] == FAT_EOC) {
                return first_db_num;
            }

            if (fat_array[i].entries[j] == first_db_num
                    && first_db_num != FAT_EOC) {
                return first_db_num;
            }

            if (fat_array[i].entries[j] == 0) {
                // assign the file less than 4096 bytes
                // to a singular, proper FAT entry, and set that
                // to 0xFFFF.
                fat_array[i].entries[j] = FAT_EOC;
                return (size_t)j;
            }
        } // end of j loop
    } // end of i loop

    return 0; // no free fat_entries available
    // => no free data blocks available.
}

// if the file has n data blocks associated,
// we use this function to assign n FAT entries
int set_multi_fat (size_t *first_db_num, size_t offset_data_block) {

    int free_entry_check = 0;
    for (int i = 0; i < sb->total_fat_blocks; ++i) {
        if (offset_data_block == 0) {
            break;
        }

        for (int j = 0; j < 2048; ++j) {
            if (fat_array[i].entries[j] == 0) {
                // case where we have contiguous free FAT entries
                if (fat_array[i].entries[j + 1] == 0) {
                    fat_array[i].entries[j] = (uint16_t) (j+1);

                    // means first_db_num hasn't been set yet
                    if (*first_db_num == FAT_EOC) {
                        // only enters here once
                        *first_db_num = (size_t)j;
                    }

                    offset_data_block--;
                }
                else {
                    // non-contiguous case
                    // need some helper function to iterate from
                    // where we are now until it finds the next
                    // free fat entry. Then store this in free_entry_check.
                    free_entry_check = get_next_fat(i, j+1);
                    if (free_entry_check == -1) {
                        return -1; // no more free entries
                    }
                    fat_array[i].entries[j] = (uint16_t)free_entry_check;

                    // means first_db_num hasn't been set yet
                    if (*first_db_num == FAT_EOC) {
                        // only enters here once
                        *first_db_num = (size_t)j;
                    }

                    // update j so we don't iterate through unnecessary
                    // entries (any entry before get_next_fat() )
                    j = fat_array[i].entries[j] - 1;
                    --offset_data_block;
                }
            }
            // the last entry should be set to 0xFFFF
            if (offset_data_block == 0) {
                fat_array[i].entries[j] = FAT_EOC;
                break;
            }
        } // j loop end
    } // i loop end
    return 0; // success condition
}

// finds the next free fat entry. The first
// one the function finds, it returns.
int get_next_fat(int updated_i, int updated_j) {
    for (int i = updated_i; i < sb->total_fat_blocks; ++i) {
        for (int j = updated_j; j < 2048; ++j) {
            if (fat_array[i].entries[j] == 0) {
                return (uint16_t)j;
            }
        }
    }
    return -1; // no free fat_entries available
    // => no free data blocks available.
}
