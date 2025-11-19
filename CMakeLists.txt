cmake_minimum_required(VERSION 3.15)
project(ctq_test)

message( STATUS "Generator - ${CMAKE_GENERATOR}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#debug options (clang)
#set( CMAKE_CXX_FLAGS "-g -O0" )
add_link_options("-lstdc++")

find_package(GTest CONFIG REQUIRED)
# Google test
#include(FetchContent)
#FetchContent_Declare(
#	googletest
#	URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
#)
#FetchContent_MakeAvailable(googletest)

# Create header-only library
add_library(ctq INTERFACE)
target_include_directories(ctq INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)

# Install library and headers
install(TARGETS ctq
	EXPORT ctqTargets
	INCLUDES DESTINATION include
)

install(DIRECTORY include/ctq
	DESTINATION include
	FILES_MATCHING PATTERN "*.h"
)

install(EXPORT ctqTargets
	FILE ctqTargets.cmake
	NAMESPACE ctq::
	DESTINATION lib/cmake/ctq
)

# Configure and install package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/ctqConfigVersion.cmake"
	VERSION 1.0.0
	COMPATIBILITY AnyNewerVersion
)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/ctqConfig.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/ctqConfig.cmake"
	@ONLY
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/ctqConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/ctqConfigVersion.cmake"
	DESTINATION lib/cmake/ctq
)

# Test executable (not installed)
enable_testing()

file(GLOB src test/*.cpp include/ctq*.h)

add_executable(ctq_test EXCLUDE_FROM_ALL ${src})

target_link_libraries(
	ctq_test
	GTest::gtest_main
	ctq
)

include(GoogleTest)
gtest_discover_tests(ctq_test)
