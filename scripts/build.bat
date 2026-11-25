@echo off
rem Builds build\windows-msvc with ninja at below normal priority, so the PC stays usable during the build.
rem The compiler and linker processes inherit the priority. The build is just as fast when the PC is otherwise idle.
rem Usage: scripts\build.bat [ninja arguments...]   e.g. scripts\build.bat -j 10, scripts\build.bat ngscopeclient
call "%~dp0msvc.bat" start "" /b /wait /belownormal ninja -C build\windows-msvc %*
