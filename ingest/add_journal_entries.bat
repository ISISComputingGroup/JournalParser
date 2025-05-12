setlocal
call %~dp0..\..\..\..\config_env.bat

REM disable slack/teams messages
set "JOURNALPARSER_NOMESSAGE=1"

"%PYTHON3%" -u %~dp0add_journal_entries.py %*

if %errorlevel% neq 0 exit /b %errorlevel%
