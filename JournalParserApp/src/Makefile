TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += JournalParserSup

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install JournalParser.dbd into <top>/dbd
#DBD += JournalParser.dbd
USR_CPPFLAGS += -I$(SLACKING)/include/slacking -I$(BOOST)/include 
USR_CPPFLAGS_WIN32 += -DNOMINMAX

# specify all source files to be compiled and added to the library
JournalParserSup_SRCS += JournalParserSup.cpp

JournalParserSup_LIBS += pugixml
JournalParserSup_LIBS_WIN32 += libcurl
JournalParserSup_SYS_LIBS_Linux += curl
JournalParserSup_LIBS += $(EPICS_BASE_IOC_LIBS)

PROD_IOC += JournalParser
JournalParser_LIBS += JournalParserSup 
JournalParser_LIBS += pugixml
JournalParser_LIBS_WIN32 += libcurl
JournalParser_SYS_LIBS_Linux += curl
JournalParser_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

