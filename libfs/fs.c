#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

/* HELPER FUNCTION PROTOTYPES */
int file_search(const char* filename);
int get_root_entry(const char* filename);
int get_fd_table_index(int fd);

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

/*struct data_block {
    // we have an array of data blocks.
    // each data block holds file data

    uint8_t **db_entries;
}__attribute__((__packed__));*/

struct root {
    uint8_t filename[FS_FILENAME_LEN];
    uint32_t filesize;
    uint16_t first_db_index;
    uint8_t padding[10];
}__attribute__((__packed__));

struct fd {
    int id;
    //not unsigned, b/c we use -1 to describe fd that hasn't been opened yet
    int offset;
    int root_entry;
}__attribute__((__packed__));

static struct superblock* sb = NULL;
static struct fat_block* fat_array = NULL;
// static struct data_block* db_array = NULL;
static struct root* root_global = NULL;
static struct fd* fd_table = NULL;

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

/*    // begin assignment of data blocks to db_array
    db_array = malloc(sizeof(struct data_block));
    db_array->db_entries = malloc(sb->total_data_blocks);
    int total_db_counter = sb->total_data_blocks;
    for(int i = 0; i < sb->total_data_blocks; i++){
        db_array->db_entries[i] = malloc(BLOCK_SIZE);
    }
    // reset the read_counter to begin at first data block index
    read_counter = sb->data_block_index;
    size_t db_index_count = 0;
    while (total_db_counter != 0) {
        if (block_read(read_counter,
                       db_array->db_entries[db_index_count]) == -1) {
            return -1;
        }
        read_counter++;
        db_index_count++;
        --total_db_counter;
    }*/

    //Now we do the same thing for the root_global
    //almost exactly the same as what we did for the superblock
    // 32 bytes * 128 entries
    root_global = malloc(sizeof(struct root) * FS_FILE_MAX_COUNT);
    if (block_read((size_t)sb->root_dir_index, root_global) == -1) {
        return -1;
    }

    // finally, malloc space for the fd table
    // (maximum fd's it can hold at a time is 32)
    fd_table = malloc(sizeof(struct fd) * FS_OPEN_MAX_COUNT);
    for(int i = 0; i < FS_OPEN_MAX_COUNT; ++i){
        fd_table[i].id = -1; //we set all of them to -1, b/c none has been opened
        fd_table[i].offset = 0; //always initliazed as 0
    }
    return 0;
}

int fs_umount(void){
    // error check if there are still open file descriptors
    for (int i = 0; i < FS_OPEN_MAX_COUNT; ++i) {

        /**
         * below if is sort of pseudo-code for now:
         * figure out way to return error if the
         * loop detects an open file descriptor
         * */
/*        if (fd_table[i].id != 0) {
            return -1;
        }*/
    }

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
    free(fd_table);
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

    // get the actual amount of free fat blocks
    // by iterating through the fat_array and
    // incrementing the fat_occupied_count by 1
    // each time it encounters a zero entry
    int fat_free_count = 0;
    for (int i = 0; i < sb->total_fat_blocks; ++i) {
        for (int j = 0; j < 2048; ++j) { // 2048 entries per FAT block
            if (fat_array->entries[i][j] == 0) {
                ++fat_free_count;
            }
        }
    }

    // now calculate the amount of occupied root entries
    // by iterating through the root_global array and
    // incrementing occupied_root_count by 1 each time
    // it encounters a null character in each entry's
    // filename member.
    int root_entry_free_count = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (root_global[i].filename[0] == '\0') {
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
           fat_free_count, sb->total_data_blocks);

    printf("rdir_free_ratio=%d/%d\n",
           root_entry_free_count, FS_FILE_MAX_COUNT);

    return 0;
}

int fs_create(const char *filename)
{

    // error checking for invalid filename
    // we define "invalid" to be filenames with 0 bytes (empty)
    // or above the 16 bytes specified
    if (strlen(filename) > FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

    // check if filename is null-terminated
    if ( *(filename + strlen(filename) ) != '\0') {
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
        if (root_global[i].filename[0] != '\0') {
            ++file_counter;
        }
    }
    if (file_counter == 128) {
        return -1;
    }

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
    // Check if file name is invalid
    if (strlen(filename) > FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }
    // checks if filename is not found
    if (file_search(filename) != 0) {
        return -1;
    }

    // checking if filename is inside the filesystem.
    // if it isn't, return -1 (can't delete file that doesn't exist)
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strncmp((char *) root_global[i].filename,
                    filename, FS_FILENAME_LEN) == 0) {
            root_global[i].filename[0] = '\0';
            root_global[i].first_db_index = 0;
            root_global[i].filesize = 0;
            break;
        }
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

int fs_open(const char *filename) {
    // Check if file name is invalid
    if (strlen(filename) > FS_FILENAME_LEN || strlen(filename) == 0) {
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
    // out of bounds error. Potentially accounts for
    // fd arg that is not currently open
    // since the struct fd member id is assigned as
    // -1 by default.
    if (fd < 0) {
        return -1;
    }
    int fd_index = get_fd_table_index(fd);
    if (fd_index == -1) {
        return -1; // fd isn't open
    }
    fd_table[fd_index].id = -1;
    return 0;
}

int fs_stat(int fd)
{
    if (fd < 0) {
        return -1;
    }
    int fd_index = get_fd_table_index(fd);
    if (fd_index == -1) {
        return -1;
    }

    //we get our curernt root entry in our fd_table, and use that
    //to get the file size in our root_global array.
    return root_global[fd_table[fd_index].root_entry].filesize;
}

int fs_lseek(int fd, size_t offset){

    if (fd < 0) {
        return -1;
    }

    //if the fd index doesn't exist, return -1
    int fd_index = get_fd_table_index(fd);
    if(fd_index == -1){
        return -1;
    }

    //if offset is greater than the filesize, obviously an error
    //TODO: there should be other checks for valid offset.
    if(offset > root_global[fd_table[fd_index].root_entry].filesize){
        return -1;
    }

    fd_table[fd_index].offset = (int)offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
    printf("ohno\n");
    return 0;
}

int fs_read(int fd, void *buf, size_t count)
{
    //invalid fd
    if (fd < 0) {
        return -1;
    }
    //if the fd index doesn't exist, return -1
    int fd_index = get_fd_table_index(fd);
    if(fd_index == -1){
        return -1;
    }

    //We need to get the amount of data blocks with data
    //Which is the file size/4096 + 1 (b/c div truncates the result).
    uint32_t filesize = root_global[fd_table[fd_index].root_entry].filesize;
    int amnt_data_blocks = ((int)filesize/BLOCK_SIZE) + 1;
    void* bounce_buf = malloc(BLOCK_SIZE); //used to hold a temp data block.

    // db_index describes all the data block indices associated with
    // the file pointed to by fd. db_index will change values as it
    // is processed in the loop below.
    size_t db_index = root_global[fd_table[fd_index].root_entry].first_db_index;
    ++db_index; // because db counts up from 0
    db_index = db_index + sb->root_dir_index;

    //these two are if we read in multiple blocks.
    int buf_offset = 0;
    size_t multi_count = count;

    //There should be 3 overarching scenarios here. Each scenario also changes
    //if the offset of the file > 0.
    //count == size, count < size, count > size.
    //If count == size, we just memcpy everything.
    //if count > size, we only read up to size.
    //TODO: < and >.


    //Mainly here because it looks ugly to keep on copy/pasting it.
    size_t fd_offset = root_global[fd_table[fd_index].root_entry].offset;

    //Here is concerning if the file offset > 0
    //We get what block to start reading from, and where in the block to read.
    int block_offset = (int)fd_offset/BLOCK_SIZE;
    size_t byte_offset = fd_offset - (size_t)(block_offset*BLOCK_SIZE);
    int offset_data_block = amnt_data_blocks - block_offset;
    int offset_db_index = db_index + block_offset;
    //size_t multi_offset_cnt = count;


    while(offset_data_block != 0){
      if(block_read(offset_db_index, bounce_buf) == -1){
          return -1;
      }
      //Now we've offset'd all the blocks, we should offset bytes in the block.

      //This condition is if we just happen to read all that's left of one block
      if(count <= BLOCK_SIZE - byte_offset){
        memcpy(buf, bounce_buf + byte_offset, count);
        root_global[fd_table[fd_index].root_entry].offset += count;
        return (int)count;
      }
      else{

        //If we've offsetted into middle of block, finish reading that block
        if(byte_offset > 0){ //should only run thru this fxn ONCE at most.
          memcpy(buf, bounce_buf + byte_offset, byte_offset);
          buf_offset = buf_offset + byte_offset;
          multi_count = multi_count - byte_offset;
          offset_db_index++;
          byte_offset = 0; //safe to do this now, since we'll never use it again

          //if multi_count 0, we just return.
          if(multi_count == 0){
            root_global[fd_table[fd_index].root_entry].offset += multi_count;
            return (int)multi_count;
          }

        }

        //so if multi_count is still greater than a BLOCK_SIZE, we just memcpy
        //the whole block into buf.
        else if(multi_count > BLOCK_SIZE){
          memcpy(buf + buf_offset, bounce_buf, BLOCK_SIZE);
          buf_offset = buf_offset + BLOCK_SIZE;
          multi_count = multi_count - BLOCK_SIZE;
          offset_db_index++;
        }

        //multi_count less than BLOCK_SIZE
        //we read what's left of the block.
        else{
          memcpy(buf + buf_offset, bounce_buf, multi_count);
          buf_offset = buf_offset + multi_count;
          offset_db_index++;
        }
      }

      offset_data_block--;

      root_global[fd_table[fd_index].root_entry].offset += count;
      return (int)count;
    }


    //so we keep on reading in data blocks into bounce_buf
    //then we write the bounce_buf into buf, and then increment the offset for
    //buf.
    while(amnt_data_blocks != 0){
        if(block_read(db_index, bounce_buf) == -1){
            return -1;
        }
        //if count < or = BLOCK_SIZE, we just read in everything to buf.
        if(count <= BLOCK_SIZE){
            memcpy(buf, bounce_buf, count);
            //we modify the offset before returning.
            root_global[fd_table[fd_index].root_entry].offset += count;
            return (int)count;
        }
            //if we're reading in multiple blocks, we have to write to the offset of
            //buf each time. We still memcpy the BLOCK_SIZE every time.
        else{
            //So multicount is for when the amnt of bytes to be read is for example
            //4097, so we read in 4096, decrement multicount by 4096, then we
            //read in the last byte in the else statement.
            if(multi_count > BLOCK_SIZE){
                memcpy(buf+buf_offset, bounce_buf, BLOCK_SIZE);
                buf_offset = buf_offset + BLOCK_SIZE;
                multi_count = multi_count - BLOCK_SIZE;
                db_index++;
            }
            else{
                memcpy(buf+buf_offset, bounce_buf, multi_count);
                buf_offset = buf_offset + multi_count;
                db_index++;
            }

        }
        --amnt_data_blocks;
    }

    //we modify the offset before returning.
    root_global[fd_table[fd_index].root_entry].offset += count;
    return (int)count;
}

/* HELPER FUNCTIONS */

/**
 * file_search - find a file
 * @filename: File name
 *
 * Find a file named @filename that exists inside the root entries.
 *
 * Return: -1 if @filename was not found in the root entries.
 * Otherwise return 0 to indicate file was found.
 */
int file_search(const char* filename) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strncmp( (char*)root_global[i].filename,
                     filename, FS_FILENAME_LEN ) == 0) {
            return 0; // found a match
        }
    }
    return -1; // fail state: could not find file
}

int get_root_entry(const char* filename) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strncmp( (char*)root_global[i].filename,
                     filename, FS_FILENAME_LEN ) == 0) {
            return i; // found a match
        }
    }
    return -1; // fail state: could not find file
}

int get_fd_table_index(int fd) {
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (fd_table[i].id == fd) {
            return i; // found index
        }
    }
    return -1; // fail state: could not find opened fd
}
