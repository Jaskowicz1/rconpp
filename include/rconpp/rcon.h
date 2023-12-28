#pragma once

#ifdef _WIN32
#pragma warning( disable : 4251 ); // 4251 warns when we export classes or structures with stl member variables
#endif

#include "export.h"
#include "client.h"
#include "server.h"
#include "utilities.h"
