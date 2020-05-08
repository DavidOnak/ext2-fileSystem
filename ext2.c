#include "ext2.h"

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fsuid.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>

#define EXT2_OFFSET_SUPERBLOCK 1024
#define EXT2_INVALID_BLOCK_NUMBER ((uint32_t) -1)

/* open_volume_file: Opens the specified file and reads the initial
   EXT2 data contained in the file, including the boot sector, file
   allocation table and root directory.

   Parameters:
     filename: Name of the file containing the volume data.
   Returns:
     A pointer to a newly allocated volume_t data structure with all
     fields initialized according to the data in the volume file
     (including superblock and group descriptor table), or NULL if the
     file is invalid or data is missing, or if the file is not an EXT2
     volume file system (s_magic does not contain the correct value).
 */
volume_t* open_volume_file(const char* filename) {

    printf("About to open\n");

    // make new pointer
    volume_t* volume = malloc(sizeof(volume_t));
    volume->fd = open(filename, O_RDONLY);
    printf("We opened\n");

    if (volume->fd < 0) {
        return NULL;
    }

    superblock_t* newSuperBlock = malloc(sizeof(superblock_t));

    // read 204 bytes (superblock) into buffer at offest 1024 from the start of file
    int superRead = pread(volume->fd, newSuperBlock, 204, 1024);
    if (superRead != 204) {
        return NULL;
    }

    volume->super = *newSuperBlock;

    // add other components to volume
    volume->block_size = 1024 << newSuperBlock->s_log_block_size;
    volume->volume_size = volume->block_size * newSuperBlock->s_blocks_count;
    volume->num_groups = (newSuperBlock->s_blocks_count / newSuperBlock->s_blocks_per_group) +
        (newSuperBlock->s_blocks_count % newSuperBlock->s_blocks_per_group != 0);

    group_desc_t* newGroupDesc = malloc(sizeof(group_desc_t));
    int groupRead = pread(volume->fd, newGroupDesc, 32, 1024 + volume->block_size);
    if (groupRead != 32) {
        return NULL;
    }

    volume->groups = newGroupDesc;

    free(newGroupDesc);
    free(newSuperBlock);
    return volume; // need to add a return null case
}

/* close_volume_file: Frees and closes all resources used by a EXT2 volume.

   Parameters:
     volume: pointer to volume to be freed.
 */
void close_volume_file(volume_t* volume) {
	
      // closes the file
    close(volume->fd);

    // free pointer in volume then the pointer itself
    free(volume->groups);
    free(volume);
    volume = NULL;
}

/* read_block: Reads data from one or more blocks. Saves the resulting
   data in buffer 'buffer'. This function also supports sparse data,
   where a block number equal to 0 sets the value of the corresponding
   buffer to all zeros without reading a block from the volume.

   Parameters:
     volume: pointer to volume.
     block_no: Block number where start of data is located.
     offset: Offset from beginning of the block to start reading
             from. May be larger than a block size.
     size: Number of bytes to read. May be larger than a block size.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_block(volume_t* volume, uint32_t block_no, uint32_t offset, uint32_t size, void* buffer) {

    return pread(volume->fd, buffer, size, (volume->block_size * block_no) + offset);
}

/* read_inode: Fills an inode data structure with the data from one
   inode in disk. Determines the block group number and index within
   the group from the inode number, then reads the inode from the
   inode table in the corresponding group. Saves the inode data in
   buffer 'buffer'.

   Parameters:
     volume: pointer to volume.
     inode_no: Number of the inode to read from disk.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns a positive value. In case of error,
     returns -1.
 */
ssize_t read_inode(volume_t* volume, uint32_t inode_no, inode_t* buffer) {

    // determine block group number and index
    int blockGroup = (inode_no - 1) / volume->super.s_inodes_per_group;
    int inodeIndex = (inode_no - 1) % volume->super.s_inodes_per_group;

    return read_block(volume, volume->groups[blockGroup].bg_inode_table, inodeIndex * volume->super.s_inode_size,
        volume->super.s_inode_size, buffer);
}

/* read_ind_block_entry: Reads one entry from an indirect
   block. Returns the block number found in the corresponding entry.

   Parameters:
     volume: pointer to volume.
     ind_block_no: Block number for indirect block.
     index: Index of the entry to read from indirect block.

   Returns:
     In case of success, returns the block number found at the
     corresponding entry. In case of error, returns
     EXT2_INVALID_BLOCK_NUMBER.
 */
/*static*/uint32_t read_ind_block_entry(volume_t* volume, uint32_t ind_block_no,
    uint32_t index) {

    // MAKE SURE TO MAKE STATIC AGAIN AFTER TESTING !!!!
    //     !!!!!!!!
    // !!!!!!!!!!!!!!!!!
    //     !!!!!!!!

    uint32_t blockNum;
    read_block(volume, ind_block_no, index * 4, 4, &blockNum);
    if (blockNum < 0) {
        return EXT2_INVALID_BLOCK_NUMBER;
    }
    else {
        return blockNum;
    }
}

/* read_inode_block_no: Returns the block number containing the data
   associated to a particular index. For indices 0-11, returns the
   direct block number; for larger indices, returns the block number
   at the corresponding indirect block.

   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure where data is to be sourced.
     index: Index to the block number to be searched.

   Returns:
     In case of success, returns the block number to be used for the
     corresponding entry. This block number may be 0 (zero) in case of
     sparse files. In case of error, returns
     EXT2_INVALID_BLOCK_NUMBER.
 */
/*static*/ uint32_t get_inode_block_no(volume_t* volume, inode_t* inode, uint64_t block_idx) {

    // MAKE SURE TO MAKE STATIC AGAIN AFTER TESTING !!!!
    //     !!!!!!!!
    // !!!!!!!!!!!!!!!!!
    //     !!!!!!!!

    if (block_idx < 12) {

        return inode->i_block[block_idx];
    }
    else if (block_idx < (volume->block_size / 4) + 12) {

        //printf("We in the first else if statement\n");

        uint32_t newIndex = block_idx - 12;
        uint32_t returnValue = read_ind_block_entry(volume, inode->i_block_1ind, newIndex);

        return returnValue;

    }
    else if (block_idx < 12 + (((volume->block_size / 4) + 1) * (volume->block_size / 4))) {
        //printf("We in the second else if statement\n");

        uint32_t indBlockNo = read_ind_block_entry(volume, inode->i_block_2ind, (block_idx - (volume->block_size / 4) + 12) / (volume->block_size / 4));

        return read_ind_block_entry(volume, indBlockNo, (block_idx - (volume->block_size / 4 + 12)) % (volume->block_size / 4));

    }
    else if (block_idx < (((((volume->block_size / 4) + 1) * (volume->block_size / 4)) + 1) * (volume->block_size / 4) + 12)) {
        // def wanna double check this triple indirect one

        //printf("We in the third else if statement\n");


        uint32_t indBlockNo2 = read_ind_block_entry(volume, inode->i_block_3ind, (block_idx - (12 + ((volume->block_size / 4) + 1) * (volume->block_size / 4))) / ((volume->block_size / 4) * (volume->block_size / 4)));
        uint32_t indBlockNo1 = read_ind_block_entry(volume, indBlockNo2, ((block_idx - (12 + ((volume->block_size / 4) + 1) * (volume->block_size / 4))) % ((volume->block_size / 4) * (volume->block_size / 4))) / (volume->block_size * volume->block_size));

        uint32_t initialFactor = ((block_idx - (12 + ((volume->block_size / 4) + 1) * (volume->block_size / 4))) % ((volume->block_size / 4) * (volume->block_size / 4)));
        uint32_t thirdIndirectFactor = initialFactor % (volume->block_size / 4);

        return read_ind_block_entry(volume, indBlockNo1, thirdIndirectFactor);
    }
    else {

        return EXT2_INVALID_BLOCK_NUMBER;
    }
}

/* read_file_block: Returns the content of a specific file, limited to
   a single block.

   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the file.
     offset: Offset, in bytes from the start of the file, of the data
             to be read.
     max_size: Maximum number of bytes to read from the block.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_file_block(volume_t* volume, inode_t* inode, uint64_t offset, uint64_t max_size, void* buffer) {

    // obtain the block number for the read_block function call
    uint64_t blockIndex = (int)(offset / volume->block_size);
    uint32_t blockNo = get_inode_block_no(volume, inode, blockIndex);

    uint32_t blockOffset = offset % (volume->block_size);

    // this was the old line,, doesnt work
    //uint32_t blockOffset = offset - (blockNo * volume->block_size);

    // get size of data to read
    uint32_t size;
    //printf("Block offset: %d\n", blockOffset);
    //printf("Max Size: %d\n", max_size);

    if ((volume->block_size - blockOffset) < max_size) {
        //printf("We inside the if of read_file_block");
        size = volume->block_size - blockOffset;
    }
    else {
        size = max_size;
    }

    return read_block(volume, blockNo, blockOffset, size, buffer);
}

/* read_file_content: Returns the content of a specific file, limited
   to the size of the file only. May need to read more than one block,
   with data not necessarily stored in contiguous blocks.

   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the file.
     offset: Offset, in bytes from the start of the file, of the data
             to be read.
     max_size: Maximum number of bytes to read from the file.
     buffer: Pointer to location where data is to be stored.

   Returns:
     In case of success, returns the number of bytes read from the
     disk. In case of error, returns -1.
 */
ssize_t read_file_content(volume_t* volume, inode_t* inode, uint64_t offset, uint64_t max_size, void* buffer) {

    uint32_t read_so_far = 0;

    if (offset + max_size > inode_file_size(volume, inode))
        max_size = inode_file_size(volume, inode) - offset;

    while (read_so_far < max_size) {
        int rv = read_file_block(volume, inode, offset + read_so_far,
            max_size - read_so_far, buffer + read_so_far);
        if (rv <= 0) return rv;
        read_so_far += rv;
    }
    return read_so_far;
}

/* follow_directory_entries: Reads all entries in a directory, calling
   function 'f' for each entry in the directory. Stops when the
   function returns a non-zero value, or when all entries have been
   traversed.

   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the directory.
     context: This pointer is passed as an argument to function 'f'
              unmodified.
     buffer: If function 'f' returns non-zero for any file, and this
             pointer is set to a non-NULL value, this buffer is set to
             the directory entry for which the function returned a
             non-zero value. If the pointer is NULL, nothing is
             saved. If none of the existing entries returns non-zero
             for 'f', the value of this buffer is unspecified.
     f: Function to be called for each directory entry. Receives three
        arguments: the file name as a NULL-terminated string, the
        inode number, and the context argument above.

   Returns:
     If the function 'f' returns non-zero for any directory entry,
     returns the inode number for the corresponding entry. If the
     function returns zero for all entries, or the inode is not a
     directory, or there is an error reading the directory data,
     returns 0 (zero).
 */
uint32_t follow_directory_entries(volume_t* volume, inode_t* inode, void* context,
    dir_entry_t* buffer,
    int (*f)(const char* name, uint32_t inode_no, void* context)) {

    dir_entry_t newDirEntry;
    int offset = 0;
    uint16_t firstSize;
    read_file_block(volume, inode, 4, 2, &firstSize);
    read_file_block(volume, inode, offset, firstSize, &newDirEntry);

    // check to see if this is it

    if ((f(newDirEntry.de_name, newDirEntry.de_inode_no, context) != 0) && (buffer != NULL)) {
        *buffer = newDirEntry;
        return newDirEntry.de_inode_no;
    }

    for (offset = firstSize; offset < inode->i_size; offset += newDirEntry.de_rec_len) {
        read_file_block(volume, inode, offset, newDirEntry.de_rec_len, &newDirEntry);
        read_file_block(volume, inode, offset, newDirEntry.de_rec_len, &newDirEntry);

        if ((f(newDirEntry.de_name, newDirEntry.de_inode_no, context) != 0) && (buffer != NULL)) {
            *buffer = newDirEntry;
            return newDirEntry.de_inode_no;
        }
    }
    return 0;
}

/* Simple comparing function to be used as argument in find_file_in_directory function */
static int compare_file_name(const char* name, uint32_t inode_no, void* context) {
    return !strcmp(name, (char*)context);
}

/* find_file_in_directory: Searches for a file in a directory.

   Parameters:
     volume: Pointer to volume.
     inode: Pointer to inode structure for the directory.
     name: NULL-terminated string for the name of the file. The file
           name must match this name exactly, including case.
     buffer: If the file is found, and this pointer is set to a
             non-NULL value, this buffer is set to the directory entry
             of the file. If the pointer is NULL, nothing is saved. If
             the file is not found, the value of this buffer is
             unspecified.

   Returns:
     If the file exists in the directory, returns the inode number
     associated to the file. If the file does not exist, or the inode
     is not a directory, or there is an error reading the directory
     data, returns 0 (zero).
 */
uint32_t find_file_in_directory(volume_t* volume, inode_t* inode, const char* name, dir_entry_t* buffer) {

    return follow_directory_entries(volume, inode, (char*)name, buffer, compare_file_name);
}

/* find_file_from_path: Searches for a file based on its full path.

   Parameters:
     volume: Pointer to volume.
     path: NULL-terminated string for the full absolute path of the
           file. Must start with '/' character. Path components
           (subdirectories) must be delimited by '/'. The root
           directory can be obtained with the string "/".
     dest_inode: If the file is found, and this pointer is set to a
                 non-NULL value, this buffer is set to the inode of
                 the file. If the pointer is NULL, nothing is
                 saved. If the file is not found, the value of this
                 buffer is unspecified.

   Returns:
     If the file exists, returns the inode number associated to the
     file. If the file does not exist, or there is an error reading
     any directory or inode in the path, returns 0 (zero).
 */
uint32_t find_file_from_path(volume_t* volume, const char* path, inode_t* dest_inode) {

    int nameStart = 1;
    int nameIndex = 1;

    inode_t currInode;
    dir_entry_t currDir;
    int currInodeNum;

    // need to read the inode for the root directory
    if (path[0] == '/') {
        char* currName = malloc(sizeof(char) * 2);
        currName[1] = '\0';
        read_block(volume, volume->groups[0].bg_inode_table, volume->super.s_inode_size, volume->super.s_inode_size, &currInode);
        currInodeNum = 2;
        currDir.de_inode_no = currInodeNum;
        printf("Inode NUm: %d \n", currInodeNum);
        printf("Directory: %s \n", currDir.de_name);
        free(currName);
    } else {
        return 0;
    }

    // now that we passed root dir, iterate through string until it is empty
    for (int i = 1; path[i-1] != '\0'; i++) {
        //printf("Cussing at : %c \n", path[i]);
        if (path[i] == '/' || path[i] == '\0') {
            if (nameStart == nameIndex) {
                break;
            }
            char* currName = malloc(sizeof(char) * (nameIndex - nameStart + 1));

            for (int j = nameStart; j < nameIndex; j++) {
                currName[j - nameStart] = path[j];
            }
            currName[nameIndex - nameStart] = '\0';

            printf("currName: %s \n", currName);
            //read_block(volume, volume->groups[0].bg_inode_table, volume->super.s_inode_size, volume->super.s_inode_size, &currInode);
            currInodeNum = find_file_in_directory(volume, &currInode, currName, &currDir);
            printf("Inode NUm: %d \n", currInodeNum);

            //int blockGroup = (currInodeNum - 1) / volume->super.s_inodes_per_group;
            //int inodeIndex = (currInodeNum - 1) % volume->super.s_inodes_per_group;
            if (currInodeNum == 0) {
                printf("*********** ERRROR GOT INODE 0 WHICH DOES NOT EXIST! ********************");
                break;
            }
            //inode_t paramInode;
            printf("Directory: %s \n", currDir.de_name);
            //printf("BG: %d \n", blockGroup);
            //printf("iI: %d \n", inodeIndex);
            read_inode(volume, currInodeNum, &currInode);
            //read_block(volume, volume->groups[blockGroup].bg_inode_table, , volume->super.s_inode_size, &currInode);

            currName = NULL;
            nameStart = i + 1;
            nameIndex = i + 1;
        }
        else {
            nameIndex++;
        }
    }

    *dest_inode = currInode;
     
    return currDir.de_inode_no;
}
