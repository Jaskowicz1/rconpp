cmake_minimum_required(VERSION 3.10)
project(rconpp
	VERSION 0.1.0
	LANGUAGES CXX
	HOMEPAGE_URL "https://github.com/Jaskowicz1/rconpp"
	DESCRIPTION "A modern Source-RCON library for C++"
)

add_library(rconpp SHARED include/rcon.h)

target_link_directories(rconpp PUBLIC include)

target_include_directories(rconpp PUBLIC
	"${PROJECT_BINARY_DIR}"
	"${PROJECT_SOURCE_DIR}/include"
)

set_target_properties(rconpp PROPERTIES
	VERSION ${CMAKE_PROJECT_VERSION}
	SOVERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE true
	 LINKER_LANGUAGE CXX
)

set(CMAKE_CXX_STANDARD 17)
