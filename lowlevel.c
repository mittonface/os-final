//
//  lowlevel.c
//  OS_final
//
//  Created by Brent Mitton on 2014-04-04.
//  Copyright (c) 2014 Brent Mitton. All rights reserved.
//

#include "lowlevel.h"

int format(char* filename, long long size){
    
    struct FAT fat;
    unsigned char *char_vector;
    vDisk = fopen(filename, "wb");
    
    
    // create the FAT at the beginning of vDisk
    char_vector = WriteFAT(&fat, size);
    
    // write the free block char vector
    fwrite(char_vector, size+1, 1, vDisk);
    
    // write blank file entries
    writeFATentries(&fat);
    
    // init all blocks to 0
    writeEmptyBlockData(size);
    
    fclose(vDisk);
    free(char_vector);
    
    return 0;
}


// Writes the initial FAT to the beginning of vDisk
unsigned char* WriteFAT(struct FAT *fat, long long size){
    
    // initialize these things.
    fat->num_blocks = size;
    fat->size = MAX_FILES;
    fat->start_block = 0;
    fat->end_block = 0;
    fat->vfree_length = size+1;
    
    // character per block? I still am not sure what this characteristic string is
    fat->free = (unsigned char*)malloc(fat->vfree_length);
    
    for(int i=0; i<fat->vfree_length; i++)
        fat->free[i] = 0;
    
    // write the FAT to the beginning of vdisk
    fwrite(&fat, sizeof(fat), 1, vDisk);
    
    return fat->free;
}

// write blank entries for the entire fat table.
int writeFATentries(struct FAT *fat){
    struct FAT_entry entry;
    
    entry.inode_number = 0ll;
    entry.length = 0ll;
    
    // filename to blank, starting with \0
    for (int i=0; i<MAX_NAME_LENGTH; i++)
        entry.filename[i] = ' ';
    
    entry.filename[0] = '\0';
    
    // for each member of the FAT table, write this blank entry
    for (int i=0; i<=fat->size; i++)
        fwrite(&entry, sizeof(entry), 1, vDisk);
    
    return 0;
}


// set all blocks to zero
int writeEmptyBlockData(long long size){
    struct file_block buffer;
    
    for(int i=0; i<BLOCK_SIZE; i++)
        buffer.block[i] = 0;

    for (int i=0; i<size; i++)
        fwrite(&buffer, BLOCK_SIZE, 1, vDisk);
    
    return 0;
}
