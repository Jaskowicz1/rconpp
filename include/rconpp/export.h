#pragma once

#ifdef RCONPP_BUILD
	#ifdef _WIN32
		#define RCONPP_EXPORT __declspec(dllexport)
	#else
		#define RCONPP_EXPORT
	#endif
#else
	#ifdef _WIN32
		#define RCONPP_EXPORT __declspec(dllimport)
	#else
		#define RCONPP_EXPORT
	#endif
#endif
