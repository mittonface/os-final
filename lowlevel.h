//
//  lowlevel.h
//  OS_final
//
//  Created by Brent Mitton on 2014-04-04.
//  Copyright (c) 2014 Brent Mitton. All rights reserved.
//

#ifndef OS_final_lowlevel_h
#define OS_final_lowlevel_h

#include "fs.h"
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// these would be the user-callable functions
int format(char* filename, long long size);
int create(char* diskName, char* file, long long length);
int removeFile(char* diskName, char* file);
int copy(char* diskName, char* file);


// these are lower level operations used by those ^^
unsigned char* WriteFAT(long long size);
int writeEmptyBlockData(long long size);
int isfree(long long pos, unsigned char* free, long long freesize);

int writeFATentries();
int setFATentry(FILE *vDisk, struct FAT fat, int entry_num, char* file, long long pos, long long length );
int delFATentry(FILE *vDisk, struct FAT fat, int entry_num);

int tryAllocate(long long pos, unsigned char* free, long long end_block, long long length);
int allocate(FILE *vDisk, struct FAT fat, long long pos, long long length);
int deallocate(FILE *vDisk, struct FAT fat, long long pos, long long length);

#endif
