cmake_minimum_required(VERSION 4.2)
project(webserver_for_ai)

set(CMAKE_CXX_STANDARD 20)

add_executable(webserver_for_ai
    main.cpp
    http_server.cpp
)
