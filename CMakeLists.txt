cmake_minimum_required(VERSION 3.10)
project(player)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-include${CMAKE_CURRENT_SOURCE_DIR}/common/typedefs.hpp)

# Libraries
find_package(SDL2 REQUIRED)
find_package(fmt REQUIRED)

add_subdirectory(lib/emulator)

# Targets
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(
        ${PROJECT_NAME} PUBLIC
        emulator
        SDL2
        fmt
)

# Link resources
add_custom_target(
        link_resources
        ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_CURRENT_SOURCE_DIR}/res
        ${CMAKE_CURRENT_BINARY_DIR}/res
        COMMENT "Linking resources" VERBATIM
)
add_dependencies(${PROJECT_NAME} link_resources)
