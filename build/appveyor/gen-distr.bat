@echo off

set "OUTDIR=%APPVEYOR_BUILD_FOLDER%\out"

cd "%OUTDIR%"
cscript gen_distr_%CONFIGURATION%.vbs
cd "%APPVEYOR_BUILD_FOLDER%"

set "PAXZ_DISTR_NAME=wcx_paxz_v%PAXZ_VERSION%%DEBUG_SUFFIX%.zip"
set "PAXZ_DISTR_FILE=%OUTDIR%\%PAXZ_DISTR_NAME%"

if not exist "%PAXZ_DISTR_FILE%" (
  echo ***ERROR: can't find output file "%PAXZ_DISTR_NAME%"
  exit /b 1
)

echo Distro Name: "%PAXZ_DISTR_NAME%"

:: -------------------------------------------------------------------------

if /i "%APPVEYOR_REPO_TAG%" neq "true" exit

:: -------------------------------------------------------------------------

set "TAG_FS=%APPVEYOR_REPO_TAG_NAME:~0,1%"
if "%TAG_FS%" neq "v" (
  echo ***ERROR: incorrect TAG name "%APPVEYOR_REPO_TAG_NAME%"
  exit /b 1
)
set "REPO_TAG=%APPVEYOR_REPO_TAG_NAME:~1%"

set REPO_TAG_SUFFIX=

for /F "tokens=1,2,3 delims=.- " %%a in ("%REPO_TAG%") do (
  set "REPO_TAG_MAJOR=%%a"
  set "REPO_TAG_MINOR=%%b"
  set "REPO_TAG_SUFFIX=%%c"
)

echo REPO_TAG: "%REPO_TAG_MAJOR%.%REPO_TAG_MINOR%-%REPO_TAG_SUFFIX%"

if "%PAXZ_VER_MAJOR%" neq "%REPO_TAG_MAJOR%" (
  echo ***ERROR: Repo tag version incorrect!
  exit /b 1
)  
if "%PAXZ_VER_MINOR%" neq "%REPO_TAG_MINOR%" (
  echo ***ERROR: Repo tag version incorrect!
  exit /b 1
)  

set "PAXZ_RELEASE_VER=%PAXZ_VER_MAJOR%.%PAXZ_VER_MINOR%"
if "%REPO_TAG_SUFFIX%" neq "" (
  set "PAXZ_RELEASE_VER=%PAXZ_RELEASE_VER%%REPO_TAG_SUFFIX%"
)

set "PAXZ_RELEASE_NAME=wcx_paxz_v%PAXZ_RELEASE_VER%%DEBUG_SUFFIX%"
set "PAXZ_RELEASE_FILE=%PAXZ_RELEASE_NAME%.zip"

rename "%PAXZ_DISTR_FILE%" "%PAXZ_RELEASE_FILE%"
set "PAXZ_DISTR_FILE=%OUTDIR%\%PAXZ_RELEASE_FILE%"

set "DEPLOY_VER=%PAXZ_VER_MAJOR%.%PAXZ_VER_MINOR%%REPO_TAG_SUFFIX%"
set "DEPLOY_TAG=%APPVEYOR_REPO_TAG_NAME%"
set "DEPLOY_NAME=PAXZ %PAXZ_VER_MAJOR%.%PAXZ_VER_MINOR%"
if "%REPO_TAG_SUFFIX%" neq "" (
  set "DEPLOY_NAME=%DEPLOY_NAME% %REPO_TAG_SUFFIX%"
)
set DEPLOY_DRAFT=false
set DEPLOY_PREREL=false

if "%REPO_TAG_SUFFIX%" neq "" (
  set DEPLOY_DRAFT=true
)
set /a VER_MINOR=%PAXZ_VER_MINOR% + 0
SET /a VER_MINOR_MOD=VER_MINOR %% 2
if VER_MINOR_MOD == 1 (
  set DEPLOY_PREREL=true
)

set "DEPLOY_DESC=PAXZ - packer plugin for Total Commander"
set "DEPLOY_DESC=%DEPLOY_DESC%\n\n[![Github Release](https://img.shields.io/github/downloads/remittor/paxz.wcx/%DEPLOY_TAG%/total.svg)]"
set "DEPLOY_DESC=%DEPLOY_DESC%(https://www.github.com/remittor/paxz.wcx/releases/%DEPLOY_TAG%)"

echo -------------------------------------
echo DEPLOY_FILE:  %PAXZ_RELEASE_FILE%
echo DEPLOY_TAG:   %DEPLOY_TAG%
echo DEPLOY_NAME:  %DEPLOY_NAME%
echo DEPLOY_DRAFT: %DEPLOY_DRAFT% 
echo -------------------------------------


