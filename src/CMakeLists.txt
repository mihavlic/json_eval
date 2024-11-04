set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g -fsanitize=address")

add_executable(
  json_eval
  util.cpp
  main.cpp
  json.cpp
)
