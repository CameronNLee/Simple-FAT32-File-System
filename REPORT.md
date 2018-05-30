# P4 REPORT

## Phase 1

For this phase, we began by implementing ***fs_mount()*** using four defined
structs: **superblock**, **fat_block**, **root** and **fd**. All four more or
less contain member variables that map to the specifications described in
phase 0. Note that the root struct describes a single root entry inside the
root block. The **fd** struct contains members describing a file's file desc.
id, its offset value, and its root entry index. Through this struct, we were
able to connect these attributes to the file's root entry attributes.

Using these structs, we declared corresponding global variables to ensure
program-wide access to these data types: **sb**, **fat_array**, **fd_table**,
and **root_entries**. **sb**'s declaration being NULL is special in that we use
this to determine if the program is mounted or not for error-checking. Example:
if ***fs_mount()*** mounts the disk, then this will be shown via **sb** having
a non-NULL value. We also declared **fat_array** as well as **root_entries** as
pointer structs to allow iterating through/accessing the data types stored.

With all that being said, we then load in the mounted disk's blocks into the
corresponding struct variables via ***block_read()***. To ensure correctness,
we go through several error checks: whether the signature reads as ECS150FS,
whether the first fat entry is indeed 0xFFFF, if the number of data blocks in
**sb** matches with the output of ***block_count()***, etc. **fd_table**
prepares itself by setting all of its **id** entries to be equal to $-1$,
indicating $32$ maximum available file descriptors upon mounting.

***fs_mount()*** does the opposite: before closing the mounted disk via
***block_disk_close()***, we load in the data held inside the global structs
via ***block_write()*** based on information held within **sb**. We then use
***free()*** on **sb** and **fat_array**. **root_entries** and **fd_table**
were statically allocated memory, so we "free" the data via ***memset()***'ing
the variables to be filled with $0$ entries.

For ***fs_info()***, we calculate the amount of occupied entries in the FAT 
blocks by means of iterating through **fat_array** and incrementing a counter
variable each time it encounters a non-zero entry. By subtracting this from the
total data blocks (held inside **sb**), we were able to calculate the correct
fat free ratio. A similar approach was followed for calculating the root ratio.
Finally, we print the required information, much of it dependent upon **sb**'s
member variables.

## Phase 2

***fs_create()***, much like the the other functions throughout phase 2 and
phase 3, makes two checks involving the **filename** variable: whether it
is too long in length (> 16 bytes), and if it is not null-terminated. It also
makes an additional check making sure if the filename already exists in one
of the root entries. If so, we return -1, since we do not want to create file
duplicates. This check is achieved via a helper function ***file_search()***,
which simply takes in a char pointer (in this case, filename) and iterates
through the entries. If a match between filenames was found, then return -1.

Before adding the file, we do another check to see if all root entries are
occupied by other files. If not, then go ahead and iterate through the root
entries until the first open entry is encountered, then set the filename, the
filesize = 0, and the first data block number associated = 0xFFFF. This ensures
that we don't have to scan all the way through the entire root entries, as we
simply break out of iterating as soon as we find an empty space.

For ***fs_delete()***, we use a helper function ***get_root_entry()*** that
accepts a char pointer as an arg (i.e. filename). This function works almost
exactly the same as ***file_search()***: the only difference is that we return
the root index indicating where filename is stored upon successful match. If
no match occurs, then we return -1, as we don't delete what doesn't exist in
the filesystem to begin with. The returned root index is stored in a variable
called **entry**. From here on, we divide the process of deletion into two
separate cases:

One case is if the amount of data blocks associated with **filename** is equal
to exactly 1 (i.e. filesize is less than or equal to **BLOCK_SIZE**). Here, we
malloc and memset a void pointer buffer called **empty** to be filled with
zero values, then write to the sole data block index associated with filename.
Hence, this is how we "free" the data block: by simply wiping its byte values
to zero. 

To "free" the fat entry, we pass in the data block index value to the fat array
entries index. If we get a match that equals 0xFFFF, then we set the entry
value to 0. Finally, we "free" the root entry associated by passing in the
variable **entry** as an index for **root_entries**, and setting the members
to 0.

The other case is <finish here>.

***fs_ls()*** was relatively simple, where we just iterate through the root
entries. We only print the relevant information if the root entry index's
filename does NOT have its first character be the null character, indicating a
free space. This way, we only print root entries that contain files, both
empty and non-empty.

## Phase 3

In ***fs_open()***, we begin populating **fd_table** entries with meaningful
information. We do this by iterating through **fd_table**. Upon first finding
an id value of -1, indicating an assignable file descriptor, we then assign
the fd_table entry an id an a filename, and finally return the id we assigned.
We also needed to handle an error case where all file descriptor entries are
already being used. In that case, we made it so that if no assignment took
place, then simply return -1.

To implement ***fs_close()***, we utilized a helper function called
***get_fd_table_index()*** that is intended to have a fd argument passed into
it. It then iterates through **fd_table**, and if a match is found with the
fd argument passed in, it then returns the index of where fd was located
(otherwise it simply returns -1 indicating that fd was not found). We then
take the successful match, pass it as an index to **fd_table**, and set that
entry's id variable to -1 to indicate closure.

Similarly, in ***fs_stat()***, we grabbed the fd index. This is important, as
it allows us to access that particular fd index's root entry member variable
held within **fd_table**. We then pass the value returned by this member
variable as an index for **root_entries**, and then return the filesize member
variable. This is why we linked the fd table to the root entry index: it
allows us to access the various attributes of a file purely through the fd
table.



## Phase 4

todo

## Testing

todo

## Summary

hmm

## Sources: 

add here
