@echo off

set VSCMD_START_DIR=%cd%
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -FC -Zi /std:c++17 /W4 ..\handmade\code\win32_handmade.cpp /link user32.lib gdi32.lib
popd