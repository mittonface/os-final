//
//  main.c
//  OS_final
//
//  Created by Brent Mitton on 2014-04-04.
//  Copyright (c) 2014 Brent Mitton. All rights reserved.
//

#include "lowlevel.h"
#include "fsutils.h"

int main(int argc, const char * argv[])
{

    format("testDisk", 1000);
    copy("testDisk", "lowlevel.c");
    create("testDisk", "heynow", 10000);
    resize("testDisk", "lowlevel.c", 30000);
    //copy("testDisk", "lowlevel.h");

    //create("testDisk", "hey", 2048);
    //moveFile("testDisk", "lowlevel.c", 100);
    //resize("testDisk", "hey", 5000);
    //readFile("testDisk", "lowlevel.c");
    //readFile("testDisk", "lowlevel.h");

    //retrieve("testDisk", "lowlevel.c");
    //retrieve("testDisk", "lowlevel.h");

    list_block_entries("testDisk");
    
    info("testDisk");
}

