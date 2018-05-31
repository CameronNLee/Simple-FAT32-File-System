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
is too long in length (>= 16 bytes), and if it is not null-terminated. It also
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
"free" a single fat entry by passing in the data block index value to the fat
array entry index. If we get a match that equals 0xFFFF, then we set the entry
value to 0.

The other case is if the amount of data blocks associated with **filename** is
greater than 1. If so, then we "walk" through the entries and set the values
held to 0, thus "freeing" them. This process is held inside a while loop that
runs until **amt_data_blocks** becomes 0 (we decrement it each iteration). This
way, the program knows when to stop freeing. For example, if the amount of
data blocks was 3, the program then knows it will free 3 fat entries, no more,
no less.

Finally, we "free" the root entry associated by passing in the
variable **entry** as an index for **root_entries**, and setting the members
to 0.

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

To manually set the offset in ***fs_lseek()***, we first make a check if the
offset is greater than the filesize tied to the **fd** argument variable. If
it isn't, then we assign the **offset** arg variable to **fd**'s offset member
variable located in **fd_table**.

## Phase 4

In ***fs_read()***, we start by populating an array, called **fat_location**, 
with where the datablock locations of the file are. We use this to as a guide to
which blocks we should read in. We have a **block_offset** and **byte_offset**
that calculates our offset in terms of blocks and bytes, rather than just bytes.
Our functions then splits off into two if statements, if there's an offset, and 
if there's no offset. 

In both situations, we have a **bounce_buf** variable to hold the current data
block, in which we then read the necessary amount from it into **buf**. Also, 
we have **buf_offset**, which is the offset that we write to the **buf**. This
is used when we read through multiple blocks.

If there is an offset, we have a while loop, whose condition is that the total 
amount of data blocks (**offset_data_block**) isn't equal to 0. The 
**offset_data_block** gets decremented by the **block_offset**, because we want
to skip the blocks we've already offsetted. We start reading from 
**fat_location[block_offset]**,which is the amount of blocks we offsetted. 
Then, if **count** is less than the difference of the block size and the byte 
offset, we're reading all that's left of the block, so we call **memcpy()**, and 
copy **bounce_buf** into **buf** There are two scenarios here, either the 
**count** is the limiting number, or the **bytes_in_file**, which is the file 
size, is the limiting number. We return whatever amount we wrote, and increment 
the file's offset by the same amount.
           
The other case is if we read more than one block. Here, we divided it into two 
cases. If we offsetted into the middle of a block (ie if **byte_offset** > 0), 
we finish reading that block, using the same logic as before. This time, since 
we do not return, we have two variables, in which we decrement: **multi_count**, 
which keeps track of how much count we still have to read, and increment 
**buf_offset**. Then we set **byte_offset** to 0, ensuring we won't trigger this 
if statement again. We have one condition, that is if our **multi_count** goes 
to 0. We just return the **multi_count**, free **bounce_buf**, and increment the 
file's offset by the **multi_count**.

Now, there's still two cases left, if the **multi_count** is still greater than
the block size, we just keep on writing to buf, incrementing and decrementing 
**buf_offset** and **multi_count** by the block size. Once **multi_count** is
less than the block size, that means we just read up to the middle of the block,
we do one last read, reading up to **multi_count** instead of the block size,
and then we return the count, as well as free the **bounce_buf**. We then 
decrement the **offset_data_block** by one.

If the offset is zero, the process is very similiar, and much simpler. Instead
of using **block_offset** to iterate through our array of FAT locations, we just
have an iterator initialized to 0, and then increment it by 1 each time in the
while loop. The rest of the while loop is nearly identical, with some variable 
names being the main difference.

***fs_write()** was very similiar to the read part, a lot of the variable names
were the same, and represented the same things; the main difference was that we
had to update the FAT table's entries each time we create a new file. To do so,
we had two functions, one that was used for when we needed to create one entry,
***get_and_set_fat()***, the other, ***set_multi_fat()*** was used when we had 
to set more than one entry in the FAT table. In ***set_multi_fat()***, we

TODO: EXPLAIN SET MULTI FAT 

In ***get_and_set_fat()***, we iterated through our FAT entries, and then once 
we found an empty entry, we set that entry to **FAT_EOC**, and then we returned 
the index of what we set. If we didn't find anything, we return 0. 

Next, we populated **fat_location** with the indexes of the entries of the FAT, 
the same way as we did in read. We then 

TODO: Expalin our amnt data blocks check


Our next scenario is writing to more than one data block. In here, our code is
extremely similiar to the one in read. All of the conditional checks are the 
same, as well as the way we iterate through our **fat_location**, and the 
variable names and how we increment/decrement them are the same as well.The only
different way was that we wrote instead of read. We first had a **bounce_buf**
that held in a whole block that we read to it with **block_read**. We'd then 
modify that bounce_buf by using ***memcpy()***, to copy our **buf** into it, and
offsetting **buf** if necessary. We then finally used **block_write** to write 
**bounce_buf** into the appropriate block in our file.

## Testing

todo

## Summary
We finished all parts of our project, with Phase 4 being signifigantly harder
than Phases 1-3. The hardest part in this project was accounting for all the 
different scenarios that could occur in both read and write. Everytime we 
thought we were finished with read, we discovered a new way that could break our
function, and then we had to fix that bug. This took up a majority of our time,
and the same was true for the write portion.

Overall, we feel that this project was a really good way to learn more about how
file systems work, and we felt that we've grown as programmers throught this
class as well. There's been a noticeable improvement from where we couldn't 
finish Project 1, to where we now managed to finish Project 4, which the 
Professor described as the hardest project for this class.

## Sources: 

add here
