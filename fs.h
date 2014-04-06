//
//  fs.h
//  OS_final
//
//  Created by Brent Mitton on 2014-04-04.
//  Copyright (c) 2014 Brent Mitton. All rights reserved.
//

#ifndef OS_final_fs_h
#define OS_final_fs_h

#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 512
#define FAT_ENTRY_SIZE 1024
#define MAX_FILES 1024
#define MAX_NAME_LENGTH 100

FILE* vDisk;

struct FAT{
    long long num_blocks;
    long long start_block;
    long long end_block;
    long long vfree_length;
    char* free;
    int fat_size;
};

struct file_block{
    char block[512];
};

struct FAT_entry{
    long long inode_number;
    long long length;
    char filename[MAX_NAME_LENGTH];
    int locked;
};

#endif
