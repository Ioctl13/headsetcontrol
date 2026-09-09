cmake_minimum_required(VERSION 3.10.0)
project(headset_ctl VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    set(CMAKE_CXX_FLAGS "/Od /Ob0 /RTC1 /D_DEBUG /MDd")
elseif(CMAKE_BUILD_TYPE STREQUAL Release)
    set(CMAKE_CXX_FLAGS "/O2 /Ob2 /Oy /GS- /GL /LTCG /OPT:REF /OPT:ICF")
endif()
message("building with flags: ${CMAKE_CXX_FLAGS}")
add_executable(headset_ctl main.cpp)
target_link_libraries(headset_ctl ws2_32.lib Bthprops.lib)