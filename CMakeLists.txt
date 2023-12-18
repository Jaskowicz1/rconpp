cmake_minimum_required(VERSION 3.26)
project(
	rconpp
	VERSION 1.0
	LANGUAGES CXX
	HOMEPAGE_URL "https://github.com/Jaskowicz1/rconpp"
	DESCRIPTION "A modern Source-RCON library for C++"
)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(rconpp
	include/rcon.h
	src/rcon.cpp)
