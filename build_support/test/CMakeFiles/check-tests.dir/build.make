# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/jh/15-445/15-445Learning

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/jh/15-445/15-445Learning/build_support

# Utility rule file for check-tests.

# Include the progress variables for this target.
include test/CMakeFiles/check-tests.dir/progress.make

test/CMakeFiles/check-tests:
	cd /home/jh/15-445/15-445Learning/build_support/test && /usr/bin/ctest --verbose

check-tests: test/CMakeFiles/check-tests
check-tests: test/CMakeFiles/check-tests.dir/build.make

.PHONY : check-tests

# Rule to build all files generated by this target.
test/CMakeFiles/check-tests.dir/build: check-tests

.PHONY : test/CMakeFiles/check-tests.dir/build

test/CMakeFiles/check-tests.dir/clean:
	cd /home/jh/15-445/15-445Learning/build_support/test && $(CMAKE_COMMAND) -P CMakeFiles/check-tests.dir/cmake_clean.cmake
.PHONY : test/CMakeFiles/check-tests.dir/clean

test/CMakeFiles/check-tests.dir/depend:
	cd /home/jh/15-445/15-445Learning/build_support && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/jh/15-445/15-445Learning /home/jh/15-445/15-445Learning/test /home/jh/15-445/15-445Learning/build_support /home/jh/15-445/15-445Learning/build_support/test /home/jh/15-445/15-445Learning/build_support/test/CMakeFiles/check-tests.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : test/CMakeFiles/check-tests.dir/depend

