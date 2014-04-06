//
//  lowlevel.c
//  OS_final
//
//  Created by Brent Mitton on 2014-04-04.
//  Copyright (c) 2014 Brent Mitton. All rights reserved.
//

#include "lowlevel.h"

int acquire_lock(FILE* vDisk, char* fileName){
    
    // start from the beginning of the disk
    rewind(vDisk);

    // find the entry to lock
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    struct FAT_entry entry;
    
    int lock_entry;
    for(lock_entry=0; lock_entry<fat.fat_size; lock_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
        
        if (strcmp(entry.filename, fileName))
            break;
        
    }
    
    
    if(lock_entry >= fat.fat_size){
        printf("Could not find file (%s) for unlocking. \n", fileName);
    }
    
    // back to the beginning of the disk
    rewind(vDisk);
    return 0;
}



int retrieve(char* diskName, char* fileName){

    // open the vDisk
    vDisk = fopen(diskName, "r+");
    
    //acquire_lock(vDisk, fileName);


    // open the FAT
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    // get char vector
    fat.free = (char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    

    // find the entry that we want to copy
    struct FAT_entry entry;
    
    long long read_entry;
    for (read_entry=0; read_entry<fat.fat_size; read_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
        
        if (strcmp(entry.filename, fileName) == 0){
            break;
        }
        
    }
    
    // seek to the location of the file
    fseek(vDisk,
          sizeof(struct FAT) +
          fat.vfree_length +
          sizeof(struct FAT_entry)*fat.fat_size+
         (BLOCK_SIZE*entry.inode_number),
          SEEK_SET);
    
    char buffer[BLOCK_SIZE];
    int bytes_to_read = entry.length;
    
    // create the new file
    // prepending vfs_ here so that I dont accidently overwrite files
    // that are actually on my system
    char new_filename[100] = "";
    strcat(new_filename, "vfs_");
    strcat(new_filename, fileName);
    
    FILE* new_file = fopen(new_filename, "w+");
    
    while (bytes_to_read>0){
        fread(buffer, 1, BLOCK_SIZE, vDisk);
        fprintf(new_file, "%s", buffer);
        bytes_to_read = bytes_to_read - BLOCK_SIZE;
    }
    
    return 0;
}


int readFile(char* diskName, char* fileName){
    
    // open the vDisk
    vDisk = fopen(diskName, "r");
    
    // open the FAT
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    
    // get char vector
    fat.free = (char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    // find the entry that we want to read
    struct FAT_entry entry;
    
    int read_entry;
    for(read_entry=0; read_entry<fat.fat_size; read_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
        
        if(strcmp(entry.filename, fileName) == 0)
            break;
    }
    
    
    // seek to the location of the file
    fseek(vDisk,
          sizeof(struct FAT) +
          fat.vfree_length +
          sizeof(struct FAT_entry)*fat.fat_size+
          (BLOCK_SIZE*entry.inode_number),
          SEEK_SET);
    
    
    char buffer[BLOCK_SIZE];
    int bytes_to_read = entry.length;

    while(bytes_to_read > 0){
        fread(buffer, 1, BLOCK_SIZE, vDisk);
        printf("%s", buffer);
        
        bytes_to_read = bytes_to_read - BLOCK_SIZE;
    }
    
    // error message if applicable
    if(read_entry >= fat.fat_size)
        printf("Could not find file (%s) for reading. \n", fileName);
    
    fclose(vDisk);

    return 0;
}

int format(char* filename, long long size){
    
    struct FAT fat;
    char *char_vector;
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
    fat.free = (char*)malloc(fat.vfree_length);
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
                setFATentry(vDisk, fat, possible_entry, file, i, length);
                break;
            }else{

                // try the next location
                i = i+(blocks_needed-next);
            }
        }else{
            // try the next pos
            i++;  // I have no idea why I need to do this...
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

// remove a file from the disk
int removeFile(char* diskName, char* file){
    
    // open the FAT
    struct FAT fat;
    vDisk = fopen(diskName, "r+");
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    // load up the char vector
    fat.free=(char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    
    // find the file that we want to delete.
    struct FAT_entry entry;
    
    int del_entry;
    for (del_entry=0; del_entry<fat.fat_size; del_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
        if (!strcmp(entry.filename, file))
            break;
    }
    
    // find out how many blocks the entry was using, rounding up
    long  blocks_needed = entry.length / BLOCK_SIZE;
    if (entry.length % BLOCK_SIZE)
        blocks_needed++;
    
    // delete fat entry
    delFATentry(vDisk, fat, del_entry);
    
    // deallocate this from char vector
    deallocate(vDisk, fat, entry.inode_number, blocks_needed);
    
    if (del_entry >= fat.fat_size)
        printf("Could not find (%s) for deletion. \n", file);
    
    fclose(vDisk);
    free(fat.free);
    
    return 0;
    
}

// I think that this copies a file to our vDisk
int copy(char *diskName, char* file){
    
    // open our disk
    vDisk = fopen(diskName, "r+");
    
    // will store some information about a local file
    struct stat localFile;
    
    if (stat(file, &localFile) != 0){
        printf("Could not stat file %s \n", file);
        exit(EXIT_FAILURE);
    }
    
    long long length = localFile.st_size;
    
    // get number of blocks needed for this file.
    // round up
    long blocks_needed = length / BLOCK_SIZE;
    if (length % BLOCK_SIZE)
        blocks_needed++;
    
    // load up our FAT
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    // load the char vector
    fat.free=(char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    struct FAT_entry entry;
    
    // search the FAT for an available location to copy the file
    int possible_entry;
    for (possible_entry=0; possible_entry<fat.fat_size; possible_entry++){
        fread(&entry, sizeof(entry), 1, vDisk);
        
        if (entry.filename[0]=='\0'){
            break;
        }
    }
    
    // now we've got to find some blocks to store the file
    int pos;
    for(pos=fat.start_block; pos<fat.end_block-blocks_needed; pos++){
        // find some available contiguous space
        if (isfree(pos, fat.free, fat.vfree_length)){
            long long next = tryAllocate(pos, fat.free, fat.end_block, blocks_needed);
            
            if (next==0l){
                // we can allocate space!
                allocate(vDisk, fat, pos, blocks_needed);
                setFATentry(vDisk, fat, possible_entry, file, pos, length);
                
                // now we actually need to write the file to the disk
                long long data_start = sizeof(struct FAT) +
                    fat.vfree_length +
                    (sizeof(struct FAT_entry)*fat.fat_size) +
                    (pos*BLOCK_SIZE);
                
                
                fseek(vDisk, data_start, SEEK_SET);
                
                FILE* source_file = fopen(file, "rb");
                char buffer[BLOCK_SIZE];
                int bytes_read;
                
                while((bytes_read=fread(buffer, 1, BLOCK_SIZE, source_file))){
                    fwrite(buffer, bytes_read, 1, vDisk);
                }
                
                fclose(source_file);
                break;
            }else{
                pos = pos+(blocks_needed - next);
            }
        }else{
            // couldnt continguous space at pos.
            pos++;
        }
    }
    
    fclose(vDisk);
    free(fat.free);
    
    if (pos >= fat.end_block - blocks_needed){
        printf("Allocation failed for copy; not enough contiguous space");
    }
    
    return 0;
}


int deallocate(FILE* vDisk, struct FAT fat, long long pos, long long length){
    
    // zero all the appropriate locations in the char vector
    while (length){
        fat.free[pos] = 0;
        pos++;
        length--;
    }
    
    // rewrite back to disk with new changes
    fseek(vDisk, sizeof(struct FAT), SEEK_SET);
    fwrite(fat.free, 1, fat.vfree_length, vDisk);
    
    return 0;
}

int delFATentry(FILE *vDisk, struct FAT fat, int entry_num){
    struct FAT_entry entry;
    
    // delete basically means set everything to blank
    strcpy(entry.filename, "");
    entry.length = 0;
    entry.inode_number = 0;
    
    // seek to the entry location on the vDisk
    //   seeking past FAT table, char vector and previous entries
    fseek(vDisk,
          sizeof(struct FAT) +                     // FAT size
          fat.vfree_length +                       // char vector size
          (sizeof(struct FAT_entry) * entry_num),  // previous entries
          SEEK_SET);
    
    
    // write the empty entry at this location
    fwrite(&entry, sizeof(struct FAT_entry), 1, vDisk);

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
    entry.locked = 0;
    
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
int tryAllocate(long long pos, char* free, long long end_block, long long length){
    
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

int isfree(long long pos, char* free, long long freesize){
    return !(free[pos]);
}

// Writes the initial FAT to the beginning of vDisk

char* WriteFAT(long long size){
    struct FAT fat;
    // initialize these things.
    fat.num_blocks = size;
    fat.fat_size = 1024;
    fat.start_block = 0;
    fat.end_block = size-1;
    fat.vfree_length = size+1;
    
    fat.free = (char*)malloc(fat.vfree_length);
    
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
    entry.locked = 0;
    
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
