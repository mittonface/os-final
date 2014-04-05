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
unsigned char* WriteFAT(struct FAT *fat, long long size);
int writeFATentries(struct FAT *fat);
int writeEmptyBlockData(long long size);


#endif
