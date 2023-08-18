cmake_minimum_required(VERSION 3.12)

## define our project
project(netzer)

option(BUILD_SHARED_LIBS "Build Shared Libraries" ON)

## request C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(LIB_NAME ${PROJECT_NAME})

set(LIB_TYPE SHARED)
if (NOT BUILD_SHARED_LIBS)
    set(LIB_TYPE STATIC)
    set(Boost_USE_STATIC_LIBS ON)
endif (NOT BUILD_SHARED_LIBS)

## cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules ${CMAKE_MODULE_PATH})

set(LIBS ${LIBS})

##### BOOST
set(BOOST_COMPONENTS system)
find_package(Boost REQUIRED COMPONENTS ${BOOST_COMPONENTS})
include_directories(${Boost_INCLUDE_DIRS})
set(LIBS ${LIBS} ${Boost_LIBRARIES})
#####

##### CURL
find_package(CURL REQUIRED)
include_directories(${CURL_INCLUDE_DIR})
set(LIBS ${LIBS} ${CURL_LIBRARY})
#####

include_directories(${PROJECT_SOURCE_DIR}/include)

# add library-target
add_subdirectory("src")

if(MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /W4) #TODO /WX (warnings as errors)
else()
    target_compile_options(${LIB_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()


# Expose public includes (including Boost transitively) to other
# subprojects through cache variable.
set(${PROJECT_NAME}_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/include
        ${Boost_INCLUDE_DIRS}
        CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

set(${PROJECT_NAME}_LIBRARIES ${PROJECT_NAME}
        CACHE INTERNAL "${PROJECT_NAME}: Libraries" FORCE)
