setlocal
set "OLDCWD=%CD%"

cd /d %~dp0
call ..\..\..\..\config_env.bat

set "PATH=C:\Instrument\Apps\EPICS\support\pugixml\master\bin\windows-x64;C:\Instrument\Apps\EPICS\support\curl\master\bin\windows-x64;C:\Instrument\Apps\EPICS\support\MySQL\master\bin\windows-x64;%PATH%"

%PYTHON% test_journal_parser.py
set errlev=%errorlevel%

cd/d %OLDCWD%

if %errlev% neq 0 exit /b %errlev%
