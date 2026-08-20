cmake_minimum_required(VERSION 3.20)
project(mini-dwn VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_TLS "Enable TLS support via OpenSSL" ON)

# Dependencies - use find_package, fallback to pkg-config hints
find_package(Boost 1.74 REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# nlohmann/json - header-only, try find_package else FetchContent
find_package(nlohmann_json 3.10 QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# libpqxx
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PQXX REQUIRED libpqxx)
endif()

if(NOT PQXX_FOUND)
    find_library(PQXX_LIB pqxx REQUIRED)
    find_path(PQXX_INCLUDE pqxx/pqxx REQUIRED)
    set(PQXX_LIBRARIES ${PQXX_LIB})
    set(PQXX_INCLUDE_DIRS ${PQXX_INCLUDE})
endif()

# libsodium
find_library(SODIUM_LIB sodium)
find_path(SODIUM_INCLUDE sodium.h)

# Sources
file(GLOB_RECURSE LIB_SOURCES src/*.cpp)
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_library(dwn_core STATIC ${LIB_SOURCES})
target_include_directories(dwn_core PUBLIC include ${SODIUM_INCLUDE} ${PQXX_INCLUDE_DIRS})
target_link_libraries(dwn_core PUBLIC
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Threads::Threads
    ${SODIUM_LIB}
    ${PQXX_LIBRARIES}
)
if(PQXX_INCLUDE_DIRS)
    target_include_directories(dwn_core PUBLIC ${PQXX_INCLUDE_DIRS})
    target_link_directories(dwn_core PUBLIC ${PQXX_LIBRARY_DIRS})
endif()

add_executable(mini-dwn src/main.cpp)
target_link_libraries(mini-dwn PRIVATE dwn_core)

# Tests
if(BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    file(GLOB TEST_SOURCES tests/*.cpp)
    add_executable(dwn_tests ${TEST_SOURCES})
    target_link_libraries(dwn_tests PRIVATE dwn_core GTest::gtest GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(dwn_tests)
endif()

# Install
install(TARGETS mini-dwn DESTINATION bin)
