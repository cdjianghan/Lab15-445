cmake_minimum_required(VERSION 3.8)

project(googletest-download NONE)

include(ExternalProject)
ExternalProject_Add(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG master
        SOURCE_DIR "/home/jh/15-445/15-445Learning/cmake-build-remote/googletest-src"
        BINARY_DIR "/home/jh/15-445/15-445Learning/cmake-build-remote/googletest-build"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        TEST_COMMAND ""
        )
