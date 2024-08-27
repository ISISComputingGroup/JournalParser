/*************************************************************************\ 
* Copyright (c) 2013 Science and Technology Facilities Council (STFC), GB. 
* All rights reverved. 
* This file is distributed subject to a Software License Agreement found 
* in the file LICENSE.txt that is included with this distribution. 
\*************************************************************************/ 

/// @file webgetDriver.cpp Implementation of #webgetDriver class and webgetConfigure() iocsh command
/// @author Freddie Akeroyd, STFC ISIS Facility, GB

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <exception>
#include <iostream>
#include <map>

#include <epicsTypes.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTimer.h>
#include <epicsMutex.h>
#include <epicsEvent.h>
#include <errlog.h>
#include <iocsh.h>
#include <epicsExit.h>
#include <shareLib.h>

#include "JournalParserSup.h"

#include <epicsExport.h>

int main(int argc, char* argv[])
{
	time_t time1, time2;
	time(&time1);
	if (argc < 6)
	{
		std::cerr << "JournalParser: Not enough arguments" << std::endl;
		return -1;
	}
	std::string file_prefix = argv[1]; // could be inst name or inst short name
	std::string run_number = argv[2];  // 5 or 8 digit with leading zeros, or * to do all in cycle file
	std::string isis_cycle = argv[3];  // e.g. cycle_14_2
	std::string journal_dir = argv[4];  // e.g. c:\data\export only
	std::string computer_name = argv[5];  // e.g. NDXGEM
    int stat = parseJournal(file_prefix, run_number, isis_cycle, journal_dir, computer_name);
	time(&time2);
	std::cout << "JournalParser: took " << difftime(time2, time1) << " seconds" << std::endl;
	return stat;
}
