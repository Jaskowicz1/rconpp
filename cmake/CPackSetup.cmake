include(GNUInstallDirs)

# Prepare information for packaging into .zip, .deb, .rpm
## Project installation metadata
set(CPACK_PACKAGE_NAME   librconpp)
set(CPACK_PACKAGE_VENDOR "Archie Jaskowicz")	# Maker of the application
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A modern Source RCON library for C++")
set(CPACK_PACKAGE_DESCRIPTION "A modern Source RCON library for C++")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Jaskowicz1/rconpp")
set(CPACK_FREEBSD_PACKAGE_MAINTAINER "archiejaskowicz@gmail.com")
set(CPACK_FREEBSD_PACKAGE_ORIGIN "misc/librconpp")
set(CPACK_RPM_PACKAGE_LICENSE "Apache 2.0")
set(CPACK_PACKAGE_CONTACT "https://jaskowicz.xyz/contact")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "A modern Source RCON library for C++")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 0)

## Select generated based on what operating system
if(WIN32)
	set(CPACK_GENERATOR ZIP)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(CPACK_GENERATOR "DEB")
else()
	set(CPACK_GENERATOR "TBZ2")
endif()