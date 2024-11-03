cmake_minimum_required(VERSION 3.20)
project(json_eval LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address")

include_directories(src)

add_executable(
  json_eval
  src/util.cpp
  src/main.cpp
)
