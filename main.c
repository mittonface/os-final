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
    readFile("testDisk", "lowlevel.c");
    retrieve("testDisk", "lowlevel.c");

    info("testDisk");
}

