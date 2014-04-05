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

int format(char* filename, long long size);
int create(char* diskName, char* file, long long length);

unsigned char* WriteFAT(struct FAT fat, long long size);
int writeFATentries(struct FAT fat);
int writeEmptyBlockData(long long size);
int isfree(long long pos, unsigned char* free, long long freesize);
int tryAllocate(long long pos, unsigned char* free, long long end_block, long long length);
int allocate(FILE *vDisk, struct FAT fat, long long pos, long long length);
int setFATentry(FILE *vDisk, struct FAT fat, int entry_num, char* file, long long pos, long long length );

#endif
