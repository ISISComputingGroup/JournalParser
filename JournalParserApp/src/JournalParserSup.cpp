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
#ifndef _WIN32
#include <sys/stat.h>
#endif
#include <exception>
#include <iostream>
#include <fstream>
#include <map>

#include <stdexcept>
#include <string>
#include <time.h>
#include <sstream>

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

#include "macLib.h"

// mysql
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/warning.h>
#include <cppconn/metadata.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/statement.h>
#include "mysql_driver.h"
#include "mysql_connection.h"

// CURL
#include <curl/curl.h>
#include <curl/easy.h>

#include <libjson.h>

#include <pugixml.hpp>
#include <boost/algorithm/string.hpp>
#include <slacking.hpp>

#include <epicsExport.h>

#include "JournalParserSup.h"

/**
 * Gets the value of an XML node without any whitespace around it.
 *
 * Args: 
 *    node: The node to get the value from.
 *    name: The name of the value to get.
 *
 * Returns:
 *    The value with no leading or trailing whitespace.
 */
static std::string trimXmlNode(pugi::xml_node& node, const std::string& name)
{
    std::string value = node.child_value(name.c_str());
    boost::trim(value);
    return value;
}

static std::string getJV(pugi::xml_node& node, const std::string& name)
{
	std::string value = trimXmlNode(node, name);
	// always needed for slack messages
    boost::replace_all(value, "&", "&amp;");
    boost::replace_all(value, "<", "&lt;");
    boost::replace_all(value, ">", "&gt;");
    return value;
}

static void escapeJSON(std::string& str)
{
    boost::replace_all(str, "\\", "\\\\");
    boost::replace_all(str, "\"", "\\\"");
}

static std::string exeCWD()
{
#ifdef _WIN32
    char buffer[MAX_PATH];
    buffer[0] = '\0';
    GetModuleFileName(NULL, buffer, sizeof(buffer));
#else
    char buffer[256];
    int n = readlink("/proc/self/exe", buffer, sizeof(buffer)-1);
    if (n >= 0)
    {
       buffer[n] = '\0';
    }
    else
    {
        strcpy(buffer,"<unknown>");
    }
#endif
    size_t pos = std::string(buffer).find_last_of( "\\/" );
    return std::string(buffer).substr(0, pos);
}

static void sendSlackAndTeamsMessage(std::string inst_name, std::string slack_mess, std::string teams_mess, std::string summ_mess)
{
	boost::to_lower(inst_name);
	std::string slack_channel = "#journal_" + inst_name;
//	std::string slack_channel = "#test";
	std::string config_file = exeCWD() + "\\JournalParser.conf";
	std::ifstream fs;
    std::string slack_api_token, teams_url;
	fs.open(config_file.c_str(), std::ios::in);
	if (fs.good())
	{
		std::getline(fs, slack_api_token);
		std::getline(fs, teams_url);
		fs.close();
    }
	else
	{
		std::cerr << "JournalParser: cannot open \"" << config_file << "\"" << std::endl;
	}
    if (slack_api_token.size() > 0)
    {
		try
		{
	        auto& slack = slack::create(slack_api_token);
            slack.chat.channel = slack_channel;
	        slack.chat.as_user = true;
            slack.chat.postMessage(slack_mess);
		}
		catch(const std::exception& ex)
		{
			std::cerr << "JournalParser: slack error: " << ex.what() << std::endl;
		}
    }
    else
    {
		std::cerr << "JournalParser: no slack token" << std::endl;
    }
    if (teams_url.size() > 0)
    {
        try
        {
            CURL *curl = curl_easy_init();
            if (curl == NULL)
            {
                throw std::runtime_error("curl init error");
            }
            curl_easy_setopt(curl, CURLOPT_URL, teams_url.c_str());
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            escapeJSON(teams_mess);
            escapeJSON(summ_mess);
            std::string jsonstr = std::string("{\"text\":\"") + teams_mess + "\", \"summary\":\"" + summ_mess +  "\"}";
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonstr.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
            CURLcode res = curl_easy_perform(curl);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (res != CURLE_OK)
            {
                throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
            }
        }
        catch(const std::exception& ex)
        {
            std::cerr << "JournalParser: teams error: " << ex.what() << std::endl;
        }
    }
    else
    {
		std::cerr << "JournalParser: no teams URL" << std::endl;
    }
}



// List of items to extract from XML.
// Should be in the same order and same amount of things as the columns in the database.
const char* xml_names[][2] = {
    { "run_number",             "0" },
    {"title",                   "" },
    {"start_time",              "" },
    {"duration",                "0" },
    {"proton_charge",           "0.0" },
    {"experiment_identifier",   "" },
    {"user_name",               "" },
    {"simulation_mode",         "" },
    {"local_contact",           "" },
    {"user_institute",          "" },
    {"instrument_name",         "" },
    {"sample_id",               "" },
    {"measurement_first_run",   "0" },
    {"measurement_id",          "" },
    {"measurement_label",       "" },
    {"measurement_type",        "" },
    {"measurement_subid",       "" },
    {"end_time",                "" },
    {"raw_frames",              "0" },
    {"good_frames",             "0" },
    {"number_periods",          "0" },
    {"number_spectra",          "0" },
    {"number_detectors",        "0" },
    {"number_time_regimes",     "0" },
    {"frame_sync",              "" },
    {"icp_version",             "" },
    {"detector_table_file",     "" },
    {"spectra_table_file",      "" },
    {"wiring_table_file",       "" },
    {"monitor_spectrum",        "0" },
    {"monitor_sum",             "0" },
    {"total_mevents",           "0.0" },
    {"comment",                 "" },
    {"field_label",             "" },
    {"instrument_geometry",     "" },
    {"script_name",             "" },
    {"sample_name",             "" },
    {"sample_orientation",      "" },
    {"temperature_label",       "" },
    {"npratio_average",         "0.0" },
    {"isis_cycle",              "" },
    {"seci_config",             "" },
    {"event_mode",              "0.0" }
};

const int number_of_xml_elements = sizeof(xml_names) / (2 * sizeof(const char*)); 

/**
 * Writes to the database based on the contents of an XML node.
 *
 * Args: 
 *     entry: The XML node representing the entry to be written to the database.
 *     prep_stmt: prepared mysql statement to use
 *
 * Returns: 
 *     0 if successful, non-zero if unsuccessful.
 */
static int writeToDatabase(pugi::xml_node& entry, sql::PreparedStatement *prep_stmt)
{	
	std::string data[number_of_xml_elements];
	
	// Get value of XML node and trim whitespace.
	int i;
	for (i=0; i<number_of_xml_elements; i++)
	{
		data[i] = trimXmlNode(entry, xml_names[i][0]);
		if (data[i] == "")
		{
			data[i] = xml_names[i][1];
		}
	}
	
	try
	{
		for (i=0; i<number_of_xml_elements; i++) 
		{
			prep_stmt->setString(i+1, data[i]);
		}	
		prep_stmt->execute();
	}
	catch (sql::SQLException &e)
	{
        errlogSevPrintf(errlogMinor, "JournalParser: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return -1;
	}
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "JournalParser: MySQL ERR: %s\n", e.what());
        return -1;
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "JournalParser: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
        return -1;
    }
	return 0;
}


static void writeSlackEntry(pugi::xml_node& entry, const std::string& inst_name)
{

	std::ostringstream slack_mess, teams_mess, summ_mess;
	// we need to have title in a ``` so if it contains markdown like
	// characters they are not interpreted
	const char* collect_mode_slack = (atof(getJV(entry, "event_mode").c_str()) > 0.0 ? "*event* mode" : "*histogram* mode");
	const char* collect_mode_teams = (atof(getJV(entry, "event_mode").c_str()) > 0.0 ? "**event** mode" : "**histogram** mode");
	time_t now;
	time(&now);
	char tbuffer[64];
	strftime(tbuffer, sizeof(tbuffer), "%a %d %b %H:%M", localtime(&now));
	// << getJV(entry, "monitor_sum") << "* monitor spectrum " << getJV(entry, "monitor_spectrum") << " sum, *"
	slack_mess << tbuffer << " Run *" << getJV(entry, "run_number") << "* finished (*" << getJV(entry, "proton_charge") << "* uAh, *" << getJV(entry, "good_frames") << "* frames, *" << getJV(entry, "duration") << "* seconds, *" << getJV(entry, "number_spectra") << "* spectra, *" << getJV(entry, "number_periods") << "* periods, " << collect_mode_slack << ", *" << getJV(entry, "total_mevents") << "* total DAE MEvents) ```" << getJV(entry, "title") << "```";
	teams_mess << tbuffer << " Run **" << getJV(entry, "run_number") << "** finished (**" << getJV(entry, "proton_charge") << "** uAh, **" << getJV(entry, "good_frames") << "** frames, **" << getJV(entry, "duration") << "** seconds, **" << getJV(entry, "number_spectra") << "** spectra, **" << getJV(entry, "number_periods") << "** periods, " << collect_mode_teams << ", **" << getJV(entry, "total_mevents") << "** total DAE MEvents) ```" << getJV(entry, "title") << "```";
	std::cerr << slack_mess.str() << std::endl;
    
    summ_mess << getJV(entry, "run_number") << ": " << getJV(entry, "title");

	sendSlackAndTeamsMessage(inst_name, slack_mess.str(), teams_mess.str(), summ_mess.str());
	
}

static int writeEntries(pugi::xpath_node_set& entries, const std::string& inst_name)
{
    int stat = 0, count = 0;
    try {
        sql::Driver * mysql_driver = sql::mysql::get_driver_instance();
        std::auto_ptr< sql::Connection > con(mysql_driver->connect("localhost", "journal", "$journal"));
	    con->setAutoCommit(0);
	    con->setSchema("journal");

        std::auto_ptr<sql::Statement> stmt(con->createStatement());
	    sql::PreparedStatement *prep_stmt;
		
	    std::string query ("INSERT INTO journal_entries VALUES (");
	    // Loop to number_of_elements-1 because last "?" should not have a trailing comma.
	    for(int i=0; i<number_of_xml_elements-1; i++)
	    {
		    query.append("?, ");
	    }
	    query.append("?)");
	    prep_stmt = con->prepareStatement(query);
    
        for (pugi::xpath_node_set::const_iterator it = entries.begin(); it != entries.end(); ++it)
        {
            pugi::xpath_node node = *it;
            pugi::xml_node entry = node.node();
            writeSlackEntry(entry, inst_name);
            stat |= writeToDatabase(entry, prep_stmt);
            ++count;
        }
    	con->commit();
    }
	catch (sql::SQLException &e)
	{
        errlogSevPrintf(errlogMinor, "JournalParser: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return -1;
	}
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "JournalParser: MySQL ERR: %s\n", e.what());
        return -1;
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "JournalParser: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
        return -1;
    }
    if (count == 0) {
		std::cerr << "JournalParser: Cannot find entry in journal file"  << std::endl;
        return -1;
    } else if (stat == -1) {
        std::cerr << "JournalParser: Error writing one of more entries to DB" << std::endl;
    }
    return stat;
}

int createJournalFile(const std::string& file_prefix, const std::string& run_number, const std::string& isis_cycle, const std::string& journal_dir, const std::string& computer_name, const std::string inst_name)
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
#ifdef _WIN32
    FILE* f = _fsopen(journal_file.c_str(), "rt", _SH_DENYNO);
#else
    FILE* f = fopen(journal_file.c_str(), "rt");
#endif
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
	char main_entry_xpath[128];
    if (run_number == "*") {
	    sprintf(main_entry_xpath, "/NXroot/NXentry[starts-with(@name, '%s')]", inst_name.c_str());
    } else {
        sprintf(main_entry_xpath, "/NXroot/NXentry[@name='%s%08d']", inst_name.c_str(), atoi(run_number.c_str()));
    }
    pugi::xpath_node_set entries = doc.select_nodes(main_entry_xpath);
    return writeEntries(entries, inst_name);
}
    
/*
 * Parses a journal file.
 *
 * Args:
 *    file_prefix: Could be either the instrument name or the instrument short name.
 *    run_number: 5 or 8 digit run number, with leading zeros.
 *    isis_cycle: ISIS cycle number (e.g. cycle_14_2)
 *    journal_dir: Directory where the journal file is found (e.g. c:\data\export only)
 *    computer_name: E.g. NDXGEM (note, if using a development machine prefix your machine name again, i.e. input NDWNDW1111 rather than NDW1111)
 *
 * Returns:
 *    0 on success, non-zero on failure.
 */
int parseJournal(const std::string& file_prefix, const std::string& run_number, const std::string& isis_cycle, const std::string& journal_dir, const std::string& computer_name)
{
	std::string inst_name = computer_name.substr(3); // after NDX
	return createJournalFile(file_prefix, run_number, isis_cycle, journal_dir, computer_name, inst_name);
}
