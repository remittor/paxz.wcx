@echo off

set DEBUG_SUFFIX=
if /i "%CONFIGURATION%" == "Debug" (
  set "DEBUG_SUFFIX=_debug"
)

if /i "%TOOLCHAIN%" == "vs2008" (
  set "TOOLCHAINVER=msvc9"
  set "SLN_NAME=paxz.wcx.VS2008.sln"
  set "MS_BLD_TOOLSET=v90"
  set "MSVC_GENERATOR=Visual Studio 2008"
  set "MS_VS_DIR=C:\Program Files (x86)\Microsoft Visual Studio 9.0"
  set "MS_BLD_DIR=C:\Windows\Microsoft.NET\Framework\v3.5"
) else (
  set "TOOLCHAIN=vs2015"
  set "TOOLCHAINVER=msvc14"
  set "SLN_NAME=paxz.wcx.sln"
  set "MS_BLD_TOOLSET=v140"
  set "MSVC_GENERATOR=Visual Studio 2015"
  set "MS_VS_DIR=C:\Program Files (x86)\Microsoft Visual Studio 14.0"
  set "MS_BLD_DIR=C:\Program Files (x86)\MSBuild\14.0\Bin"
)

set "AVLOGGER=C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"
set "MS_SDK_DIR=C:\Program Files\Microsoft SDKs\Windows\v7.1"

set "ORIG_PATH=%PATH%"

set "PATH=%MS_SDK_DIR%\bin;%PATH%"
set "PATH=%MS_BLD_DIR%;%PATH%"
set "PATH=%MS_VS_DIR%\Common7\IDE;%PATH%"

set "INCLUDE=%MS_SDK_DIR%\include;%INCLUDE%"
set "INCLUDE=%MS_VS_DIR%\VC\include;%INCLUDE%"

echo -------------------------------------
echo REPO_BRANCH:       %APPVEYOR_REPO_BRANCH%
echo TOOLCHAIN:         %TOOLCHAIN%
echo CONFIGURATION:     %CONFIGURATION%
echo PAXZ_VERSION:      %PAXZ_VERSION%
echo APPVEYOR_REPO_TAG: %APPVEYOR_REPO_TAG%
if /i "%APPVEYOR_REPO_TAG%" == "true" (
  echo APPVEYOR_REPO_TAG: "%APPVEYOR_REPO_TAG_NAME%"
)
echo -------------------------------------
