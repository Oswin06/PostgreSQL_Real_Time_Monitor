#!/bin/bash

# PostgreSQL Monitor - Linux/macOS Build Script
# This script compiles the PostgreSQL Real-Time Monitor for Unix-like systems

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=false
RUN_AFTER_BUILD=false
VERBOSE=false
INSTALL_PREFIX="$HOME/PostgreSQLMonitor"
ENABLE_TESTS=false
ENABLE_DOCS=false
BUILD_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")

# Function to show usage
show_usage() {
    cat << EOF
PostgreSQL Monitor Build Script

Usage: $0 [OPTIONS]

OPTIONS:
    -h, --help              Show this help message
    -t, --type TYPE         Build type: Debug or Release (default: Release)
    -c, --clean             Clean previous build before building
    -r, --run               Run the application after successful build
    -v, --verbose           Enable verbose build output
    -j, --jobs N            Number of parallel build jobs (default: $BUILD_JOBS)
    -p, --prefix PATH       Installation prefix (default: $INSTALL_PREFIX)
    --tests                Enable building tests
    --docs                 Enable building documentation

Examples:
    $0                      # Basic release build
    $0 -t Debug -c -v       # Debug build with clean and verbose output
    $0 -r                   # Build and run immediately
    $0 -j 8                 # Build with 8 parallel jobs

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -r|--run)
            RUN_AFTER_BUILD=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            BUILD_JOBS="$2"
            shift 2
            ;;
        -p|--prefix)
            INSTALL_PREFIX="$2"
            shift 2
            ;;
        --tests)
            ENABLE_TESTS=true
            shift
            ;;
        --docs)
            ENABLE_DOCS=true
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Print build configuration
print_info "PostgreSQL Monitor Build Configuration"
echo "========================================="
echo "Build Type: $BUILD_TYPE"
echo "Clean Build: $CLEAN_BUILD"
echo "Run After Build: $RUN_AFTER_BUILD"
echo "Verbose Output: $VERBOSE"
echo "Build Jobs: $BUILD_JOBS"
echo "Install Prefix: $INSTALL_PREFIX"
echo "Enable Tests: $ENABLE_TESTS"
echo "Enable Documentation: $ENABLE_DOCS"
echo ""

# Check if we're in the right directory
if [[ ! -f "CMakeLists.txt" ]]; then
    print_error "CMakeLists.txt not found. Please run this script from the project root."
    print_error "Current directory: $(pwd)"
    exit 1
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required tools
print_info "Checking for required tools..."

if ! command_exists cmake; then
    print_error "CMake is not installed or not in PATH."
    print_info "Please install CMake:"
    print_info "  Ubuntu/Debian: sudo apt install cmake"
    print_info "  CentOS/RHEL:   sudo yum install cmake"
    print_info "  macOS:         brew install cmake"
    exit 1
fi
print_success "CMake found: $(cmake --version | head -n1)"

# Check for C++ compiler
if command_exists g++; then
    print_success "G++ found: $(g++ --version | head -n1)"
elif command_exists clang++; then
    print_success "Clang++ found: $(clang++ --version | head -n1)"
else
    print_error "No C++ compiler found (g++ or clang++)"
    print_info "Please install a C++ compiler:"
    print_info "  Ubuntu/Debian: sudo apt install build-essential"
    print_info "  CentOS/RHEL:   sudo yum groupinstall 'Development Tools'"
    print_info "  macOS:         xcode-select --install"
    exit 1
fi

# Check for Qt6
print_info "Checking for Qt6..."

QT6_FOUND=false
QT_PATHS=(
    "/usr/lib/qt6"
    "/usr/local/Qt-6.5.0"
    "/usr/local/Qt-6.4.0"
    "/opt/homebrew/opt/qt"
    "/usr/local/opt/qt"
    "$HOME/Qt/6.5.0/gcc_64"
    "$HOME/Qt/6.4.0/gcc_64"
)

if command_exists qmake6; then
    QT_VERSION=$(qmake6 -version 2>/dev/null | grep -o 'Qt version [0-9]\+' | head -n1)
    if [[ $QT_VERSION == *"Qt version 6"* ]]; then
        QT6_FOUND=true
        print_success "Qt6 found via qmake6"
    fi
fi

if [[ "$QT6_FOUND" == false ]]; then
    for qt_path in "${QT_PATHS[@]}"; do
        if [[ -f "$qt_path/bin/qmake" ]]; then
            QT_VERSION=$("$qt_path/bin/qmake" -version 2>/dev/null | grep -o 'Qt version [0-9]\+' | head -n1)
            if [[ $QT_VERSION == *"Qt version 6"* ]]; then
                QT6_FOUND=true
                QT_PATH="$qt_path"
                print_success "Qt6 found at $qt_path"
                export CMAKE_PREFIX_PATH="$qt_path"
                break
            fi
        fi
    done
fi

if [[ "$QT6_FOUND" == false ]]; then
    print_warning "Qt6 not found in common locations."
    print_info "Qt6 installation options:"
    print_info "  Ubuntu/Debian: sudo apt install qt6-base-dev qt6-tools-dev"
    print_info "  CentOS/RHEL:   sudo dnf install qt6-qtbase-devel qt6-qttools-devel"
    print_info "  macOS:         brew install qt6"
    print_info "  Download:      https://www.qt.io/download"
fi

# Check for PostgreSQL and libpqxx
print_info "Checking for PostgreSQL..."

PG_FOUND=false
PG_PATHS=(
    "/usr/lib/postgresql"
    "/usr/local/pgsql"
    "/opt/homebrew/opt/postgresql"
    "/usr/local/opt/postgresql"
)

if pkg-config --exists libpq; then
    PG_FOUND=true
    PG_VERSION=$(pkg-config --modversion libpq)
    print_success "PostgreSQL found: $PG_VERSION"
fi

if pkg-config --exists libpqxx; then
    PQXX_VERSION=$(pkg-config --modversion libpqxx)
    print_success "libpqxx found: $PQXX_VERSION"
else
    print_warning "libpqxx not found via pkg-config"
fi

if [[ "$PG_FOUND" == false ]]; then
    for pg_path in "${PG_PATHS[@]}"; do
        if [[ -f "$pg_path/lib/libpq.so" ]] || [[ -f "$pg_path/lib/libpq.dylib" ]]; then
            PG_FOUND=true
            PG_PATH="$pg_path"
            print_success "PostgreSQL found at $pg_path"
            export CMAKE_LIBRARY_PATH="$pg_path/lib:$CMAKE_LIBRARY_PATH"
            export CMAKE_INCLUDE_PATH="$pg_path/include:$CMAKE_INCLUDE_PATH"
            break
        fi
    done
fi

if [[ "$PG_FOUND" == false ]]; then
    print_warning "PostgreSQL not found in common locations."
    print_info "PostgreSQL installation options:"
    print_info "  Ubuntu/Debian: sudo apt install libpq-dev libpqxx-dev"
    print_info "  CentOS/RHEL:   sudo yum install postgresql-devel postgresql-libs"
    print_info "  macOS:         brew install postgresql libpqxx"
fi

# Clean previous build if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    print_info "Cleaning previous build..."
    if [[ -d "build" ]]; then
        rm -rf build
        if [[ -d "build" ]]; then
            print_warning "Could not remove build directory completely"
        else
            print_success "Build directory cleaned"
        fi
    fi
fi

# Create build directory
print_info "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
print_info "Configuring project with CMake..."

CMAKE_CMD=(
    cmake
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
)

if [[ "$ENABLE_TESTS" == true ]]; then
    CMAKE_CMD+=(-DBUILD_TESTS=ON)
fi

if [[ "$ENABLE_DOCS" == true ]]; then
    CMAKE_CMD+=(-DBUILD_DOCS=ON)
fi

if [[ "$VERBOSE" == true ]]; then
    CMAKE_CMD+=(-DCMAKE_VERBOSE_MAKEFILE=ON)
fi

if [[ -n "$QT_PATH" ]]; then
    CMAKE_CMD+=(-DCMAKE_PREFIX_PATH="$QT_PATH")
fi

# Add platform-specific optimizations
case "$(uname -s)" in
    Linux*)
        CMAKE_CMD+=(-DCMAKE_CXX_FLAGS="-O3 -march=native")
        ;;
    Darwin*)
        CMAKE_CMD+=(-DCMAKE_CXX_FLAGS="-O3 -march=native")
        ;;
esac

echo "Running: ${CMAKE_CMD[*]} ${CMAKE_CMD[@]: -1}"
"${CMAKE_CMD[@]}" .. || {
    print_error "CMake configuration failed."
    print_info "Please check that all dependencies are installed:"
    print_info "  - C++17 compatible compiler"
    print_info "  - Qt6 development libraries (Qt6Core, Qt6Widgets)"
    print_info "  - PostgreSQL development libraries (libpq, libpqxx)"
    echo ""
    print_info "Try installing dependencies:"
    case "$(uname -s)" in
        Linux*)
            if command_exists apt; then
                print_info "  sudo apt install qt6-base-dev qt6-tools-dev libpq-dev libpqxx-dev"
            elif command_exists yum; then
                print_info "  sudo dnf install qt6-qtbase-devel qt6-qttools-devel postgresql-devel libpqxx-devel"
            fi
            ;;
        Darwin*)
            print_info "  brew install qt6 postgresql libpqxx"
            ;;
    esac
    exit 1
}

print_success "CMake configuration successful"

# Build the project
print_info "Building project with $BUILD_JOBS parallel jobs..."

BUILD_CMD=(
    cmake
    --build .
    --config "$BUILD_TYPE"
    --parallel "$BUILD_JOBS"
)

if [[ "$VERBOSE" == true ]]; then
    BUILD_CMD+=(--verbose)
fi

echo "Running: ${BUILD_CMD[*]}"
"${BUILD_CMD[@]}" || {
    print_error "Build failed."
    exit 1
}

print_success "Build completed successfully"

# Create distribution directory
print_info "Creating distribution..."
mkdir -p dist

# Copy executable and required files
cp "Ban_Delta_Breach_Notifier" dist/ 2>/dev/null || cp "src/Ban_Delta_Breach_Notifier" dist/ 2>/dev/null || {
    print_error "Could not find the executable"
    exit 1
}

cp -r ../config dist/ 2>/dev/null || {
    print_warning "Could not copy config directory"
}

cp ../README.md dist/ 2>/dev/null || true

# Create run script
cat > dist/run_monitor.sh << 'EOF'
#!/bin/bash

# PostgreSQL Monitor Run Script

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check if executable exists
if [[ ! -f "Ban_Delta_Breach_Notifier" ]]; then
    echo "Error: Ban_Delta_Breach_Notifier executable not found in $SCRIPT_DIR"
    exit 1
fi

# Set library path if needed
export LD_LIBRARY_PATH="$SCRIPT_DIR:$LD_LIBRARY_PATH"

# Run the application
echo "Starting PostgreSQL Monitor..."
./Ban_Delta_Breach_Notifier "$@"
EOF

chmod +x dist/run_monitor.sh

# Create desktop entry for Linux
if [[ "$(uname -s)" == "Linux"* ]]; then
    print_info "Creating desktop entry..."
    cat > dist/postgresql-monitor.desktop << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=PostgreSQL Monitor
Comment=Real-time PostgreSQL database monitoring system
Exec=$PWD/dist/run_monitor.sh
Icon=postgresql
Terminal=false
Categories=Development;Database;
Keywords=database;monitor;postgresql;
EOF
fi

print_success "Distribution created in build/dist/"

# Install if prefix is not default location
if [[ "$INSTALL_PREFIX" != "$HOME/PostgreSQLMonitor" ]]; then
    print_info "Installing to $INSTALL_PREFIX..."
    cmake --install . --config "$BUILD_TYPE" || {
        print_warning "Installation failed, but build was successful"
    }
fi

echo ""
echo "========================================"
print_success "BUILD SUCCESSFUL!"
echo "========================================"
echo ""
echo "Distribution created in: build/dist/"
echo "Executable: build/dist/Ban_Delta_Breach_Notifier"
echo ""
echo "To run the application:"
echo "  cd build/dist"
echo "  ./run_monitor.sh"
echo ""
echo "Or install system-wide:"
echo "  sudo make install  (if CMAKE_INSTALL_PREFIX is system directory)"
echo ""

# Show final summary
if [[ "$RUN_AFTER_BUILD" == true ]]; then
    print_info "Starting PostgreSQL Monitor..."
    cd dist
    ./run_monitor.sh
else
    print_info "Build completed! Use ./build/dist/run_monitor.sh to start the application."
fi

echo "Thank you for using PostgreSQL Monitor!"