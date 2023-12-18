cmake_minimum_required(VERSION 3.26)
project(
	rconpp
	VERSION 0.1.0
	LANGUAGES CXX
	HOMEPAGE_URL "https://github.com/Jaskowicz1/rconpp"
	DESCRIPTION "A modern Source-RCON library for C++"
)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(rconpp SHARED
	include/rcon.h
	src/rcon.cpp)

set_target_properties(rconpp PROPERTIES
	VERSION ${CMAKE_PROJECT_VERSION}
	SOVERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE true
)
target_include_directories(rconpp PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/dpp>
)
