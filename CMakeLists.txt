cmake_minimum_required(VERSION 3.1)

project(hoverpractice)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRCS
    hoverpractice.cpp
    sdl.cpp
    )

set(HEADERS
    controller.h
    sdl.h
    )

if(WIN32)
    add_definitions(-DUSE_XINPUT)
    set(SRCS ${SRCS} xinput.cpp)
    set(HEADERS ${HEADERS} xinput.h)
    if(MSVC)
        add_definitions(-DUSE_DINPUT)
        set(SRCS ${SRCS} dinput.cpp)
        set(HEADERS ${HEADERS} dinput.h)
    endif()
endif()

add_executable(hoverpractice ${SRCS} ${HEADERS})
