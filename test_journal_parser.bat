REM posts a test message to journal_demo on slack and test channel on ISIS Instrument and beam status Teams
xcopy /y /i \\isis\shares\ISIS_Experiment_Controls\JournalParser\JournalParser.conf %~dp0bin\%EPICS_HOST_ARCH%
%~dp0bin\%EPICS_HOST_ARCH%\JournalParser.exe "DEMO" 00000001 cycle_16_4 \\isis\shares\ISIS_Experiment_Controls\JournalParser NDXDEMO
