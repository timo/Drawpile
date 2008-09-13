# Optimize.cmake

if ( DEBUG )
	message ( STATUS "Optimization: None; Debug" )
	set ( CMAKE_CXX_FLAGS_DEBUG "-g -Wall -O0 -pipe" )
else ( )
	if ( GENERIC )
		message ( STATUS "Optimization: Generic" )
		set ( CMAKE_CXX_FLAGS_RELEASE "-march=athlon64 -mtune=generic -mno-3dnow -pipe -ffast-math -fno-rtti -Wall -DNDEBUG" )
	else ( )
		message ( STATUS "Optimization: Native" )
		if ( NOT "$ENV{CXXFLAGS}" STREQUAL "" )
			set ( CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -DNDEBUG" )
		else ( )
			set ( CMAKE_CXX_FLAGS_RELEASE "-march=native -mtune=generic -pipe -ffast-math -fno-rtti -Wall -DNDEBUG" )
		endif ( )
	endif ( )
endif ( )
