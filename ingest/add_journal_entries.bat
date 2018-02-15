setlocal

call %~dp0..\..\..\..\config_env.bat
set "PATH=C:\Instrument\Apps\EPICS\support\pugixml\master\bin\windows-x64;C:\Instrument\Apps\EPICS\support\curl\master\bin\windows-x64;C:\Instrument\Apps\EPICS\support\MySQL\master\bin\windows-x64;%PATH%"

"%PYTHON%" %~dp0add_journal_entries.py %*

if %errorlevel% neq 0 exit /b %errorlevel%
