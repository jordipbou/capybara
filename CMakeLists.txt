cmake_minimum_required (VERSION 3.8)
project(capybara LANGUAGES C)

set (capybara_VERSION_MAJOR 1)
set (capybara_VERSION_MINOR 0)

set (CMAKE_C_STANDARD 99)
set (CMAKE_C_STANDARD_REQUIRED On)

if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif()

set (CMAKE_C_FLAGS_RELEASE "-O3")
set (CMAKE_EXE_LINKER_FLAGS "")

add_executable (
	tests
	src/tests.c
	deps/Unity/src/unity.c)
target_include_directories (tests PRIVATE src deps/Unity/src)
