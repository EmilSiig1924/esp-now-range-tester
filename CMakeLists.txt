cmake_minimum_required(VERSION 3.16)
# Fixed app version: keeps builds independent of git state (an empty repo breaks git describe).
set(PROJECT_VER "1")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp_now_range_tester)
