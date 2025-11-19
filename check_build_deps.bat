@echo off
setlocal enabledelayedexpansion

REM PostgreSQL Monitor - Windows Build Dependencies Checker
REM This script verifies all required dependencies are available before building

echo ========================================
echo PostgreSQL Monitor - Build Dependencies Check
echo ========================================
echo.

REM Check if we're in the right directory
if not exist "CMakeLists.txt" (
    echo ERROR: CMakeLists.txt not found. Please run this script from the project root.
    echo Current directory: %CD%
    pause
    exit /b 1
)

REM Initialize counters
set ISSUES_FOUND=0
set QT6_FOUND=false
set POSTGRESQL_FOUND=false
set LIBPQXX_FOUND=false

echo [1/6] Checking Basic Development Tools
echo ----------------------------------------

REM Check for CMake
where cmake >nul 2>nul
if errorlevel 1 (
    echo [X] CMake not found
    echo     Download from: https://cmake.org/download/
    set /a ISSUES_FOUND+=1
) else (
    for /f "tokens=3" %%i in ('cmake --version ^| findstr /r "cmake version [0-9][0-9]*\.[0-9][0-9]*"') do (
        echo [✓] CMake %%i
    )
)

REM Check for Visual Studio C++ compiler
where cl >nul 2>nul
if errorlevel 1 (
    echo [!] Visual Studio C++ compiler not found in PATH
    echo     Run from Visual Studio Developer Command Prompt
    echo     Or install Visual Studio 2019/2022 with C++ tools
) else (
    for /f "tokens=3" %%i in ('cl ^| findstr /r "Microsoft.*Version [0-9][0-9]*\.[0-9][0-9]*"') do (
        echo [✓] Visual Studio %%i
    )
)

echo.
echo [2/6] Checking Qt6 Framework
echo ----------------------------------------

REM Check for Qt6 in common locations
set QT6_FOUND=false
set QT_PATHS[0]=C:\Qt\6.5.0\msvc2019_64
set QT_PATHS[1]=C:\Qt\6.4.0\msvc2019_64
set QT_PATHS[2]=C:\Qt\6.3.0\msvc2019_64
set QT_PATHS[3]=C:\Qt\6.5.0\msvc2022_64
set QT_PATHS[4]=C:\Qt\6.4.0\msvc2022_64
set QT_PATHS[5]=C:\Qt\6.5.0\mingw_64
set QT_PATHS[6]=C:\Qt\6.4.0\mingw_64

for /L %%i in (0,1,6) do (
    if exist "!QT_PATHS[%%i]!\bin\qmake.exe" (
        for /f "tokens=3" %%j in ('"!QT_PATHS[%%i]!\bin\qmake.exe" -version ^| findstr /r "Qt version [0-9][0-9]*\.[0-9][0-9]*"') do (
            echo [✓] Qt6 found at !QT_PATHS[%%i]! (%%j)
        )
        set QT6_FOUND=true
        goto qt_found
    )
)

:qt_found
if "%QT6_FOUND%"=="false" (
    echo [X] Qt6 not found
    echo     Common Qt6 locations:
    echo       C:\Qt\6.5.0\msvc2019_64
    echo       C:\Qt\6.4.0\msvc2019_64
    echo     Download from: https://www.qt.io/download
    echo     Or install via vcpkg: vcpkg install qt6-base qt6-tools
    set /a ISSUES_FOUND+=1
)

echo.
echo [3/6] Checking PostgreSQL Libraries
echo ----------------------------------------

REM Check for PostgreSQL in common locations
set PG_FOUND=false
set PG_PATHS[0]=C:\Program Files\PostgreSQL\15
set PG_PATHS[1]=C:\Program Files\PostgreSQL\14
set PG_PATHS[2]=C:\Program Files\PostgreSQL\13
set PG_PATHS[3]=C:\PostgreSQL\15
set PG_PATHS[4]=C:\PostgreSQL\14

for /L %%i in (0,1,4) do (
    if exist "!PG_PATHS[%%i]!\bin\libpq.dll" (
        echo [✓] PostgreSQL found at !PG_PATHS[%%i]!
        set POSTGRESQL_FOUND=true
        set PG_PATH=!PG_PATHS[%%i]!
    )

    if exist "!PG_PATHS[%%i]!\bin\libpqxx.dll" (
        echo [✓] libpqxx found at !PG_PATHS[%%i]!
        set LIBPQXX_FOUND=true
        set PQXX_PATH=!PG_PATHS[%%i]!
    )
)

if "%POSTGRESQL_FOUND%"=="false" (
    echo [X] PostgreSQL client library (libpq.dll) not found
    set /a ISSUES_FOUND+=1
)

if "%LIBPQXX_FOUND%"=="false" (
    echo [X] libpqxx library not found
    echo     Install options:
    echo       - Download from: https://www.postgresql.org/download/windows/
    echo       - Use vcpkg: vcpkg install libpqxx
    echo       - Build from source: https://pqxx.org/download/
    set /a ISSUES_FOUND+=1
)

echo.
echo [4/6] Checking Additional Development Tools
echo ----------------------------------------

REM Check for Git
where git >nul 2>nul
if errorlevel 1 (
    echo [!] Git not found (optional for version control)
) else (
    for /f "tokens=3" %%i in ('git --version') do (
        echo [✓] Git %%i
    )
)

REM Check for vcpkg
where vcpkg >nul 2>nul
if errorlevel 1 (
    echo [!] vcpkg not found (optional dependency manager)
    echo     Install from: https://github.com/Microsoft/vcpkg
) else (
    echo [✓] vcpkg found
)

echo.
echo [5/6] Checking Project Structure
echo ----------------------------------------

set PROJECT_FILES=CMakeLists.txt main.cpp include\DatabaseManager.h include\AlertSystem.h include\AlertWindow.h include\QueryEngine.h src\DatabaseManager.cpp src\AlertSystem.cpp src\AlertWindow.cpp src\QueryEngine.cpp config\database.conf config\queries.conf

set ALL_FILES_FOUND=true
for %%f in (%PROJECT_FILES%) do (
    if exist "%%f" (
        echo [✓] %%f
    ) else (
        echo [X] %%f not found
        set ALL_FILES_FOUND=false
        set /a ISSUES_FOUND+=1
    )
)

echo.
echo [6/6] System Information
echo ----------------------------------------

echo Operating System: Windows
echo Processor: %PROCESSOR_IDENTIFIER%
echo Architecture: %PROCESSOR_ARCHITECTURE%

REM Check Visual Studio versions
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019" (
    echo [✓] Visual Studio 2019 found
)
if exist "C:\Program Files\Microsoft Visual Studio\2022" (
    echo [✓] Visual Studio 2022 found
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017" (
    echo [✓] Visual Studio 2017 found
)

echo.
echo ========================================
echo SUMMARY
echo ========================================

if %ISSUES_FOUND% equ 0 (
    echo [✓] All critical dependencies found!
    echo     You should be able to build the project.
    echo.
    echo To build the project:
    echo   build.bat                    - Basic build
    echo   build.bat debug              - Debug build
    echo   build.bat clean              - Clean and build
    echo   build.bat run                - Build and run
    echo.
) else (
    echo [X] %ISSUES_FOUND% critical issue(s) found
    echo.
    echo Please resolve these issues before attempting to build.
    echo.
    echo Common solutions:
    echo   1. Run from Visual Studio Developer Command Prompt
    echo   2. Install Qt6: https://www.qt.io/download
    echo   3. Install PostgreSQL: https://www.postgresql.org/download/windows/
    echo   4. Use vcpkg for dependencies: vcpkg install qt6 libpqxx
    echo.
)

REM If issues found, wait for user input
if %ISSUES_FOUND% gtr 0 (
    echo.
    echo Press any key to continue...
    pause >nul
)

exit /b %ISSUES_FOUND%