# Build Instructions

This document provides comprehensive instructions for building the PostgreSQL Real-Time Monitor on different platforms.

## Quick Start

### Prerequisites

- **C++17 compatible compiler** (GCC 9+, Clang 10+, or MSVC 2019+)
- **CMake 3.16+**
- **Qt6** (Core, Widgets, Tools modules)
- **PostgreSQL client libraries** (libpq, libpqxx)
- **Git** (optional, for version control)

### Platform-Specific Instructions

#### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake
sudo apt install qt6-base-dev qt6-tools-dev
sudo apt install libpq-dev libpqxx-dev

# Check dependencies
./check_build_deps.sh

# Build the project
./build.sh

# Or with options
./build.sh -t Debug -c -v    # Debug build, clean, verbose
./build.sh -r               # Build and run
```

#### Linux (CentOS/RHEL/Fedora)

```bash
# Fedora
sudo dnf install cmake gcc-c++ qt6-qtbase-devel qt6-qttools-devel
sudo dnf install postgresql-devel libpqxx-devel

# CentOS/RHEL (with EPEL)
sudo yum install epel-release
sudo yum install cmake gcc-c++ qt6-qtbase-devel qt6-qttools-devel
sudo yum install postgresql-devel libpqxx-devel

# Build
./build.sh
```

#### macOS

```bash
# Install dependencies with Homebrew
brew install cmake qt6 postgresql libpqxx

# Set Qt6 environment variable (if needed)
export CMAKE_PREFIX_PATH=$(brew --prefix qt6)

# Build
./build.sh
```

#### Windows

```cmd
# Option 1: Using Visual Studio Installer
# - Install Visual Studio 2019/2022 with C++ tools
# - Install Qt6 from qt.io
# - Install PostgreSQL from postgresql.org

# Option 2: Using vcpkg (recommended)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
.\vcpkg install qt6 postgresql libpqxx

# Check dependencies
check_build_deps.bat

# Build (run from Visual Studio Developer Command Prompt)
build.bat

# Or with options
build.bat debug            # Debug build
build.bat clean run        # Clean build and run
build.bat verbose          # Verbose output
```

## Build Scripts

### Dependency Checkers

- **Linux/macOS**: `./check_build_deps.sh`
- **Windows**: `check_build_deps.bat`

These scripts verify all required dependencies are available before building.

### Main Build Scripts

#### Linux/macOS: `build.sh`

```bash
# Usage
./build.sh [OPTIONS]

# Options:
  -h, --help              Show help message
  -t, --type TYPE         Build type: Debug or Release (default: Release)
  -c, --clean             Clean previous build before building
  -r, --run               Run the application after successful build
  -v, --verbose           Enable verbose build output
  -j, --jobs N            Number of parallel build jobs (default: auto-detect)
  -p, --prefix PATH       Installation prefix
  --tests                Enable building tests
  --docs                 Enable building documentation

# Examples:
./build.sh                         # Release build
./build.sh -t Debug -c -v          # Debug build, clean, verbose
./build.sh -r                      # Build and run
./build.sh -j 8                    # Build with 8 parallel jobs
```

#### Windows: `build.bat`

```cmd
# Usage
build.bat [OPTIONS]

# Options:
  debug                  Debug build (default: Release)
  release                Release build
  clean                  Clean previous build before building
  run                    Run the application after successful build
  verbose                Enable verbose build output
  --prefix PATH          Installation prefix

# Examples:
build.bat                         # Release build
build.bat debug clean verbose     # Debug build, clean, verbose
build.bat run                     # Build and run
```

## Manual Build Instructions

If you prefer to build manually without the scripts:

### 1. Create Build Directory

```bash
mkdir build
cd build
```

### 2. Configure with CMake

```bash
# Linux/macOS
cmake -DCMAKE_BUILD_TYPE=Release ..

# Windows (Visual Studio)
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..

# Set Qt6 path if needed
cmake -DCMAKE_PREFIX_PATH=/path/to/qt6 ..
```

### 3. Build

```bash
# Linux/macOS
cmake --build . --config Release --parallel

# Windows
cmake --build . --config Release --config Release --parallel
```

### 4. Install (Optional)

```bash
cmake --install . --config Release
```

## Troubleshooting

### Common Issues

#### CMake cannot find Qt6

**Linux/macOS:**
```bash
export CMAKE_PREFIX_PATH=/path/to/qt6
cmake -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH ..
```

**Windows:**
```cmd
cmake -DCMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2019_64 ..
```

#### PostgreSQL libraries not found

**Linux:**
```bash
# Install development packages
sudo apt install libpq-dev libpqxx-dev

# Or set paths manually
export CMAKE_LIBRARY_PATH=/usr/lib/postgresql
export CMAKE_INCLUDE_PATH=/usr/include/postgresql
```

**Windows:**
```cmd
cmake -DPostgreSQL_ROOT=C:\Program Files\PostgreSQL\15 ..
```

#### Build fails with compiler errors

1. **Check compiler version:**
   - GCC: `g++ --version` (needs 9.0+)
   - Clang: `clang++ --version` (needs 10.0+)
   - MSVC: Included with Visual Studio 2019+

2. **Ensure C++17 support:**
   ```bash
   cmake -DCMAKE_CXX_STANDARD=17 ..
   ```

#### Qt6 issues on Windows

1. **Use vcpkg** (recommended):
   ```cmd
   vcpkg install qt6-base qt6-tools
   cmake -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake ..
   ```

2. **Set environment variables:**
   ```cmd
   set CMAKE_PREFIX_PATH=C:\Qt\6.5.0\msvc2019_64
   set PATH=%CMAKE_PREFIX_PATH%\bin;%PATH%
   ```

### Getting Help

1. **Run dependency checker first:**
   - Linux/macOS: `./check_build_deps.sh`
   - Windows: `check_build_deps.bat`

2. **Check the main README.md** for detailed setup instructions

3. **Verify project files are complete:**
   ```bash
   # Should show 14 files
   ls -1 CMakeLists.txt main.cpp include/*.h src/*.cpp config/*.conf
   ```

4. **Clean build if making changes:**
   ```bash
   rm -rf build
   ./build.sh -c
   ```

## Advanced Configuration

### Using vcpkg for Dependencies

```bash
# Clone and bootstrap vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# or
./bootstrap-vcpkg.bat  # Windows

# Install dependencies
./vcpkg install qt6 postgresql libpqxx

# Integrate with CMake
./vcpkg integrate install

# Build with vcpkg toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake ..
```

### Cross-Platform Development

For development across multiple platforms:

1. **Use vcpkg** for consistent dependency management
2. **Set environment variables** for Qt6 and PostgreSQL paths
3. **Use the build scripts** as they handle platform differences
4. **Test on all target platforms** before deployment

### Performance Optimizations

For release builds with optimizations:

```bash
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS="-O3 -march=native" \
      ..
```

This enables maximum performance optimizations for your specific CPU architecture.