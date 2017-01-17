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
#include <fstream>
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

#include <pugixml.hpp>
#include <boost/algorithm/string.hpp>
#include <slacking.hpp>

#include <epicsExport.h>

#include "JournalParserSup.h"

static std::string getJV(pugi::xml_node& node, const std::string& name)
{ 
	// check title for characters to escape
    std::string value = node.child_value(name.c_str());
    boost::trim(value);
	// always needed for slack messages 
    boost::replace_all(value, "&", "&amp;");
    boost::replace_all(value, "<", "&lt;");
    boost::replace_all(value, ">", "&gt;");
    return value;
}
	
static std::string exeCWD()
{
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    size_t pos = std::string(buffer).find_last_of( "\\/" );
    return std::string(buffer).substr(0, pos);
}

/// file_prefix = argv[1]; // could be inst name or inst short name
/// run_number = argv[2];  // 5 or 8 digit with leading zeros
/// isis_cycle = argv[3];  // e.g. cycle_14_2
///	const char* journal_dir = argv[4];  // e.g. c:\data\export only
/// computer_name = argv[5];  // e.g. NDXGEM
int parseJournal(const std::string& file_prefix, const std::string& run_number, const std::string& isis_cycle, const std::string& journal_dir, const std::string& computer_name)
{
	size_t pos = isis_cycle.find("_");
	if (pos == std::string::npos)
	{
		std::cerr << "JournalParser: Cycle name syntax incorrect" << std::endl;
		return -1;		
	}
	std::string journal_file = journal_dir + "\\journal_" + isis_cycle.substr(pos+1) + ".xml";
    struct stat st;
	if (stat(journal_file.c_str(), &st) != 0)
	{
		std::cerr << "JournalParser: file \"" << journal_file << "\" does not exist" << std::endl;
		return -1;		
	}
    FILE* f = _fsopen(journal_file.c_str(), "rt", _SH_DENYNO);
	if (f == NULL)
	{
		std::cerr << "JournalParser: Error opening \"" << journal_file << "\"" << std::endl;
		return -1;
	}
	size_t journal_size = st.st_size;
	char* buffer = static_cast<char*>(pugi::get_memory_allocation_function()(journal_size));
	size_t n = fread(buffer, 1, journal_size, f);
	fclose(f);
	if (n <= 0)
	{
		std::cerr << "JournalParser: Error reading \"" << journal_file << "\"" << std::endl;
		pugi::get_memory_deallocation_function()(buffer);
		return -1;
	}
	pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_buffer_inplace_own(buffer, n);
	if (!result)
	{
		std::cerr << "JournalParser: Cannot parse \"" << journal_file << "\"" << std::endl; 
		return -1;
    }
    std::string main_entry_xpath = "/NXroot/NXentry[@name='" + file_prefix + run_number + "']";    
    pugi::xpath_node main_entry = doc.select_single_node(main_entry_xpath.c_str());
	std::ostringstream mess;
	pugi::xml_node& entry = main_entry.node();
	// we need to have title in a ``` so if it contains markdown like 
	// characters they are not interpreted
	mess << "Run *" << getJV(entry, "run_number") << "* finished (*" << getJV(entry, "proton_charge") << "* uAh, *" << getJV(entry, "good_frames") << "* frames, *" << getJV(entry, "duration") << "* seconds, *" << getJV(entry, "total_mevents") << "* MEvents) ```" << getJV(entry, "title") << "```";
	std::cerr << mess.str() << std::endl;
	std::string inst_name = computer_name.substr(3); // after NDX
	boost::to_lower(inst_name);
//	std::string slack_channel = "#journal_" + inst_name;
	std::string slack_channel = "#test";
	std::string config_file = exeCWD() + "\\JournalParser.conf";
	std::ifstream fs;
	fs.open(config_file.c_str(), std::ios::in);
	if (fs.good())
	{
		std::string api_token;
		std::getline(fs, api_token);
		fs.close();
		try
		{
	        auto& slack = slack::create(api_token);
            slack.chat.channel = slack_channel;
	        slack.chat.as_user = true;
            slack.chat.postMessage(mess.str());
		}
		catch(const std::exception& ex)
		{
			std::cerr << "JournalParser: slack error: " << ex.what() << std::endl;
		}
	}
	else
	{
		std::cerr << "JournalParser: cannot open \"" << config_file << "\"" << std::endl;
	}
	return 0;
}