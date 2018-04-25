cmake_minimum_required(VERSION 3.10)
project(player)

set(CMAKE_CXX_STANDARD 17)

# Libraries
add_subdirectory(lib/emulator)

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC emulator fmt)
