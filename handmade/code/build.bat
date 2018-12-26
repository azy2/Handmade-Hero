@echo off

set VSCMD_START_DIR=%cd%
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

IF NOT EXIST ..\..\build mkdir ..\..\build
pushd ..\..\build
cl -MT /Oi -Gm- /GR- -EHa- -nologo -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -FC -Zi -Fm /std:c++17 /W4 /WX /wd4201 /wd4100 /wd4189 ..\handmade\code\win32_handmade.cpp /link -opt:ref user32.lib gdi32.lib
popd