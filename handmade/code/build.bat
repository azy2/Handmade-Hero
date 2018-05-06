@echo off

set VSCMD_START_DIR=%cd%
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

pushd ..\..\build
cl -FC -Zi ..\handmade\code\win32_handmade.cpp /link user32.lib gdi32.lib
popd