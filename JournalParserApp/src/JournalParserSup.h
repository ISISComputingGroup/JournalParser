/*************************************************************************\ 
* Copyright (c) 2013 Science and Technology Facilities Council (STFC), GB. 
* All rights reverved. 
* This file is distributed subject to a Software License Agreement found 
* in the file LICENSE.txt that is included with this distribution. 
\*************************************************************************/ 

#ifndef JOURNAL_PARSER_H
#define JOURNAL_PARSER_H

epicsShareExtern int parseJournal(const std::string& file_prefix, const std::string& run_number, const std::string& isis_cycle, const std::string& journal_dir, const std::string& computer_name);

epicsShareExtern void sendSlackAndTeamsMessage(std::string inst_name, std::string slack_mess, std::string teams_mess, std::string summ_mess);

#endif /* JOURNAL_PARSER_H */

