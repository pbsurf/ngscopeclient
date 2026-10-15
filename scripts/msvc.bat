@echo off
rem Run a command in the VS 2022 Build Tools x64 dev environment, from the root of the repository.
rem Usage: scripts\msvc.bat <command...>   e.g. scripts\msvc.bat cmake --preset windows-msvc
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -no_logo
if errorlevel 1 (
	echo VsDevCmd.bat failed
	exit /b 1
)
rem Git isn't installed on Windows, but vcpkg downloads its own. CMake uses it for the version string (git describe).
for /d %%G in ("%LOCALAPPDATA%\vcpkg\downloads\tools\git-*-windows") do if exist "%%G\cmd\git.exe" set "PATH=%%G\cmd;%PATH%"
cd /d "%~dp0.." || exit /b 1
%*
