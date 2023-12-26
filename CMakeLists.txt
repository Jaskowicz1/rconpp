cmake_minimum_required(VERSION 3.10)

option(BUILD_TESTS "Build the test program" ON)

project(rconpp
	VERSION 0.1.0
	LANGUAGES CXX
	HOMEPAGE_URL "https://github.com/Jaskowicz1/rconpp"
	DESCRIPTION "A modern Source-RCON library for C++"
)

if (DPP_BUILD_TEST)
	enable_testing(${CMAKE_CURRENT_SOURCE_DIR})
endif()

include(GNUInstallDirs)

add_library(rconpp INTERFACE)

target_include_directories(rconpp INTERFACE include/rconpp/rcon.h)

set_target_properties(rconpp PROPERTIES
	VERSION ${CMAKE_PROJECT_VERSION}
	SOVERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE true
	LINKER_LANGUAGE CXX
)

if(BUILD_TESTS)
	add_executable(unittest "unittest/test.cpp")
	target_compile_features(unittest PRIVATE cxx_std_17)
	target_link_directories(unittest PUBLIC rconpp)

	add_test(
		NAME unittest
		COMMAND unittest
	)
endif()
