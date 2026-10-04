cmake_minimum_required(VERSION 3.13)

project(FlpAnalyzer)

add_definitions(-DLOGIC2)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum supported MacOS version" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES
    src/FlpAnalyzer.cpp
    src/FlpAnalyzer.h
    src/FlpAnalyzerResults.cpp
    src/FlpAnalyzerResults.h
    src/FlpAnalyzerSettings.cpp
    src/FlpAnalyzerSettings.h
    src/FlpSimulationDataGenerator.cpp
    src/FlpSimulationDataGenerator.h
    src/FlpDecode.cpp
    src/FlpDecode.h
)

add_analyzer_plugin(${PROJECT_NAME} SOURCES ${SOURCES})

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(flp_decode_test src/FlpDecodeTest.cpp src/FlpDecode.cpp)
if(NOT MSVC)
    target_compile_options(flp_decode_test PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

enable_testing()
add_test(NAME flp_decode_test COMMAND flp_decode_test)
