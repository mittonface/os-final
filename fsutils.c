//
//  fsutils.c
//  OS_final
//
//  Created by Brent Mitton on 2014-04-04.
//  Copyright (c) 2014 Brent Mitton. All rights reserved.
//

#include "fsutils.h"

// prints information about the filesystem.
int info(char* filename){
    
    
    struct FAT fat;
    struct FAT_entry entry;
    
    int num_files = 0;
    long long num_blocks = 0;
    
    vDisk = fopen(filename, "r");
    
    // read the FAT into fat
    fread(&fat, sizeof(fat), 1, vDisk);
    
    // Print out information about the FAT
    printf("Total number of Blocks -> %lld \n", fat.num_blocks);
    printf("Size of FAT -> %d \n", fat.fat_size);
    
    // seek up to the start of the entries
    fseek(vDisk, fat.vfree_length, SEEK_CUR);
    
    for (int i=0; i<fat.fat_size; i++){
        // read in an entry
        fread(&entry, sizeof(entry), 1, vDisk);
        
        if (entry.filename[0] != '\0'){
            num_files++;
            num_blocks += entry.length / BLOCK_SIZE;
            
            if (entry.length % BLOCK_SIZE)
                num_blocks++;
            
            // print out information about this entry.
            printf("Entry #%d -> %s \n", num_files, entry.filename);
            printf("\t inode: %lld, length: %lld \n", entry.inode_number, entry.length);
        }
    }
    
    // print out summary information
    printf("\n %d files, %lld blocks, %lld bytes \n", num_files, num_blocks, num_blocks*BLOCK_SIZE);
    
    return 0;
}

// lists all the files on the FS
int list(char* filename, char* args){
    
    struct FAT fat;
    struct FAT_entry entry;
    
    vDisk = fopen(filename, "r");
    
    // read the fat
    fread(&fat, sizeof(fat), 1, vDisk);
    
    // seek to the entries
    fseek(vDisk, fat.vfree_length, SEEK_CUR);
    
    for (int i=0; i<fat.fat_size; i++){
        // get the entry
        fread(&entry, sizeof(entry), 1, vDisk);
    
        if (entry.filename[0] != '\0')
            printf("%s \n", entry.filename);
    }
    return 0;
}


// print all the block entries
int list_block_entries(char* filename){
    
    // open the vdisk
    vDisk = fopen(filename, "r+");
    
    // load fat
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    // load characteristic vector
    fat.free = (char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    printf("Characteristic Vector: \n");
    for (int i=0; i<fat.vfree_length; i++)
        printf("%d", fat.free[i]);
    puts("");
    
    return 0;
    
}














