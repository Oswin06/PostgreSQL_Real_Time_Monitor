cmake_minimum_required(VERSION 3.16)

project(Ban_Delta_Breach_Notifier VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable Qt MOC, UIC, and RCC processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find required Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Find PostgreSQL and libpqxx
# Try different approaches for finding PostgreSQL
find_package(PostgreSQL REQUIRED)
if(PostgreSQL_FOUND)
    message(STATUS "Found PostgreSQL: ${PostgreSQL_VERSION}")
    message(STATUS "PostgreSQL include dir: ${PostgreSQL_INCLUDE_DIRS}")
    message(STATUS "PostgreSQL library: ${PostgreSQL_LIBRARIES}")
else()
    message(FATAL_ERROR "PostgreSQL not found. Please install PostgreSQL development libraries.")
endif()

# Try to find libpqxx
find_path(PQXX_INCLUDE_DIR pqxx/pqxx
    PATHS
    /usr/include
    /usr/local/include
    /opt/homebrew/include
    ${PostgreSQL_INCLUDE_DIRS}
)

find_library(PQXX_LIBRARY
    NAMES pqxx
    PATHS
    /usr/lib
    /usr/local/lib
    /opt/homebrew/lib
    ${PostgreSQL_LIBRARY_DIRS}
)

if(PQXX_INCLUDE_DIR AND PQXX_LIBRARY)
    message(STATUS "Found libpqxx: ${PQXX_LIBRARY}")
    set(PQXX_FOUND TRUE)
else()
    message(FATAL_ERROR "libpqxx not found. Please install libpqxx development libraries.")
endif()

# Source files
set(SOURCES
    main.cpp
    src/DatabaseManager.cpp
    src/AlertSystem.cpp
    src/AlertWindow.cpp
    src/QueryEngine.cpp
    src/ConfigManager.cpp
)

# Header files
set(HEADERS
    include/DatabaseManager.h
    include/AlertSystem.h
    include/AlertWindow.h
    include/QueryEngine.h
    include/ConfigManager.h
)

# Create executable
add_executable(Ban_Delta_Breach_Notifier
    ${SOURCES}
    ${HEADERS}
)

# Include directories
target_include_directories(Ban_Delta_Breach_Notifier PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PostgreSQL_INCLUDE_DIRS}
    ${PQXX_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(Ban_Delta_Breach_Notifier
    Qt6::Core
    Qt6::Widgets
    ${PostgreSQL_LIBRARIES}
    ${PQXX_LIBRARY}
    pthread  # Often required by libpqxx
)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Ban_Delta_Breach_Notifier PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Ban_Delta_Breach_Notifier PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(Ban_Delta_Breach_Notifier PRIVATE /W4)
endif()

# Debug configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(Ban_Delta_Breach_Notifier PRIVATE DEBUG)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(Ban_Delta_Breach_Notifier PRIVATE -g -O0)
    endif()
endif()

# Release configuration
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(Ban_Delta_Breach_Notifier PRIVATE QT_NO_DEBUG_OUTPUT)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(Ban_Delta_Breach_Notifier PRIVATE -O3 -DNDEBUG)
    endif()
endif()

# Install rules
install(TARGETS Ban_Delta_Breach_Notifier
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install configuration files
install(DIRECTORY config/
    DESTINATION share/Ban_Delta_Breach_Notifier/config
    FILES_MATCHING PATTERN "*.conf"
)

# Install documentation
install(FILES README.md
    DESTINATION share/doc/Ban_Delta_Breach_Notifier
)

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "PostgreSQL-Monitor")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "PostgreSQL Real-Time Monitoring System")
set(CPACK_PACKAGE_VENDOR "Database Monitoring Systems")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6widgets6, libpqxx-7, libpq5")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtwidgets, libpqxx, libpq")
endif()

include(CPack)

# Custom targets
add_custom_target(clean-build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMENT "Cleaning build directory completely"
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== PostgreSQL Monitor Configuration Summary ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt version: ${Qt6_VERSION}")
message(STATUS "PostgreSQL version: ${PostgreSQL_VERSION}")
message(STATUS "libpqxx found: ${PQXX_FOUND}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")

# Development helper targets
if(UNIX)
    # Create a run target for development
    add_custom_target(run
        COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Ban_Delta_Breach_Notifier
        DEPENDS Ban_Delta_Breach_Notifier
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running PostgreSQL Monitor"
    )

    # Create a debug run target
    add_custom_target(debug-run
        COMMAND gdb --args ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Ban_Delta_Breach_Notifier
        DEPENDS Ban_Delta_Breach_Notifier
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running PostgreSQL Monitor under GDB"
    )
endif()

# Testing support (optional)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Documentation (optional)
option(BUILD_DOCS "Build documentation" OFF)
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_subdirectory(docs)
    endif()
endif()