# -*- cmake -*-

if (LINUX)
	# Failing to specify a full path causes a link failure for plugins (that
	# themselves link to llcommon which consumes libjsoncpp.a): bug in cmake !
	set(JSONCPP_LIBRARIES ${CMAKE_BINARY_DIR}/libjsoncpp/libjsoncpp.a)
else ()
	set(JSONCPP_LIBRARIES jsoncpp)
endif ()

include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/libjsoncpp/json)
