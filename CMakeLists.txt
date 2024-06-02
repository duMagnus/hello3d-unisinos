cmake_minimum_required(VERSION 3.28)
project(compgraf)

set(CMAKE_CXX_STANDARD 17)

add_executable(compgraf main.cpp)

find_package(glad CONFIG REQUIRED)
target_link_libraries(compgraf PRIVATE glad::glad)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(compgraf PRIVATE glfw)

find_package(glm CONFIG REQUIRED)
target_link_libraries(compgraf PRIVATE glm::glm)

find_package(Stb REQUIRED)
target_include_directories(compgraf PRIVATE ${Stb_INCLUDE_DIR})