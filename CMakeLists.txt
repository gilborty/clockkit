cmake_minimum_required(VERSION 3.14)
project(ClockKit)

# Compiler flags
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -fPIC -Wall -Werror -Weffc++ -pedantic")

# Find dependencies
find_package(PkgConfig)

# Note(gilbert): Don't worry about building the Ruby, Python, or TCL bindings
set(CLOCKKIT_SOURCES
    ClockKit/ClockClient.cpp
    ClockKit/ClockPacket.cpp
    ClockKit/ClockServer.cpp
    ClockKit/ConfigReader.cpp
    ClockKit/PhaseLockedClock.cpp
    ClockKit/SystemClock.cpp
    ClockKit/Timestamp.cpp
    ClockKit/VariableFrequencyClock.cpp
    ClockKit/clockkit.cpp
)

set(CLOCKKIT_EXE_SOURCES
    ClockKit/ClockServerMain.cpp
    ClockKit/kissnet-udp.cpp
    ClockKit/PhaseLockMain.cpp
    ClockKit/test-unit.cpp
)

# Build the library statically 
set(CLOCKKIT_ALL_SOURCES ${CLOCKKIT_SOURCES} ${CLOCKKIT_EXE_SOURCES})
add_library(clockkit STATIC ${CLOCKKIT_SOURCES})
target_include_directories(clockkit
PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Executables
add_executable(ckserver ClockKit/ClockServerMain.cpp)
target_link_libraries(ckserver clockkit)

add_executable(ckphaselock ClockKit/PhaseLockMain.cpp)
target_link_libraries(ckphaselock clockkit)

add_executable(cktest-standalone ClockKit/test-standalone.cpp)
target_link_libraries(cktest-standalone clockkit)

add_executable(cktest-unit ClockKit/test-unit.cpp)
target_link_libraries(cktest-unit clockkit)