//
//  lowlevel.c
//  OS_final
//
//  Created by Brent Mitton on 2014-04-04.
//  Copyright (c) 2014 Brent Mitton. All rights reserved.
//

/**
 * @file
 *  Most of the OS level functions will be here.
 */
#include "lowlevel.h"




/**
 * @param diskName
 *  The filename of the virtual disk
 * @param fileName
 *  The file that we want to write to.
 * @param input
 *  The input to write to the file
 * @param mode
 *  0 - for overwrite
 *  1 - for append
 *
 *  Writes the contents to the file. Depending on the mode
 *  given this will either replace the entire file contents
 *  or append to the end of the file. (Growing the file if needed)
 */
int writeFile(char* diskName, char* fileName, char* input, int mode){
    
    if (mode == 0){
        // i'm going to cheat a little bit and just delete the file and create
        // it again with new contents
        removeFile(diskName, fileName);
        
        // now I'll create a file of the correct size
        create(diskName, fileName, strlen(input));
        
        // now I just need to update the file with the new contents
        // first I'll get the entry so that I can get the inode. Perhaps
        // I should have create return the inode
        
        // load vDisk and fat data
        vDisk = fopen(diskName, "r+");

        struct FAT fat;
        struct FAT_entry entry;
        
        fread(&fat, sizeof(struct FAT), 1, vDisk);
        fat.free = (char*)malloc(fat.vfree_length);
        fread(fat.free, fat.vfree_length, 1, vDisk);
        
        int write_entry;
        for (write_entry=0; write_entry<fat.fat_size; write_entry++){
            fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
            
            if (strcmp(entry.filename, fileName) == 0)
                break;
        }
        
        
        if (write_entry>=fat.fat_size){
            printf("Could not find file (%s) to overwrite", fileName);
            return 1;
        }
        
        // now seek to the data location
        // the entry should already be correct
        fseek(vDisk,
              sizeof(struct FAT) +
              fat.vfree_length +
              sizeof(struct FAT_entry) * fat.fat_size+
              BLOCK_SIZE * entry.inode_number,
              SEEK_SET);
        
        // I should just be able to write directly from the input to the vDisk
        // now
        fwrite(input, strlen(input), 1, vDisk);
        
    }else{
        // append the contents to the end of the file.
        // this is trickier...
        
        // load vDisk and fat data
        vDisk = fopen(diskName, "r+");
      
        
        struct FAT fat;
        struct FAT_entry entry;
        
        fread(&fat, sizeof(struct FAT), 1, vDisk);
        fat.free = (char*)malloc(fat.vfree_length);
        fread(fat.free, fat.vfree_length, 1, vDisk);
        
        // find the entry that we'll want to change
        int write_entry;
        for(write_entry=0; write_entry<fat.fat_size; write_entry++){
            fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
            
            if (strcmp(entry.filename, fileName) ==0)
                break;
        }
        
        // resize the file to allow for the new data.
        
        printf("Entry length %lld \n", entry.length);
        int resize_stat = resize(diskName, fileName, entry.length+strlen(input));

        // I'm going to reload the entry here, because it looks like I might need
        // to
        
        fseek(vDisk, sizeof(struct FAT) + fat.vfree_length, SEEK_SET);
        
        for(write_entry=0; write_entry<fat.fat_size; write_entry++){
            fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
            
            if (strcmp(entry.filename, fileName) ==0)
                break;
        }
        

        printf("Entry length %lld \n", entry.length);
        printf("Entry inode %lld \n", entry.inode_number);
        printf("fat size %lld \n", fat.fat_size);
        printf("Entry length %lld \n", entry.length);

        if (resize_stat == 0) {
            // successfully resized the file.
            // but we need to reopen it now
            vDisk = fopen(diskName, "r+");
            
            fseek(vDisk,
                  sizeof(struct FAT) +
                  fat.vfree_length +
                  (sizeof(struct FAT_entry) * fat.fat_size) +
                  (BLOCK_SIZE * entry.inode_number) +
                  entry.length,     // the entry will be the new length of the file
                  SEEK_SET);                        // but we haven't saved the new yet.
            
            fwrite(input, strlen(input), 1, vDisk);
            fclose(vDisk);
            free(fat.free);
        }
        
        
    }
    
    fclose(vDisk);
    
    return 0;
}





/**
 * @param diskName
 *  The filename of the virtual disk
 * @param fileName
 *  The file that we want to perform the operatoin on.
 * @param new_inode
 *  The new starting block that we want to move the file to.
 * @return 
 *  0 - if we moved the file
 *  1 - if we could not move the file
 *
 *  Moves a file to the given block. 
 * 
 *  Will not move a file to a block if there is already a file
 *  there is not enough contiguous empty space for it
 */
int moveFile(char* diskName, char* fileName, long long new_inode){
    
    // first thing we'll do is see if we have enough free contiguous space
    // at the desired location
    vDisk = fopen(diskName, "r+");
    
    if (acquire_lock(vDisk, fileName))
        return 1;
    
    // load FAT and char vector
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    fat.free = (char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    // we'll have to get the information about the file we want to move
    struct FAT_entry entry;

    int move_entry;
    for (move_entry=0; move_entry<fat.fat_size; move_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
        
        if (strcmp(entry.filename, fileName) == 0)
            break;
    }
    
    if (move_entry >= fat.fat_size){
        printf("Could not find the file (%s) to move.", fileName);
        release_lock(vDisk, fileName);
        return 1;
    }
    
    // find out how many blocks we need for this file
    int blocks_needed = entry.length / BLOCK_SIZE;
    if (entry.length % BLOCK_SIZE)
        blocks_needed++;
    
    int canMove = 1;

    // check the characteristic vector for the free space
    // TODO:
    //    Found a bit of an issue. Even if the file that we are
    //    copying is what is currently occupying the space, the
    //    copy does not work.
    //    I'll work around this if I have time.
    for (int i=new_inode; i<=new_inode+blocks_needed; i++){
        
        if (i > fat.end_block){
            canMove = 0;
            break;
        }
        
        if (fat.free[i] != 0){
            canMove = 0;
            break;
        }
    }
    
    
    if (canMove == 1){
        // I'm going to open up an additional pointer to the vDisk.
        // This way I can simultaneously read the section to be moved
        // and write to the new section.
        FILE* vDisk_TEMP = fopen(diskName, "r+");
        
        // read from vDisk_TEMP, write to vDisk
        
        // seek to the read location
        fseek(vDisk_TEMP,
              sizeof(struct FAT) +
              fat.vfree_length +
              sizeof(struct FAT_entry) * fat.fat_size +
              BLOCK_SIZE * entry.inode_number,
              SEEK_SET);
        
        
        // seek to the write location
        fseek(vDisk,
              sizeof(struct FAT) +
              fat.vfree_length +
              sizeof(struct FAT_entry) * fat.fat_size +
              BLOCK_SIZE * new_inode,
              SEEK_SET);
        
        
        char buffer[BLOCK_SIZE];
        long long bytes_to_read = blocks_needed * 512;
        
        while (bytes_to_read>0){
            // move content from read section to write section
            fread(buffer, 1, BLOCK_SIZE, vDisk_TEMP);
            fwrite(buffer, BLOCK_SIZE, 1, vDisk);
            bytes_to_read = bytes_to_read - BLOCK_SIZE;
        }
        
        fclose(vDisk_TEMP);
        // the data should now be dealt with. Just need to adjust the
        // fat entry, allocate the new space and deallocate the old space
        long long old_inode = entry.inode_number;
        entry.inode_number = new_inode;
        
        // unfortunately I have to do a seek to move_entry to write this change.
        fseek(vDisk,
              sizeof(struct FAT) +
              fat.vfree_length +
              sizeof(struct FAT_entry)*move_entry,
              SEEK_SET
              );
        fwrite(&entry, 1, sizeof(struct FAT_entry), vDisk);
        
        // now allocate the new space and deallocate the old space.
        allocate(vDisk, fat, new_inode, blocks_needed);
        deallocate(vDisk, fat, old_inode, blocks_needed);
        
        fclose(vDisk);
        free(fat.free);
        release_lock(vDisk, fileName);
        return 0;
    }else{
        printf("Cannot move file (%s) to block (%lld) \n", fileName, new_inode);
        release_lock(vDisk, fileName);
        return 1;
    }

    release_lock(vDisk, fileName);
    return 0;
}

/**
 * @param diskName
 *  The filename of the virtual disk
 * @param fileName
 *  The name of the file we want to perform the operation on.
 * @param new_size
 *  The new size of the file (in bytes)
 *
 *  Resizes the given file. Two different things could happen.
 *
 *  If we are making the file bigger, we will try to find enough contiguous
 *  space on the disk to allow us to store the file. We will never overwrite
 *  allocated space.
 *
 *  If we are making the file smaller, we will just deallocate blocks near the
 *  end of the file. This could cause the file to lose data. But... I don't really
 *  know what else to do. Files should already be taking up the amount of space 
 *  that they need
 */
int resize(char* diskName, char* fileName, long long new_size){
    
    printf("NEW SIZE %lld \n", new_size );
    // open the vDisk
    vDisk = fopen(diskName, "r+");
    

    
    // open the FAT
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    fat.free = (char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    // first I'll find the file to be resized
    struct FAT_entry entry;
    
    int resize_entry;
    for (resize_entry=0; resize_entry<fat.fat_size; resize_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);

        if (strcmp(entry.filename, fileName) == 0)
            break;
    }
    
    // now that I have the entry for the file, I know the starting block.
    // so I just need to deal with the characteristic vector to deallocate / allocate
    // some space. And then change the size stored on the entry.
    
    long long blocks_used = entry.length / BLOCK_SIZE;
    if (entry.length % BLOCK_SIZE)
        blocks_used++;
    
    long long blocks_needed = new_size / BLOCK_SIZE;
    if (new_size % BLOCK_SIZE)
        blocks_needed++;
    

    if (blocks_used > blocks_needed){
        // we're shrinking the file. This could cause us to lose data.
        long long original_endblock = entry.inode_number + blocks_used;
        long long new_endblock = entry.inode_number + blocks_needed;
        
        
        // now I need to write the entry back to disk.
        // seek back one entry and save the entry
        entry.length = new_size;
        fseek(vDisk, -(sizeof(struct FAT_entry)), SEEK_CUR);
        fwrite(&entry, sizeof(struct FAT_entry), 1, vDisk);
        
        // deallocate from new_enblock to original_enblock

        deallocate(vDisk, fat, new_endblock, (original_endblock-new_endblock));
        
        // turns out those changes won't get saved unless I close the disk.
        // who knew!?
        fclose(vDisk);
        free(fat.free);
        return 0;
    }else if (blocks_used < blocks_needed){
        // we're growing the file. I'll try to find somewhere that this file can fit.
        // but I won't overwrite any data if I can't find the contiguous space
        
        // first, check if the space we're in can just easily be grown.
        int pos;
        int in_place = 1;
        for (pos = entry.inode_number+blocks_used; pos<=entry.inode_number+blocks_needed; pos++){
            if(isfree(pos, fat.free, 0) == 0){
                in_place = 0;
                break;          // we cannot allocate the space in this location
            }
        }
        
        if (in_place){
            // in place allocation
            
            // adjust the entry size and write it back to disk
            entry.length = new_size;

            fseek(vDisk, -(sizeof(struct FAT_entry)), SEEK_CUR);
            fwrite(&entry, sizeof(struct FAT_entry), 1, vDisk);
            
            // allocate the space on the char vector
            allocate(vDisk, fat, entry.inode_number, blocks_needed);
            
            fclose(vDisk);
            free(fat.free);
            return 0;
            
        }else{
            // find somewhere that we can allocate this file
            int j;
            long long next;
            for(j=fat.start_block;j<fat.end_block-blocks_needed; j++){
                if (isfree(j, fat.free, 0)){
                    next = tryAllocate(j, fat.free, fat.end_block, blocks_needed);
                    
                    // move the file to the next available portion of contiguous space
                    if (next == 0ll){
                        // move file will close the file.
                        // I want to make sure I can open it again
                        fpos_t fpointer;
                        fgetpos(vDisk, &fpointer);
                        
                        moveFile(diskName, fileName, j);
                        
                        vDisk = fopen(diskName, "r+");

                        fsetpos(vDisk, &fpointer);
                        
                        allocate(vDisk, fat, j, blocks_needed);
                        break;
                    }else{
                        j = j+(blocks_needed-next);
                    }
                }
            }

            
            if (j >= fat.end_block-blocks_needed){
                printf("Could not resize (%s). Not enough contiguous space", fileName);
                return 1;
            }

        }

    }else{
        // same number of blocks, so I dont need to allocate anything else.
        // I should just need to adjust the entry length
        entry.length = new_size;
        
        // should be able to write back to disk by seeking backwards one
        // entry
        fseek(vDisk, -(sizeof(struct FAT_entry)), SEEK_CUR);
        fwrite(&entry, sizeof(struct FAT_entry), 1, vDisk);
    }
    
    
    fclose(vDisk);
    free(fat.free);
    return 0;
}


/**
 * @param diskName
 *  The filename of the virtual disk
 * @param fileName
 *  The name of the file we want to perform the operation on.
 *
 *  This function retrieves the file from the virtual disk and stores it on 
 *  local disk. Stores the file as whatever it was already named, prepended
 *  with vfs_
 */
int retrieve(char* diskName, char* fileName){

    // open the vDisk
    vDisk = fopen(diskName, "r+");
    
    if (acquire_lock(vDisk, fileName))
        return 1;
    
    
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
    
    release_lock(vDisk, fileName);
    fclose(new_file);
    fclose(vDisk);
    free(fat.free);
    return 0;
}

/**
 * @param diskName
 *  The filename of the virtual disk
 * @param fileName
 *  The name of the file we want to perform the operation on.
 *
 *  This takes a file on the virtual disk and outputs it contents
 *  to stdout.
 */
int readFile(char* diskName, char* fileName){
    
    // open the vDisk
    vDisk = fopen(diskName, "r");
    
    // acquire the lock for read the file. Or return if we can't get it
    if (acquire_lock(vDisk, fileName))
        return 1;
    
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
    
    // release the lock on the file.
    release_lock(vDisk, fileName);
    
    fclose(vDisk);
    free(fat.free);

    return 0;
}


/**
 * @param diskName
 *  The filename of the virtual disk
 * @param file
 *  The name of the file we want to perform the operation on.
 * @param length
 *  The size of the file to be created (in bytes)
 *
 *  Creates an empty file on the virtual disk
 */
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


/**
 * @param diskName
 *  The filename of the virtual disk
 * @param fileName
 *  The name of the file we want to perform the operation on.
 *
 *  Removes the file from the virtual disk.
 */
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


/**
 * @param diskName
 *  The filename of the virtual disk
 * @param file
 *  The name of the file (on local disk) we want to perform the operation on.
 *
 *  Takes a file from the local disk and copies it to the virtual disk
 */
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


/**
 * @param filename
 *  The filename of the virtual disk (to be created)
 * @param size
 *  The size of the virtual disk that we will be creating
 *
 *  This will format/create a new virtual disk at ./filename
 */

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


/**
 * @param vDisk
 *  A reference to the virtual disk
 * @param fat
 *  A copy of the FAT
 * @param pos
 *  The starting position (on the char. vector) for deallocation.
 * @param length
 *  The amount of blocks to .
 *
 *  Decallocates items from the virtual disk. Does this by marking 0's in
 *  the characteristic vector
 */
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

/**
 * @param vDisk
 *  A reference to the virtual disk
 * @param fat
 *  A copy of the FAT
 * @param entry_num
 *  The number of the entry that we want to remove from the FAT.
 *
 *  Removes an entry from the FAT. Does this by setting all properties
 *  to blank
 */
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


/**
 * @param vDisk
 *  A reference to the virtual disk
 * @param fat
 *  A copy of the FAT
 * @param entry_num
 *  The number of the entry that we want to remove from the FAT.
 * @param file
 *  The filename for the entry on the FAT
 * @param pos
 *  The inode number for the entry on the FAT
 * @param length
 *  The length of the entry on the FAT (in bytes)
 *
 *  Creates a new entry on the FAT for the given file.
 */
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

/**
 * @param vDisk
 *  A reference to the virtual disk
 * @param fat
 *  A copy of the FAT
 * @param pos
 *  The inode number for the entry on the FAT
 * @param length
 *  The length of the entry on the FAT (in blocks)
 *
 *  Allocates the space as used on the FAT. Does this by writing 
 *  1's to the characteristic vector
 */
int allocate(FILE *vDisk, struct FAT fat, long long pos, long long length){

    printf("pos %lld   length %lld \n", pos, length);

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

/**
 * @param pos
 *  The starting position.
 * @param free
 *  The characteristic vector.
 * @param end_block
 *  This doesn't actually seem to be used. I can probably get rid of it
 * @param length
 *  The length of the item that we want to allocate space for
 *
 *  Tries to find a place which to which we can allocate something of length
 *  length
 */
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


/**
 * @param pos
 *  The position that we want to check.
 * @param free
 *  The characteristic vector.
 * @param freesize
 *  Not used. Remove.
 *
 *  Checks whether a block is allocated
 */
int isfree(long long pos, char* free, long long freesize){
    return !(free[pos]);
}


/**
 * @param size
 *  The number of blocks that our disk will have
 *
 * @return
 *  the (fully unallocated) characteristic vector
 *  
 *  Create the initial FAT record
 */
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

/**
 * Writes out blank entries across the whole FAT
 */
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


/**
 * @param size
 *  The number of blocks to zero
 *
 *  zero the entirety of every block
 */
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


/**
 * @param vDisk
 *  A reference to the virtual disk
 * @param fileName
 *  The file to acquire the lock on
 * @return 
 *  1 - if the file is already locked
 *  0 - if we've acquired the lock.
 *
 *  Locks a file so that it cannot be accessed while in use.
 */
int acquire_lock(FILE* vDisk, char* fileName){
    
    // start from the beginning of the disk
    rewind(vDisk);
    
    
    // load the fat
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    // get char vector
    fat.free = (char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    // find the entry to lock
    struct FAT_entry entry;
    
    int lock_entry;
    for(lock_entry=0; lock_entry<fat.fat_size; lock_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
        
        if (strcmp(entry.filename, fileName) == 0)
            break;
        
    }
    
    
    if(lock_entry >= fat.fat_size){
        printf("Could not find file (%s) for locking. \n", fileName);
        rewind(vDisk);
        return 1;
    }else{
        if (entry.locked == 1){
            printf("Unable to lock file (%s) \n", fileName);
        }else{
            // I think I've seeked past the entry I want to overwrite
            // (the one I currently have.) make the change, seek backwards
            // write.
            entry.locked = 1;
            fseek(vDisk, -(sizeof(struct FAT_entry)), SEEK_CUR);
            fwrite(&entry, sizeof(struct FAT_entry), 1, vDisk);
            rewind(vDisk);
        }
    }
    
    return 0;
}


/**
 * @param vDisk
 *  A reference to the virtual disk
 * @param fileName
 *  The file to acquire the lock on
 * @return
 *  Always returns 0, unless there is an error.
 *
 *  Releases the lock on a file so that it can be accessed again.
 */
int release_lock(FILE* vDisk, char* fileName){
    // start from the beginning of the disk
    rewind(vDisk);
    
    // load the fat
    struct FAT fat;
    fread(&fat, sizeof(struct FAT), 1, vDisk);
    
    // get char vector
    fat.free = (char*)malloc(fat.vfree_length);
    fread(fat.free, fat.vfree_length, 1, vDisk);
    
    // find the entry to lock
    struct FAT_entry entry;
    
    int lock_entry;
    for(lock_entry=0; lock_entry<fat.fat_size; lock_entry++){
        fread(&entry, sizeof(struct FAT_entry), 1, vDisk);
        
        if (strcmp(entry.filename, fileName) == 0)
            break;
        
    }
    
    
    if (lock_entry > fat.fat_size){
        printf("Could not find file (%s) for unlocking. \n", fileName);
        rewind(vDisk);
        return 1;
    }else{
        if (entry.locked == 0)
            return 0;  // it was already unlocked, that makes things easy.
        
        // just like when locking, we've already seeked past the entry.
        // so we want to seek backwards a bit and save there.
        entry.locked = 0;
        fseek(vDisk, -(sizeof(struct FAT_entry)), SEEK_CUR);
        fwrite(&entry, sizeof(struct FAT_entry), 1, vDisk);
        rewind(vDisk);
    }
    return 0;
}