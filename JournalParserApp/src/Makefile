TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library
LIBRARY_IOC += JournalParserSup
PROD_IOC += JournalParser JournalTestPost

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install JournalParser.dbd into <top>/dbd
#DBD += JournalParser.dbd
USR_CPPFLAGS += -I$(SLACKING)/include/slacking -I$(BOOST)/include 
USR_CPPFLAGS_WIN32 += -DNOMINMAX

ifeq ($(STATIC_BUILD),YES)
USR_CPPFLAGS += -DCURL_STATICLIB -DSTATIC_CONCPP
endif

# specify all source files to be compiled and added to the library
JournalParserSup_SRCS += JournalParserSup.cpp

JournalParserSup_LIBS += pugixml libjson
JournalParserSup_LIBS_WIN32 += libcurl
JournalParserSup_SYS_LIBS_Linux += curl
JournalParserSup_LIBS += $(EPICS_BASE_IOC_LIBS) $(MYSQLLIB)

JournalParser_LIBS += JournalParserSup $(MYSQLLIB) utilities
JournalParser_LIBS += pugixml zlib libjson
JournalParser_LIBS_WIN32 += libcurl
JournalParser_SYS_LIBS_WIN32 += wldap32 crypt32 Normaliz
JournalParser_SYS_LIBS_Linux += curl
JournalParser_LIBS += $(EPICS_BASE_IOC_LIBS)

JournalTestPost_LIBS += JournalParserSup $(MYSQLLIB) utilities
JournalTestPost_LIBS += pugixml zlib libjson
JournalTestPost_LIBS_WIN32 += libcurl
JournalTestPost_SYS_LIBS_WIN32 += wldap32 crypt32 Normaliz
JournalTestPost_SYS_LIBS_Linux += curl
JournalTestPost_LIBS += $(EPICS_BASE_IOC_LIBS)

# add some DLLs so easier to run standalone
BIN_INSTALLS_WIN32 += $(wildcard $(MYSQL)/bin/$(EPICS_HOST_ARCH)/*.dll)
BIN_INSTALLS_WIN32 += $(wildcard $(PUGIXML)/bin/$(EPICS_HOST_ARCH)/*.dll)
BIN_INSTALLS_WIN32 += $(wildcard $(CURL)/bin/$(EPICS_HOST_ARCH)/*.dll)

# optional extra local definitions here



# ifeq ($(SHARED_LIBRARIES),YES)
# USR_CPPFLAGS_WIN32 += -DSQLITE_API=__declspec(dllimport)
# else
# #USR_CXXFLAGS += -DCPPCONN_LIB_BUILD
# endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

