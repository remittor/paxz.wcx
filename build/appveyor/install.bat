@echo off

del /F /Q %APPVEYOR_BUILD_FOLDER%\lz4
ECHO Install LZ4 project ...
git clone --depth 1 --branch=master https://github.com/lz4/lz4.git %APPVEYOR_BUILD_FOLDER%\lz4
