message("-- Building for windows (x86) with precompiled packaged dependencies")

ADD_DEFINITIONS(/bigobj)

SET(WINDOWS_32_BIT 1)

# BIG FAT STINKY KLUDGE
SET(CMAKE_CXX_COMPILER_WORKS 1)
