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
    
    
    create("testDisk", "tes3tfile", 51200);
    create("testDisk", "te3stfile", 51200);
    create("testDisk", "t3estfile", 51200);
    create("testDisk", "testfil1e", 51200);
    create("testDisk", "testfi1le", 51200);
    create("testDisk", "testf1ile", 51200);
    create("testDisk", "test1file", 51200);
    create("testDisk", "tes1tfile", 51200);
    create("testDisk", "te1stfile", 51200);
    create("testDisk", "t1estfile", 5000);
    
    // can only allocate two more blocks
    create("testDisk", "t1estfiewle", 512);









    info("testDisk");


}

