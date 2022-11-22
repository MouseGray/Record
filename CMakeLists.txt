cmake_minimum_required(VERSION 3.5)

project(Record LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILE_FEATURES OFF)

add_executable(Record
    main.cpp
    value.h
    record.h
    serializer.h
    storage.h storage.cpp
)
