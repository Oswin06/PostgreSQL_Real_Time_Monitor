#!/bin/bash

# PostgreSQL Monitor - Build Dependencies Checker
# This script verifies all required dependencies are available before building

set -e

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
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_header() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if package exists via pkg-config
pkg_config_exists() {
    pkg-config --exists "$1" 2>/dev/null
}

# Function to get version via pkg-config
pkg_config_version() {
    pkg-config --modversion "$1" 2>/dev/null || echo "unknown"
}

# Function to check file/directory exists
path_exists() {
    [[ -e "$1" ]]
}

# Function to check minimum version
check_version() {
    local program="$1"
    local version="$2"
    local min_version="$3"

    if command -v python3 >/dev/null 2>&1; then
        python3 -c "
import sys
from packaging import version
if version.parse('$version') < version.parse('$min_version'):
    sys.exit(1)
" 2>/dev/null && return 0
    fi

    # Fallback to string comparison (not ideal but works for simple cases)
    if [[ "$version" == "$min_version" ]] || [[ "$version" > "$min_version" ]]; then
        return 0
    fi
    return 1
}

print_header "PostgreSQL Monitor - Build Dependencies Check"
echo ""

# Detect OS
OS="Unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
    OS="Windows"
fi

print_info "Detected OS: $OS"
echo ""

# Check basic tools
print_header "Basic Development Tools"

TOOLS=(
    "cmake:CMake:3.16"
    "make:GNU Make:3.0"
    "g++:G++ Compiler:9.0"
    "clang++:Clang++ Compiler:10.0"
)

for tool_info in "${TOOLS[@]}"; do
    IFS=':' read -r tool name min_version <<< "$tool_info"

    if command_exists "$tool"; then
        version_output=$("$tool" --version 2>/dev/null | head -n1 || echo "version unknown")
        version=$(echo "$version_output" | grep -o '[0-9]\+\.[0-9]\+\.[0-9]\+' | head -n1 || echo "unknown")

        if check_version "$tool" "$version" "$min_version" 2>/dev/null; then
            print_success "$name $version"
        else
            print_warning "$name $version (recommended: >= $min_version)"
        fi
    else
        if [[ "$tool" == "g++" ]] && command_exists clang++; then
            print_info "$name not found, but Clang++ is available"
        elif [[ "$tool" == "clang++" ]] && command_exists g++; then
            print_info "$name not found, but G++ is available"
        else
            print_error "$name not found"
        fi
    fi
done

# Check Qt6
echo ""
print_header "Qt6 Framework"

QT6_FOUND=false

# Check for qmake6 first
if command_exists qmake6; then
    QT_VERSION=$(qmake6 -version 2>/dev/null | grep -o 'Qt version [0-9]\+\.[0-9]\+\.[0-9]\+' | head -n1 || echo "Qt version unknown")
    if [[ $QT_VERSION == *"Qt version 6"* ]]; then
        QT6_FOUND=true
        print_success "Qt6 found ($QT_VERSION)"
    else
        print_warning "qmake6 found but not Qt6 ($QT_VERSION)"
    fi
else
    print_warning "qmake6 not found"
fi

# Check Qt6 via pkg-config
if pkg_config_exists Qt6Core; then
    QT_VERSION=$(pkg_config_version Qt6Core)
    print_success "Qt6 found via pkg-config ($QT_VERSION)"
    QT6_FOUND=true
else
    print_warning "Qt6 not found via pkg-config"
fi

# Check common Qt6 paths
if [[ "$QT6_FOUND" == false ]]; then
    QT_PATHS=(
        "/usr/lib/qt6"
        "/usr/local/Qt-6.5.0"
        "/usr/local/Qt-6.4.0"
        "/opt/homebrew/opt/qt"
        "/usr/local/opt/qt"
        "$HOME/Qt/6.5.0/gcc_64"
        "$HOME/Qt/6.4.0/gcc_64"
    )

    for qt_path in "${QT_PATHS[@]}"; do
        if path_exists "$qt_path/bin/qmake"; then
            QT_VERSION=$("$qt_path/bin/qmake" -version 2>/dev/null | grep -o 'Qt version [0-9]\+\.[0-9]\+\.[0-9]\+' | head -n1 || echo "Qt version unknown")
            if [[ $QT_VERSION == *"Qt version 6"* ]]; then
                print_success "Qt6 found at $qt_path ($QT_VERSION)"
                QT6_FOUND=true
                break
            fi
        fi
    done
fi

if [[ "$QT6_FOUND" == false ]]; then
    print_error "Qt6 not found"
    print_info "Install Qt6:"
    case "$OS" in
        "Linux")
            print_info "  Ubuntu/Debian: sudo apt install qt6-base-dev qt6-tools-dev"
            print_info "  CentOS/RHEL:   sudo dnf install qt6-qtbase-devel qt6-qttools-devel"
            ;;
        "macOS")
            print_info "  brew install qt6"
            ;;
        "Windows")
            print_info "  Download from: https://www.qt.io/download"
            ;;
    esac
fi

# Check PostgreSQL and libpqxx
echo ""
print_header "PostgreSQL Libraries"

POSTGRESQL_FOUND=false
LIBPQXX_FOUND=false

# Check libpq
if pkg_config_exists libpq; then
    PQ_VERSION=$(pkg_config_version libpq)
    print_success "libpq found ($PQ_VERSION)"
    POSTGRESQL_FOUND=true
else
    print_warning "libpq not found via pkg-config"
fi

# Check libpqxx
if pkg_config_exists libpqxx; then
    PQXX_VERSION=$(pkg_config_version libpqxx)
    print_success "libpqxx found ($PQXX_VERSION)"
    LIBPQXX_FOUND=true
else
    print_warning "libpqxx not found via pkg-config"
fi

# Check PostgreSQL in common locations
if [[ "$POSTGRESQL_FOUND" == false ]] || [[ "$LIBPQXX_FOUND" == false ]]; then
    PG_PATHS=(
        "/usr/lib/postgresql"
        "/usr/local/pgsql"
        "/opt/homebrew/opt/postgresql"
        "/usr/local/opt/postgresql"
        "/Applications/Postgres.app/Contents/Versions/latest"
    )

    for pg_path in "${PG_PATHS[@]}"; do
        if path_exists "$pg_path/lib/libpq.so" || path_exists "$pg_path/lib/libpq.dylib"; then
            if [[ "$POSTGRESQL_FOUND" == false ]]; then
                print_success "libpq found at $pg_path"
                POSTGRESQL_FOUND=true
            fi
        fi

        if path_exists "$pg_path/lib/libpqxx.so" || path_exists "$pg_path/lib/libpqxx.dylib"; then
            if [[ "$LIBPQXX_FOUND" == false ]]; then
                print_success "libpqxx found at $pg_path"
                LIBPQXX_FOUND=true
            fi
        fi
    done
fi

if [[ "$POSTGRESQL_FOUND" == false ]]; then
    print_error "libpq not found"
    print_info "Install PostgreSQL client libraries:"
    case "$OS" in
        "Linux")
            print_info "  Ubuntu/Debian: sudo apt install libpq-dev"
            print_info "  CentOS/RHEL:   sudo yum install postgresql-devel"
            ;;
        "macOS")
            print_info "  brew install postgresql"
            ;;
        "Windows")
            print_info "  Download from: https://www.postgresql.org/download/windows/"
            ;;
    esac
fi

if [[ "$LIBPQXX_FOUND" == false ]]; then
    print_error "libpqxx not found"
    print_info "Install libpqxx:"
    case "$OS" in
        "Linux")
            print_info "  Ubuntu/Debian: sudo apt install libpqxx-dev"
            print_info "  CentOS/RHEL:   sudo yum install libpqxx-devel"
            ;;
        "macOS")
            print_info "  brew install libpqxx"
            ;;
        "Windows")
            print_info "  Use vcpkg: vcpkg install libpqxx"
            ;;
    esac
fi

# Check additional development tools
echo ""
print_header "Additional Development Tools"

if command_exists git; then
    print_success "Git ($(git --version | cut -d' ' -f3))"
else
    print_warning "Git not found (optional for version control)"
fi

if command_exists python3; then
    PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
    print_success "Python3 $PYTHON_VERSION"

    # Check for packaging module (used for version checking)
    if python3 -c "import packaging" 2>/dev/null; then
        print_success "Python packaging module available"
    else
        print_warning "Python packaging module not available (pip install packaging)"
    fi
else
    print_warning "Python3 not found (optional for some build tools)"
fi

# Check for threading library
echo ""
print_header "System Libraries"

if [[ "$OS" == "Linux" ]]; then
    if path_exists "/usr/include/pthread.h" || path_exists "/usr/include/x86_64-linux-gnu/pthread.h"; then
        print_success "POSIX Threads library found"
    else
        print_warning "POSIX Threads library not found"
    fi

    if path_exists "/usr/include/openssl/ssl.h" || pkg_config_exists openssl; then
        print_success "OpenSSL found"
    else
        print_warning "OpenSSL not found (may be required for PostgreSQL)"
    fi
elif [[ "$OS" == "macOS" ]]; then
    print_success "macOS system libraries available"
fi

# Check project structure
echo ""
print_header "Project Structure"

PROJECT_FILES=(
    "CMakeLists.txt"
    "main.cpp"
    "include/DatabaseManager.h"
    "include/AlertSystem.h"
    "include/AlertWindow.h"
    "include/QueryEngine.h"
    "src/DatabaseManager.cpp"
    "src/AlertSystem.cpp"
    "src/AlertWindow.cpp"
    "src/QueryEngine.cpp"
    "config/database.conf"
    "config/queries.conf"
)

ALL_FILES_FOUND=true
for file in "${PROJECT_FILES[@]}"; do
    if path_exists "$file"; then
        print_success "$file"
    else
        print_error "$file not found"
        ALL_FILES_FOUND=false
    fi
done

if [[ "$ALL_FILES_FOUND" == false ]]; then
    print_error "Some project files are missing"
    print_info "Make sure you're in the project root directory"
fi

# Summary
echo ""
print_header "Summary"

ISSUES_FOUND=0

if [[ "$QT6_FOUND" == false ]]; then
    ((ISSUES_FOUND++))
fi

if [[ "$POSTGRESQL_FOUND" == false ]]; then
    ((ISSUES_FOUND++))
fi

if [[ "$LIBPQXX_FOUND" == false ]]; then
    ((ISSUES_FOUND++))
fi

if [[ "$ALL_FILES_FOUND" == false ]]; then
    ((ISSUES_FOUND++))
fi

if [[ $ISSUES_FOUND -eq 0 ]]; then
    print_success "All dependencies found! You should be able to build the project."
    echo ""
    print_info "To build the project:"
    print_info "  Linux/macOS: ./build.sh"
    print_info "  Windows:      build.bat"
else
    print_error "$ISSUES_FOUND critical issue(s) found"
    echo ""
    print_info "Please install the missing dependencies before attempting to build."
    echo ""
    print_info "For detailed installation instructions, see:"
    print_info "  - README.md in the project root"
    print_info "  - build.sh script contains dependency commands for your OS"
fi

# Exit with appropriate code
exit $ISSUES_FOUND