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

static std::string exeCWD()
{
    char buffer[MAX_PATH];
    GetModuleFileName(NULL, buffer, MAX_PATH);
    size_t pos = std::string(buffer).find_last_of( "\\/" );
    return std::string(buffer).substr(0, pos);
}

void sendSlackMessage(std::string inst_name, std::string mess)
{
	boost::to_lower(inst_name);
	std::string slack_channel = "#journal_" + inst_name;
//	std::string slack_channel = "#test";
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
            slack.chat.postMessage(mess);
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
}

/**
 * Writes to the database based on the contents of an XML node.
 *
 * Args: 
 *     entry: The XML node representing the entry to be written to the database.
 *
 * Returns: 
 *     0 if successful, non-zero if unsuccessful.
 */
int writeToDatabase(pugi::xml_node& entry)
{
	// Must match with the number of columns in SQL database and also with list of XML attributes below.
	const int number_of_elements = 43; 
	
	// List of items to extract from XML.
	// Should be in the same order as the columns in the database.
	const char* xml_names[number_of_elements] = {
		"run_number",
		"title",
		"start_time",
		"duration",
		"proton_charge",
		"experiment_identifier",
		"user_name",
		"simulation_mode",
		"local_contact",
		"user_institute",
		"instrument_name",
		"sample_id",
		"measurement_first_run",
		"measurement_id",
		"measurement_label",
		"measurement_type",
		"measurement_subid",
		"end_time",
		"raw_frames",
		"good_frames",
		"number_periods",
		"number_spectra",
		"number_detectors",
		"number_time_regimes",
		"frame_sync",
		"icp_version",
        "detector_table_file",
        "spectra_table_file",
        "wiring_table_file",
        "monitor_spectrum",
        "monitor_sum",
        "total_mevents",
        "comment",
        "field_label",
        "instrument_geometry",
        "script_name",
        "sample_name",
        "sample_orientation",
        "temperature_label",
        "npratio_average",
        "isis_cycle",
        "seci_config",
        "event_mode"
	};

	std::string data[number_of_elements];
	
	// Get value of XML node and trim whitespace.
	int i;
	for (i=0; i<number_of_elements; i++)
	{
		data[i] = trimXmlNode(entry, xml_names[i]);
	}
	
	try
	{
		sql::Driver * mysql_driver = sql::mysql::get_driver_instance();
		std::auto_ptr< sql::Connection > con(mysql_driver->connect("localhost", "journal", "$journal"));
		std::auto_ptr<sql::Statement> stmt(con->createStatement());
		con->setAutoCommit(0);
		con->setSchema("journal");
		sql::PreparedStatement *prep_stmt;
		
		std::string query ("INSERT INTO journal_entries VALUES (");
		// Loop to number_of_elements-1 because last "?" should not have a trailing comma.
		for(i=0; i<number_of_elements-1; i++)
		{
			query.append("?, ");
		}
		query.append("?)");
		
		prep_stmt = con->prepareStatement(query);

		for (i=0; i<number_of_elements; i++) 
		{
			prep_stmt->setString(i+1, data[i]);
		}
		
		prep_stmt->execute();
		con->commit();
	}
	catch (sql::SQLException &e)
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s (MySQL error code: %d, SQLState: %s)\n", e.what(), e.getErrorCode(), e.getSQLStateCStr());
        return -1;
	}
	catch (std::runtime_error &e)
	{
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: %s\n", e.what());
        return -1;
	}
    catch(...)
    {
        errlogSevPrintf(errlogMinor, "pvdump: MySQL ERR: FAILED TRYING TO WRITE TO THE ISIS PV DB\n");
        return -1;
    }
	return 0;
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
	char main_entry_xpath[128];
	sprintf(main_entry_xpath, "/NXroot/NXentry[@name='%s%08d']", inst_name.c_str(), atoi(run_number.c_str()));
    pugi::xpath_node main_entry = doc.select_single_node(main_entry_xpath);
	std::ostringstream mess;
	pugi::xml_node& entry = main_entry.node();
	// we need to have title in a ``` so if it contains markdown like
	// characters they are not interpreted
	const char* collect_mode = (atof(getJV(entry, "event_mode").c_str()) > 0.0 ? "*event* mode" : "*histogram* mode");
	time_t now;
	time(&now);
	char tbuffer[64];
	strftime(tbuffer, sizeof(tbuffer), "%a %d %b %H:%M", localtime(&now));
	// << getJV(entry, "monitor_sum") << "* monitor spectrum " << getJV(entry, "monitor_spectrum") << " sum, *"
	mess << tbuffer << " Run *" << getJV(entry, "run_number") << "* finished (*" << getJV(entry, "proton_charge") << "* uAh, *" << getJV(entry, "good_frames") << "* frames, *" << getJV(entry, "duration") << "* seconds, *" << getJV(entry, "number_spectra") << "* spectra, *" << getJV(entry, "number_periods") << "* periods, " << collect_mode << ", *" << getJV(entry, "total_mevents") << "* total DAE MEvents) ```" << getJV(entry, "title") << "```";
	std::cerr << mess.str() << std::endl;

	// sendSlackMessage(inst_name, mess.str());
	
	return writeToDatabase(entry);
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
