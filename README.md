# ext2-fileSystem
Opening a ext2 file and accessing data by going through all of its inodes, direct blocks, and inderect blocks.

# Operations to read ext2 file
The file ext2.c contains the methods required to do essential operations to read an ext2 file system. This includes the opterations such as getting the information, inode, and content of a block; and seaching through the file system for a requested file directory.

# Representing a ext2-fileSystem
The file ext2fs.c uses an instance of the ext2 class to represent a ext2 file system by using it's operations and using FUSE for opening a file to use. This file can give the user information on the selected file to open, such as volume, number of blocks and inodes, and block size. It can also be used to open requested directories or find a directory with a given name.
