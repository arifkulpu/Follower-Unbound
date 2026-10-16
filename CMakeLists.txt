cmake_minimum_required(VERSION 3.23)

project(
    FollowerUnbound
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)
add_subdirectory(extern/CommonLibSSE-NG EXCLUDE_FROM_ALL)

file(GLOB_RECURSE HEADERS 
    "src/*.h"
)

file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

add_commonlibsse_plugin(${PROJECT_NAME}
    SOURCES 
        ${HEADERS} 
        ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    "C:/Users/pc/Desktop/projeler/King Core/include"
)

target_precompile_headers(${PROJECT_NAME} PRIVATE
    src/PCH.h
)
