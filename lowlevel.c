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
    char_vector = WriteFAT(size);
    
    // write the free block char vector
    fwrite(char_vector, size+1, 1, vDisk);
    
    // write blank file entries
    writeFATentries();
    
    // init all blocks to 0
    writeEmptyBlockData(size);
    
    fclose(vDisk);
    free(char_vector);
    
    return 0;
}

int create(char* diskName, char* file, long long length){
    
    struct FAT fat;
    struct FAT_entry entry;
    
    // open the vDisk
    vDisk = fopen(diskName, "r+");
    
    // find out how many blocks we need to store this file.
    long long blocks_needed = length / BLOCK_SIZE;
    
    // round up
    if (length % BLOCK_SIZE)
        blocks_needed++;
    
    // read the FAT in
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    // read in char vector for num of blocks
    fat.free = (unsigned char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    // search for a free entry in the table
    int possible_entry;
    for (possible_entry=0; possible_entry<fat.fat_size; possible_entry++){
        fread(&entry, sizeof(entry), 1, vDisk);
        
        if (entry.filename[0] == '\0'){
            break;
        }
    }
    
    int i;
    
    for (i=fat.start_block; i<fat.end_block-blocks_needed; i++){
        // find the first free block of contiguous space
        if (isfree(i, fat.free, fat.vfree_length)){
            long long next = tryAllocate(i, fat.free, fat.end_block, blocks_needed);
            
            if (next==0ll){

                // we can allocate the space for this here
                allocate(vDisk, fat, i, blocks_needed);
                setFATentry(vDisk, fat, possible_entry, file, i, blocks_needed);
                break;
            }else{

                // try the next location
                i = i+(blocks_needed-next);
            }
        }else{
            i++;
        }
    }
    
    fclose(vDisk);
    free(fat.free);
    
    if (i >= fat.end_block-blocks_needed){
        printf("Allocation failed. There was not enough contiguous space. \n");
        return 1;
    }
    
    return 0;
}


int setFATentry(FILE *vDisk, struct FAT fat, int entry_num,
                char* file, long long pos, long long length)
{
    
    struct FAT_entry entry;
    
    // create the new entry
    strcpy(entry.filename, file);
    entry.length = length;
    entry.inode_number = pos;
    
    // seek to the entry location on the vDisk
    // that means: past the FAT table, past the char string, and to entry #entry_num
    fseek(vDisk,
          sizeof(struct FAT) +                     // FAT
          fat.vfree_length +                      // char string
          (sizeof(struct FAT_entry) * entry_num),  // prev FAT entries
          SEEK_SET);
    
    // write the FAT entry to disk
    fwrite(&entry, sizeof(struct FAT_entry), 1, vDisk);
    
    return 0;
}


int allocate(FILE *vDisk, struct FAT fat, long long pos, long long length){

    // populate the char vector with 1s to show that we have used
    // the blocks
    while (length){
        fat.free[pos] = 1;
        pos++;
        length--;
    }
    
    // seek to the char vector on disk, write the new char vector to disk
    fseek(vDisk, sizeof(struct FAT), SEEK_SET);
    fwrite(fat.free, 1, fat.vfree_length, vDisk);
    
    return 0;
}

// I guess we're checking to see if we can fit this file in
// this location.
int tryAllocate(long long pos, unsigned char* free, long long end_block, long long length){
    
        char offset_check = (free[pos]);
    
    while(length){
        if (offset_check)
            return length;
        pos++;
        offset_check = free[pos];
        length--;
    }
    return 0;
}

int isfree(long long pos, unsigned char* free, long long freesize){
    return !(free[pos]);
}

// Writes the initial FAT to the beginning of vDisk
unsigned char* WriteFAT(long long size){
    struct FAT fat;
    // initialize these things.
    fat.num_blocks = size;
    fat.fat_size = 1024;
    fat.start_block = 0;
    fat.end_block = size-1;
    fat.vfree_length = size+1;
    
    // character per block? I still am not sure what this characteristic string is
    fat.free = (unsigned char*)malloc(fat.vfree_length);
    
    for(int i=0; i<fat.vfree_length; i++){
        fat.free[i] = 0;
    }
    
    // write the FAT to the beginning of vdisk
    fwrite(&fat, sizeof(fat), 1, vDisk);
    
    return fat.free;
}

// write blank entries for the entire fat table.
int writeFATentries(){
    struct FAT fat;
    struct FAT_entry entry;
    
    // load the fat table
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    entry.inode_number = 0ll;
    entry.length = 0ll;
    
    // filename to blank, starting with \0
    for (int i=0; i<MAX_NAME_LENGTH; i++){
        entry.filename[i] = ' ';
    }
    
    entry.filename[0] = '\0';
    
    // for each member of the FAT table, write this blank entry
    for (int i=0; i<=fat.fat_size; i++){
        fwrite(&entry, sizeof(entry), 1, vDisk);
    }
    return 0;
}


// set all blocks to zero
int writeEmptyBlockData(long long size){
    struct file_block buffer;
    
    for(int i=0; i<BLOCK_SIZE; i++){
        buffer.block[i] = 0;
    }

    for (long long i=0; i<size; i++){
        fwrite(&buffer, BLOCK_SIZE, 1, vDisk);
    }
    
    return 0;
}
