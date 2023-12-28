cmake_minimum_required(VERSION 3.10)

option(BUILD_TESTS "Build the test program" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_definitions(RCONPP_BUILD)

set(RCONPP_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

project(rconpp
	VERSION 0.5.0
	LANGUAGES CXX
	HOMEPAGE_URL "https://github.com/Jaskowicz1/rconpp"
	DESCRIPTION "A modern Source-RCON library for C++"
)

if(NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

if(UNIX OR MSYS)
	find_program(LDCONFIG_EXECUTABLE "ldconfig")
endif()

file(GLOB rconpp_src "src/rconpp/*.cpp" "src/rconpp/*.rc")

if(BUILD_SHARED_LIBS)
	add_library(rconpp SHARED ${rconpp_src})
else()
	add_library(rconpp STATIC ${rconpp_src})
endif()

target_include_directories(rconpp PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/rconpp>
)

set_target_properties(rconpp PROPERTIES
	VERSION ${CMAKE_PROJECT_VERSION}
	SOVERSION ${CMAKE_PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE true
)

target_compile_features(rconpp PRIVATE cxx_std_17)
target_compile_features(rconpp PRIVATE cxx_constexpr)
target_compile_features(rconpp PRIVATE cxx_lambdas)

if(BUILD_TESTS)
	add_executable(unittest "unittest/test.cpp")
	target_compile_features(unittest PRIVATE cxx_std_17)
	target_link_libraries(unittest PUBLIC rconpp)
	enable_testing()
	add_test(
		NAME unittest
		COMMAND unittest
	)
endif()

if(NOT WIN32)
	include(GNUInstallDirs)
	install(TARGETS rconpp LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
	message("Library install directory at ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
	install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
	message("Include files install directory at ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}")
	install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -DRUN_LDCONFIG=${RUN_LDCONFIG} -DLDCONFIG_EXECUTABLE=${LDCONFIG_EXECUTABLE} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/PostInstall.cmake)")

	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/rconpp.pc.in" "${CMAKE_BINARY_DIR}/rconpp.pc" @ONLY)
	install(FILES "${CMAKE_BINARY_DIR}/rconpp.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

# Setup information for packaging and distribution
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackSetup.cmake")

# CPack initialization for distribution
include(CPack)