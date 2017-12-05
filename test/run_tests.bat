call ..\..\..\..\config_env.bat

set "path=C:\Instrument\Apps\EPICS\support\pugixml\master\bin\windows-x64;C:\Instrument\Apps\EPICS\support\curl\master\bin\windows-x64;C:\Instrument\Apps\EPICS\support\MySQL\master\bin\windows-x64;%path%"

%python% test_journal_parser.py
