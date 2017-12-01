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

int writeToDatabase(const std::string run_number, const std::string title, const std::string start_time, const std::string duration, const std::string uamps, const std::string rb_num, const std::string users, const std::string simulation_mode, const std::string local_contact, const std::string user_institute, const std::string instrument_name, const std::string sample_id, const std::string measurement_first_run, const std::string measurement_id, const std::string measurement_label, const std::string measurement_type, const std::string measurement_subid, const std::string end_time, const std::string raw_frames, const std::string good_frames, const std::string number_periods, const std::string number_spectra, const std::string number_detectors, const std::string number_time_regimes, const std::string frame_sync)
{
	try
	{
		sql::Driver * mysql_driver = sql::mysql::get_driver_instance();

		std::auto_ptr< sql::Connection > con(mysql_driver->connect("localhost", "journal", "$journal"));

		std::auto_ptr<sql::Statement> stmt(con->createStatement());

		con->setAutoCommit(0);
		con->setSchema("journal");

		sql::PreparedStatement *prep_stmt;

		prep_stmt = con->prepareStatement("INSERT INTO journal_entries VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

		prep_stmt->setString(1, run_number);
		prep_stmt->setString(2, title);
		prep_stmt->setString(3, start_time);
		prep_stmt->setString(4, duration);
		prep_stmt->setString(5, uamps);
		prep_stmt->setString(6, rb_num);
		prep_stmt->setString(7, users);
    prep_stmt->setString(8, simulation_mode);
    prep_stmt->setString(9, local_contact);
    prep_stmt->setString(10, user_institute);
    prep_stmt->setString(11, instrument_name);
    prep_stmt->setString(12, sample_id);
    prep_stmt->setString(13, measurement_first_run);
    prep_stmt->setString(14, measurement_id);
    prep_stmt->setString(15, measurement_label);
    prep_stmt->setString(16, measurement_type);
    prep_stmt->setString(17, measurement_subid);
    prep_stmt->setString(18, end_time);
    prep_stmt->setString(19, raw_frames);
    prep_stmt->setString(20, good_frames);
    prep_stmt->setString(21, number_periods);
    prep_stmt->setString(22, number_spectra);
    prep_stmt->setString(23, number_detectors);
    prep_stmt->setString(24, number_time_regimes);
    prep_stmt->setString(25, frame_sync);
		prep_stmt->execute();

		con->commit();
		puts("Did mysql successfully");
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


	std::string run_id = trimXmlNode(entry, "run_number");
	std::string title = trimXmlNode(entry, "title");
	std::string start_time = trimXmlNode(entry, "start_time");
	std::string duration = trimXmlNode(entry, "duration");
	std::string uamps = trimXmlNode(entry, "proton_charge");
	std::string rb_number = trimXmlNode(entry, "experiment_identifier");
	std::string users = trimXmlNode(entry, "user_name");
  std::string simulation_mode = trimXmlNode(entry, "simulation_mode");
  std::string local_contact = trimXmlNode(entry, "local_contact");
  std::string user_institute = trimXmlNode(entry, "user_institute");
  std::string instrument_name = trimXmlNode(entry, "instrument_name");
  std::string sample_id = trimXmlNode(entry, "sample_id");
  std::string measurement_first_run = trimXmlNode(entry, "measurement_first_run");
  std::string measurement_id = trimXmlNode(entry, "measurement_id");
  std::string measurement_label = trimXmlNode(entry, "measurement_label");
  std::string measurement_type = trimXmlNode(entry, "measurement_type");
  std::string measurement_subid = trimXmlNode(entry, "measurement_subid");
  std::string end_time = trimXmlNode(entry, "end_time");
  std::string raw_frames = trimXmlNode(entry, "raw_frames");
  std::string good_frames = trimXmlNode(entry, "good_frames");
  std::string number_periods = trimXmlNode(entry, "number_periods");
  std::string number_spectra = trimXmlNode(entry, "number_spectra");
  std::string number_detectors = trimXmlNode(entry, "number_detectors");
  std::string number_time_regimes = trimXmlNode(entry, "number_time_regimes");
  std::string frame_sync = trimXmlNode(entry, "frame_sync");

	return writeToDatabase(run_id, title, start_time, duration, uamps, rb_number, users, simulation_mode, local_contact, user_institute, instrument_name, sample_id, measurement_first_run, measurement_id, measurement_label, measurement_type, measurement_subid, end_time, raw_frames, good_frames, number_periods, number_spectra, number_detectors, number_time_regimes, frame_sync);
}

/// file_prefix = argv[1]; // could be inst name or inst short name
/// run_number = argv[2];  // 5 or 8 digit with leading zeros
/// isis_cycle = argv[3];  // e.g. cycle_14_2
///	const char* journal_dir = argv[4];  // e.g. c:\data\export only
/// computer_name = argv[5];  // e.g. NDXGEM
int parseJournal(const std::string& file_prefix, const std::string& run_number, const std::string& isis_cycle, const std::string& journal_dir, const std::string& computer_name)
{
	std::string inst_name = computer_name.substr(3); // after NDX
	int success;
	success = createJournalFile(file_prefix, run_number, isis_cycle, journal_dir, computer_name, inst_name);
	if (success != 0)
	{
		return success;
	}

	return 0;
}
