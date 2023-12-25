# -*- cmake -*-

project(llcommon)

include(00-Common)
include(APR)
include(Boost)
include(EXPAT)
include(LLCommon)
include(Linking)
include(Tracy)
include(JsonCpp)
include(xxHash)
include(ZLIB)

set(llcommon_SOURCE_FILES
    indra_constants.cpp
    llapp.cpp
    llapr.cpp
    llassettype.cpp
    llbase64.cpp
    llbitpack.cpp
    llcallbacklist.cpp
    llcommandlineparser.cpp
    llcommon.cpp
    llcoros.cpp
    llcrc.cpp
    llcriticaldamp.cpp
    lldate.cpp
    lldependencies.cpp
    lldictionary.cpp
    llerror.cpp
    llevent.cpp
    lleventcoro.cpp
    lleventdispatcher.cpp
    llevents.cpp
    lleventtimer.cpp
    llfasttimer.cpp
    llfile.cpp
    llframetimer.cpp
    llleap.cpp
    llliveappconfig.cpp
    lllivefile.cpp
    llmd5.cpp
    llmemory.cpp
    llmortician.cpp
    llmutex.cpp
    llpointer.cpp
    llprocesslauncher.cpp
    llqueuedthread.cpp
    llrand.cpp
    llrefcount.cpp
    llsafehandle.cpp
    llsd.cpp
    llsdjson.cpp
    llsdserialize.cpp
    llsdutil.cpp
    llsingleton.cpp
    llstat.cpp
    llstreamtools.cpp
    llstring.cpp
    llstringtable.cpp
    llsys.cpp
    llthread.cpp
    llthreadpool.cpp
    lltimer.cpp
    hbtracy.cpp
    lluri.cpp
    lluuid.cpp
    llworkerthread.cpp
    llworkqueue.cpp
    llxorcipher.cpp
    hbxxh.cpp
    )
    
set(llcommon_HEADER_FILES
    CMakeLists.txt

    indra_constants.h
    linden_common.h
    llalignedarray.h
    llapp.h
    llapr.h
    llassettype.h
    llatomic.h
    llbase64.h
    llbitpack.h
    llcallbacklist.h
    llcommandlineparser.h
    llcommon.h
    llcommonmath.h
    llcoros.h
    llcrc.h
    llcriticaldamp.h
    lldate.h
    lldependencies.h
    lldictionary.h
    llendianswizzle.h
    llerror.h
    llerrorcontrol.h
    llevent.h
    lleventcoro.h
    lleventdispatcher.h
    llevents.h
    lleventtimer.h
    hbfastmap.h
    hbfastset.h
    llfasttimer.h
    llfile.h
    llframetimer.h
    llhandle.h
    llinitdestroyclass.h
    llinstancetracker.h
    llleap.h
    llliveappconfig.h
    lllivefile.h
    llmd5.h
    llmemory.h
    llmemorystream.h
    llmortician.h
    llmutex.h
    llpointer.h
    llpreprocessor.h
    llpriqueuemap.h
    llprocesslauncher.h
    llqueuedthread.h
    llrand.h
    llrefcount.h
    llsafehandle.h
    llsd.h
    llsdjson.h
    llsdserialize.h
    llsdutil.h
    llsingleton.h
    llstat.h
    llstl.h
    llstreamtools.h
    llstrider.h
    llstring.h
    llstringtable.h
    llsys.h
    llthread.h
    llthreadpool.h
    llthreadsafequeue.h
    lltimer.h
    hbtracy.h
    lluri.h
    lluuid.h
    llversionviewer.h
    llwin32headers.h
    llwin32headerslean.h
    llworkerthread.h
    llworkqueue.h
    llxorcipher.h
    hbxxh.h
    stdtypes.h
    )

set_source_files_properties(${llcommon_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llcommon_SOURCE_FILES ${llcommon_HEADER_FILES})

if (NOT WINDOWS)
  add_definitions(-fPIC)
endif (NOT WINDOWS)

add_library(llcommon ${llcommon_SOURCE_FILES})

add_dependencies(llcommon prepare)
target_link_libraries(llcommon
    ${WINDOWS_LIBRARIES}
    ${APR_LIBRARIES}
    ${EXPAT_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${TRACY_LIBRARY}
    ${BOOST_FIBER_LIBRARY}
    ${BOOST_CONTEXT_LIBRARY}
    ${BOOST_PROGRAM_OPTIONS_LIBRARY}
    ${BOOST_THREAD_LIBRARY}
    ${BOOST_CHRONO_LIBRARY}
    ${BOOST_SYSTEM_LIBRARY}
    ${BOOST_ATOMIC_LIBRARY}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llcommon PRIVATE linden_common.h hbfastmap.h)
  if (USEUNITYBUILD)
    set_target_properties(llcommon PROPERTIES UNITY_BUILD ON)
	# Exclude this one because of old events and new events ambiguity...
	set_property(SOURCE llevent.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llcommon COMPILE_DEFINITIONS = ${ALLDEFINES}")
