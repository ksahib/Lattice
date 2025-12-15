cmake_minimum_required(VERSION 3.16)
project(drogon_upload_example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED CONFIG)

add_executable(server main.cpp worker_manager.cpp)
target_link_libraries(server PRIVATE Drogon::Drogon)
