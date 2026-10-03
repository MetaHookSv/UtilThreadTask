@echo off
setlocal
set "Configuration=Release"
call "%~dp0build-UtilThreadTask-x86.bat" %*
exit /b %errorlevel%
