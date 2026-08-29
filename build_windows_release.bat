@echo off
setlocal EnableExtensions

rem Pass --no-pause when calling this script from another automated script.
set "PAUSE_ON_FAILURE=1"
if /I "%~1"=="--no-pause" set "PAUSE_ON_FAILURE=0"

for %%I in ("%~dp0.") do set "SUICUNE_ROOT=%%~fI"
set "RELEASE_DIR=%SUICUNE_ROOT%\Release"
set "RELEASE_EXE=%RELEASE_DIR%\suiCune.exe"
set "BUILD_LOG=%SUICUNE_ROOT%\build_windows_release.log"

if not defined NUMBER_OF_PROCESSORS set "NUMBER_OF_PROCESSORS=1"

set "MSYS2_BASH="
if defined MSYS2_ROOT if exist "%MSYS2_ROOT%\usr\bin\bash.exe" set "MSYS2_BASH=%MSYS2_ROOT%\usr\bin\bash.exe"
if not defined MSYS2_BASH if exist "C:\msys64\usr\bin\bash.exe" set "MSYS2_BASH=C:\msys64\usr\bin\bash.exe"

if not defined MSYS2_BASH (
    echo ERROR: MSYS2 was not found.
    echo Install MSYS2 in C:\msys64, including the UCRT64 GCC, make, SDL2,
    echo SDL2_net, and PhysFS packages described in README.md.
    goto :build_failed
)

if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"
if errorlevel 1 (
    echo ERROR: Could not create "%RELEASE_DIR%".
    goto :build_failed
)

echo Building suiCune for 64-bit Windows in Release mode...
echo Output: "%RELEASE_EXE%"
echo Log:    "%BUILD_LOG%"
echo.

set "MSYSTEM=UCRT64"
set "CHERE_INVOKING=1"

rem Run Bash from the repository directory. CMD captures all Bash output to the
rem log file, then prints the log back to this window. This deliberately avoids
rem piping through tee, which causes quoting/escaping problems across CMD + Bash.
pushd "%SUICUNE_ROOT%"
if errorlevel 1 (
    echo ERROR: Could not enter "%SUICUNE_ROOT%".
    goto :build_failed
)

"%MSYS2_BASH%" -lc "rm -f Release/suiCune.exe && make clean && make -j%NUMBER_OF_PROCESSORS% RELEASE=1 'EXEOUT=-o Release/'" > "%BUILD_LOG%" 2>&1
set "BUILD_RC=%ERRORLEVEL%"

echo -------------------- build output --------------------
if exist "%BUILD_LOG%" type "%BUILD_LOG%"
echo ------------------ end build output ------------------

popd

if not "%BUILD_RC%"=="0" goto :build_failed

if not exist "%RELEASE_EXE%" (
    echo.
    echo ERROR: The build command completed without creating "%RELEASE_EXE%".
    goto :build_failed
)

echo.
echo Build complete:
echo "%RELEASE_EXE%"
echo.
echo Full build log saved to:
echo "%BUILD_LOG%"
exit /b 0

:build_failed
echo.
echo ============================================================
echo ERROR: The Windows Release build failed.
echo ============================================================
echo.
echo Full build log:
echo "%BUILD_LOG%"
echo.
if "%PAUSE_ON_FAILURE%"=="1" (
    echo Press any key to close this window...
    pause >nul
)
exit /b 1
