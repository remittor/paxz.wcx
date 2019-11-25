@echo off

set "THIS_DIR=%CD%"
set "SAVED_PATH=%PATH%"

set MS_BLD_LL=m
set MS_BLD_AUX=
if "%TOOLCHAIN%" == "vs2008" (
  set MS_BLD_AUX="/p:VCBuildUseEnvironment=true"
)

:: ---------------------------------------------------

set TARGET_CPU=x86
set PLATFORM=Win32
set "PATH=%MS_VS_DIR%\VC\bin;%SAVED_PATH%"
set "LIB=%MS_SDK_DIR%\lib;%MS_VS_DIR%\VC\lib;"
ECHO ***
ECHO *** Building %MSVC_GENERATOR% %PLATFORM%\%CONFIGURATION% in %APPVEYOR_BUILD_FOLDER%
ECHO ***
msbuild %SLN_NAME% /m /verbosity:%MS_BLD_LL% /p:PlatformToolset=%MS_BLD_TOOLSET% /t:Clean,Build /p:Platform=%PLATFORM% /p:Configuration=%CONFIGURATION% /logger:"%AVLOGGER%" %MS_BLD_AUX%

:: ---------------------------------------------------

set TARGET_CPU=amd64
set PLATFORM=x64
set "PATH=%MS_VS_DIR%\VC\bin\amd64;%SAVED_PATH%"
set "LIB=%MS_SDK_DIR%\lib\x64;%MS_VS_DIR%\VC\lib\amd64;"
if "%TOOLCHAIN%" == "vs2008" (
  set VC_PROJECT_ENGINE_NOT_USING_REGISTRY_FOR_INIT=1
)
ECHO ***
ECHO *** Building %MSVC_GENERATOR% %PLATFORM%\%CONFIGURATION% in %APPVEYOR_BUILD_FOLDER%
ECHO ***
msbuild %SLN_NAME% /m /verbosity:%MS_BLD_LL% /p:PlatformToolset=%MS_BLD_TOOLSET% /t:Clean,Build /p:Platform=%PLATFORM% /p:Configuration=%CONFIGURATION% /logger:"%AVLOGGER%" %MS_BLD_AUX%

:: ---------------------------------------------------

echo ***** Build OK *****
