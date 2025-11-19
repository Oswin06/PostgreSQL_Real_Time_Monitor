@echo off
REM PostgreSQL Monitor - Windows Build Script
REM This script compiles the PostgreSQL Real-Time Monitor for Windows

setlocal enabledelayedexpansion

echo ========================================
echo PostgreSQL Monitor - Windows Build Script
echo ========================================
echo.

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo ERROR: CMakeLists.txt not found. Please run this script from the project root.
    echo Current directory: %CD%
    pause
    exit /b 1
)

REM Set default values
set BUILD_TYPE=Release
set CLEAN_BUILD=false
set RUN_AFTER_BUILD=false
set VERBOSE=false
set INSTALL_PREFIX=C:\PostgreSQLMonitor

REM Parse command line arguments
:parse_args
if "%~1"=="" goto args_done
if /i "%~1"=="debug" set BUILD_TYPE=Debug
if /i "%~1"=="release" set BUILD_TYPE=Release
if /i "%~1"=="clean" set CLEAN_BUILD=true
if /i "%~1"=="run" set RUN_AFTER_BUILD=true
if /i "%~1"=="verbose" set VERBOSE=true
if /i "%~1"=="--prefix" (
    shift
    set INSTALL_PREFIX=%~1
)
shift
goto parse_args
:args_done

echo Build Configuration:
echo   Build Type: %BUILD_TYPE%
echo   Clean Build: %CLEAN_BUILD%
echo   Run After Build: %RUN_AFTER_BUILD%
echo   Install Prefix: %INSTALL_PREFIX%
echo.

REM Check for required tools
echo [1/6] Checking for required tools...

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake is not installed or not in PATH.
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)
echo ✓ CMake found

where cl >nul 2>nul
if errorlevel 1 (
    echo WARNING: Visual Studio C++ compiler (cl.exe) not found in PATH.
    echo You may need to run this from a Visual Studio Developer Command Prompt.
    echo Continuing anyway - CMake might find the compiler automatically.
)

REM Check for Qt6
echo [2/6] Checking for Qt6...

REM Try to find Qt6 in common locations
set QT6_FOUND=false
set QT_PATHS[0]=C:\Qt\6.5.0\msvc2019_64
set QT_PATHS[1]=C:\Qt\6.4.0\msvc2019_64
set QT_PATHS[2]=C:\Qt\6.3.0\msvc2019_64
set QT_PATHS[3]=C:\Qt\6.5.0\mingw_64
set QT_PATHS[4]=C:\Qt\6.4.0\mingw_64

for /L %%i in (0,1,4) do (
    if exist "!QT_PATHS[%%i]!\bin\qmake.exe" (
        set QT_PATH=!QT_PATHS[%%i]!
        set QT6_FOUND=true
        echo ✓ Found Qt6 at !QT_PATH!
        goto qt_found
    )
)

:qt_found
if "%QT6_FOUND%"=="false" (
    echo WARNING: Qt6 not found in common locations.
    echo Make sure Qt6 is installed and CMAKE_PREFIX_PATH is set correctly.
    echo Qt6 download: https://www.qt.io/download
)

REM Check for PostgreSQL
echo [3/6] Checking for PostgreSQL...

set PG_FOUND=false
set PG_PATHS[0]=C:\Program Files\PostgreSQL\15
set PG_PATHS[1]=C:\Program Files\PostgreSQL\14
set PG_PATHS[2]=C:\Program Files\PostgreSQL\13
set PG_PATHS[3]=C:\PostgreSQL\15
set PG_PATHS[4]=C:\PostgreSQL\14

for /L %%i in (0,1,4) do (
    if exist "!PG_PATHS[%%i]!\bin\libpq.dll" (
        set PG_PATH=!PG_PATHS[%%i]!
        set PG_FOUND=true
        echo ✓ Found PostgreSQL at !PG_PATH!
        goto pg_found
    )
)

:pg_found
if "%PG_FOUND%"=="false" (
    echo WARNING: PostgreSQL not found in common locations.
    echo Please install PostgreSQL from https://www.postgresql.org/download/windows/
)

REM Clean build if requested
if "%CLEAN_BUILD%"=="true" (
    echo [4/6] Cleaning previous build...
    if exist "build" (
        rmdir /s /q build
        if exist "build" (
            echo WARNING: Could not remove build directory completely.
        ) else (
            echo ✓ Build directory cleaned
        )
    )
)

REM Create build directory
echo [5/6] Creating build directory...
if not exist "build" (
    mkdir build
    if not exist "build" (
        echo ERROR: Failed to create build directory.
        pause
        exit /b 1
    )
)
echo ✓ Build directory ready

cd build

REM Configure with CMake
echo [6/6] Configuring project with CMake...
set CMAKE_CMD=cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" ^
    ..

if "%VERBOSE%"=="true" (
    set CMAKE_CMD=!CMAKE_CMD! --debug-output
)

if "%QT6_FOUND%"=="true" (
    set CMAKE_CMD=!CMAKE_CMD! -DCMAKE_PREFIX_PATH="%QT_PATH%"
)

echo Running: %CMAKE_CMD%
%CMAKE_CMD%

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed.
    echo Please check that all dependencies are installed:
    echo   - Visual Studio 2019/2022 with C++ tools
    echo   - Qt6 development libraries
    echo   - PostgreSQL development libraries (libpq, libpqxx)
    echo.
    echo Common solutions:
    echo   1. Run from Visual Studio Developer Command Prompt
    echo   2. Install vcpkg: vcpkg install qt6 postgresql libpqxx
    echo   3. Set CMAKE_PREFIX_PATH to your Qt6 installation
    pause
    exit /b 1
)

echo ✓ CMake configuration successful

REM Build the project
echo Building project...
set BUILD_CMD=cmake --build . --config %BUILD_TYPE% --parallel

if "%VERBOSE%"=="true" (
    set BUILD_CMD=!BUILD_CMD! --verbose
)

echo Running: %BUILD_CMD%
%BUILD_CMD%

if errorlevel 1 (
    echo.
    echo ERROR: Build failed.
    pause
    exit /b 1
)

echo ✓ Build completed successfully

REM Create distribution directory
echo Creating distribution...
if not exist "dist" mkdir dist

REM Copy executable and required files
copy /Y "bin\%BUILD_TYPE%\Ban_Delta_Breach_Notifier.exe" "dist\" >nul
xcopy /Y /I /E "..\config" "dist\config" >nul
copy /Y "..\README.md" "dist\" >nul

REM Copy Qt6 DLLs if Qt6 was found
if "%QT6_FOUND%"=="true" (
    echo Copying Qt6 runtime files...
    copy /Y "%QT_PATH%\bin\Qt6Core.dll" "dist\" >nul 2>nul
    copy /Y "%QT_PATH%\bin\Qt6Widgets.dll" "dist\" >nul 2>nul
    copy /Y "%QT_PATH%\bin\Qt6Gui.dll" "dist\" >nul 2>nul

    REM Copy Qt6 platform plugins
    if not exist "dist\platforms" mkdir "dist\platforms"
    copy /Y "%QT_PATH%\plugins\platforms\qwindows.dll" "dist\platforms\" >nul 2>nul
)

REM Copy PostgreSQL DLLs if found
if "%PG_FOUND%"=="true" (
    echo Copying PostgreSQL runtime files...
    copy /Y "%PG_PATH%\bin\libpq.dll" "dist\" >nul 2>nul
    copy /Y "%PG_PATH%\bin\libpqxx.dll" "dist\" >nul 2>nul
    copy /Y "%PG_PATH%\bin\libssl.dll" "dist\" >nul 2>nul
    copy /Y "%PG_PATH%\bin\libcrypto.dll" "dist\" >nul 2>nul
    copy /Y "%PG_PATH%\bin\zlib1.dll" "dist\" >nul 2>nul
)

REM Create run script
echo @echo off > "dist\run_monitor.bat"
echo cd /d "%%~dp0" >> "dist\run_monitor.bat"
echo start "" "Ban_Delta_Breach_Notifier.exe" >> "dist\run_monitor.bat"

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Distribution created in: build\dist\
echo Executable: build\dist\Ban_Delta_Breach_Notifier.exe
echo.
echo To run the application:
echo   1. cd build\dist
echo   2. run run_monitor.bat
echo   3. Or double-click Ban_Delta_Breach_Notifier.exe
echo.

REM Run the application if requested
if "%RUN_AFTER_BUILD%"=="true" (
    echo Starting PostgreSQL Monitor...
    cd dist
    start "" "Ban_Delta_Breach_Notifier.exe"
)

echo Build completed successfully!
pause
exit /b 0