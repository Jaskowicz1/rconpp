cmake_minimum_required(VERSION 3.10)

project(rconpp
	VERSION 0.1.0
	LANGUAGES CXX
	HOMEPAGE_URL "https://github.com/Jaskowicz1/rconpp"
	DESCRIPTION "A modern Source-RCON library for C++"
)

include(GNUInstallDirs)

add_library(rconpp INTERFACE)

target_include_directories(rconpp INTERFACE include/rconpp/rcon.h)

set_target_properties(rconpp PROPERTIES
	VERSION ${CMAKE_PROJECT_VERSION}
	SOVERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE true
	LINKER_LANGUAGE CXX
)
