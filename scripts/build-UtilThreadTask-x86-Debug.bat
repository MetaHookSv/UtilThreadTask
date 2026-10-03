@echo off
setlocal
set "Configuration=Debug"
call "%~dp0build-UtilThreadTask-x86.bat" %*
exit /b %errorlevel%
