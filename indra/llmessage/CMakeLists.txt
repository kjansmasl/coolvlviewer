# -*- cmake -*-

project(llmessage)

include(00-Common)
include(JsonCpp)
include(LLCommon)
include(LLFilesystem)
include(LLMath)
include(LLMessage)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(llmessage_SOURCE_FILES
    llassetstorage.cpp
    llavatarnamecache.cpp
    llbuffer.cpp
    llbufferstream.cpp
    llcachename.cpp
    llcircuit.cpp
    llclassifiedflags.cpp
    llcoproceduremanager.cpp
    llcorebufferarray.cpp
    llcorebufferstream.cpp
    llcorehttpcommon.cpp
    llcorehttpheaders.cpp
    llcorehttplibcurl.cpp
    llcorehttpopcancel.cpp
    llcorehttpoperation.cpp
    llcorehttpoprequest.cpp
    llcorehttpopsetget.cpp
    llcorehttpoptions.cpp
    llcorehttppolicy.cpp
    llcorehttppolicyclass.cpp
    llcorehttppolicyglobal.cpp
    llcorehttpreplyqueue.cpp
    llcorehttprequest.cpp
    llcorehttprequestqueue.cpp
    llcorehttpresponse.cpp
    llcorehttpservice.cpp
    llcorehttputil.cpp
    lldatapacker.cpp
    lldispatcher.cpp
    llexperiencecache.cpp
    llhost.cpp
    llhttpconstants.cpp
    llhttpnode.cpp
    llhttpretrypolicy.cpp
    llhttpsdhandler.cpp
    llinstantmessage.cpp
    lliopipe.cpp
    lliosocket.cpp
    llmessage.cpp
    llmessageconfig.cpp
    llmessagereader.cpp
    llmessagetemplate.cpp
    llmessagetemplateparser.cpp
    llnamevalue.cpp
    llnet.cpp
    llpacketack.cpp
    llpacketring.cpp
    llpartdata.cpp
    llpatch_code.cpp
    llpatch_dct.cpp
    llproxy.cpp
    llpumpio.cpp
    llsdmessagebuilder.cpp
    llsdmessagereader.cpp
    lltemplatemessagebuilder.cpp
    lltemplatemessagereader.cpp
    llthrottle.cpp
    lltransfermanager.cpp
    lltransfersourceasset.cpp
    lltransfersourcefile.cpp
    lltransfertargetfile.cpp
    lltransfertargetvfile.cpp
    llurlhistory.cpp
    lluserauth.cpp
    llxfer.cpp
    llxfer_file.cpp
    llxfermanager.cpp
    llxfer_mem.cpp
    llxfer_vfile.cpp
    llxmlrpctransaction.cpp
    message_prehash.cpp
    )

set(llmessage_HEADER_FILES
    CMakeLists.txt
    llassetstorage.h
    llavatarnamecache.h
    llbuffer.h
    llbufferstream.h
    llcachename.h
    llcircuit.h
    llclassifiedflags.h
    llcoproceduremanager.h
    llcorebufferarray.h
    llcorebufferstream.h
    llcorehttpcommon.h
    llcorehttphandler.h
    llcorehttpheaders.h
    llcorehttpinternal.h
    llcorehttplibcurl.h
    llcorehttpopcancel.h
    llcorehttpoperation.h
    llcorehttpoprequest.h
    llcorehttpopsetget.h
    llcorehttpoptions.h
    llcorehttppolicy.h
    llcorehttppolicyclass.h
    llcorehttppolicyglobal.h
    llcorehttpreplyqueue.h
    llcorehttprequest.h
    llcorehttprequestqueue.h
    llcorehttpresponse.h
    llcorehttpservice.h
    llcorehttputil.h
    llcoremutex.h
    llcorerefcounted.h
    lldatapacker.h
    lldbstrings.h
    lldispatcher.h
    llexperiencecache.h
    llextendedstatus.h
    llhost.h
    llhttpconstants.h
    llhttpnode.h
    llhttpretrypolicy.h
    llhttpsdhandler.h
    llinstantmessage.h
    lliopipe.h
    lliosocket.h
    llmessage.h
    llmessagebuilder.h
    llmessageconfig.h
    llmessagereader.h
    llmessagetemplate.h
    llmessagetemplateparser.h
    llnamevalue.h
    llnet.h
    llpacketack.h
    llpacketring.h
    llpartdata.h
    llpatch_code.h
    llpatch_dct.h
    llproxy.h
    llpumpio.h
    llqueryflags.h
    llregionflags.h
    llsdmessagebuilder.h
    llsdmessagereader.h
    llteleportflags.h
    lltemplatemessagebuilder.h
    lltemplatemessagereader.h
    llthrottle.h
    lltransfermanager.h
    lltransfersourceasset.h
    lltransfersourcefile.h
    lltransfertargetfile.h
    lltransfertargetvfile.h
    llurlhistory.h
    lluserauth.h
    llxfer.h
    llxfermanager.h
    llxfer_file.h
    llxfer_mem.h
    llxfer_vfile.h
    llxmlrpctransaction.h  
    message_prehash.h
    )

set_source_files_properties(${llmessage_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

if (DARWIN OR LINUX)
  # Boost headers define unused members in condition_variable so...
  set_source_files_properties(${llmessage_SOURCE_FILES}
                              PROPERTIES COMPILE_FLAGS -Wno-unused-variable)
endif (DARWIN OR LINUX)

list(APPEND llmessage_SOURCE_FILES ${llmessage_HEADER_FILES})

add_library (llmessage ${llmessage_SOURCE_FILES})
add_dependencies(llmessage prepare)
target_link_libraries(
    llmessage
    ${CURL_LIBRARIES}
    ${NGHTTP2_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CRYPTO_LIBRARIES}
    ${JSONCPP_LIBRARIES}
    ${XMLRPCEPI_LIBRARIES}
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llmessage PRIVATE <linden_common.h> <llfasttimer.h> <hbfastmap.h>)
  if (USEUNITYBUILD)
    # For some reason, cmake (at least v3.20.0) seems to refuse to use unity
    # build for llmessage, even when asked to... HB
    set_target_properties(llmessage PROPERTIES UNITY_BUILD ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llmessage COMPILE_DEFINITIONS = ${ALLDEFINES}")
