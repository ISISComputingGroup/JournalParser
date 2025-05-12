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
    if (argc < 5)
    {
        std::cerr << "JournalTestPost: Not enough arguments: inst_name slack_message teams_message summ_message" << std::endl;
        return -1;
    }
    std::string inst_name = argv[1]; // could be inst name or inst short name
    std::string slack_mess = argv[2];
    std::string teams_mess = argv[3];
    std::string summ_mess = argv[4];

    sendSlackAndTeamsMessage(inst_name, slack_mess, teams_mess, summ_mess);
    return 0;
}
