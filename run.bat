@echo off
if not exist tm_server.exe call build.bat
tm_server.exe --open
