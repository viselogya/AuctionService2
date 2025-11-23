cmake_minimum_required(VERSION 3.18)
project(AuctionService VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  cpp_httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.12.3
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(cpp_httplib nlohmann_json)

find_package(PostgreSQL REQUIRED)
find_package(CURL REQUIRED)

file(GLOB_RECURSE AUCTION_SOURCES
  CONFIGURE_DEPENDS
  src/core/*.cpp
  src/repository/*.cpp
  src/service/*.cpp
  src/api/*.cpp
  src/model/*.cpp
  src/main.cpp
)

add_executable(auction_service ${AUCTION_SOURCES})

target_include_directories(auction_service PRIVATE
  include
  src
)

target_link_libraries(auction_service
  PRIVATE
    httplib::httplib
    nlohmann_json::nlohmann_json
    PostgreSQL::PostgreSQL
    CURL::libcurl
)

if(MSVC)
  target_compile_options(auction_service PRIVATE /W4 /permissive-)
else()
  target_compile_options(auction_service PRIVATE -Wall -Wextra -Wpedantic)
endif()

