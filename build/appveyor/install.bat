@echo off

set "EXT_LIBS=%APPVEYOR_BUILD_FOLDER%\extlibs"

ECHO Install LZ4 project ...
git clone --depth 1 --branch=master https://github.com/lz4/lz4.git %EXT_LIBS%\lz4
