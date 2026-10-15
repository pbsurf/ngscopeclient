@echo off
rem Configures the windows-msvc build with the unit tests on. The ";" in the vcpkg features list gets mangled
rem when passed through cmd /c, so it lives here instead.
call "%~dp0msvc.bat" cmake --preset windows-msvc -DBUILD_TESTING=ON "-DVCPKG_MANIFEST_FEATURES=iio;tests"
