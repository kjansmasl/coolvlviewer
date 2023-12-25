# -*- cmake -*-

project(llui)

include(00-Common)
include(Hunspell)
include(LLCommon)
include(LLFilesystem)
include(LLImage)
include(LLInventory)
include(LLMath)
include(LLMessage)
include(LLRender)
include(LLWindow)
include(LLXML)

set(llui_SOURCE_FILES
    llalertdialog.cpp
    llbutton.cpp
    llcheckboxctrl.cpp
    llclipboard.cpp
    llcombobox.cpp
    llconfirmationmanager.cpp
    llconsole.cpp
    llcontainerview.cpp
    lldraghandle.cpp
    lleditmenuhandler.cpp
    hbexternaleditor.cpp
    hbfileselector.cpp
    llfloater.cpp
    hbfloateruserauth.cpp
    llfocusmgr.cpp
    lliconctrl.cpp
    llkeywords.cpp
    lllineeditor.cpp
    lllocale.cpp
    llmenugl.cpp
    llmimetypes.cpp
    llmodaldialog.cpp
    llmultislider.cpp
    llmultisliderctrl.cpp
    llnamebox.cpp
    llnameeditor.cpp
    llnamelistctrl.cpp
    llnotifications.cpp
    llpanel.cpp
    llprogressbar.cpp
    llradiogroup.cpp
    llresizebar.cpp
    llresizehandle.cpp
    llrootview.cpp
    llscrollbar.cpp
    llscrollcontainer.cpp
    llscrollingpanellist.cpp
    llscrolllistctrl.cpp
    llslider.cpp
    llsliderctrl.cpp
    llspellcheck.cpp
    llspinctrl.cpp
    llstatbar.cpp
    llstatgraph.cpp
    llstatview.cpp
    llstyle.cpp
    llstylemap.cpp
    lltabcontainer.cpp
    lltextbox.cpp
    lltexteditor.cpp
    lltrans.cpp
    llui.cpp
    lluictrl.cpp
    lluictrlfactory.cpp
    lluistring.cpp
    llundo.cpp
    lluploaddialog.cpp
    llview.cpp
    llviewborder.cpp
    llviewquery.cpp
    llvirtualtrackball.cpp
    llxyvector.cpp
    )
    
set(llui_HEADER_FILES
    CMakeLists.txt

    llalertdialog.h
    llbutton.h
    llcallbackmap.h
    llcheckboxctrl.h
    llclipboard.h
    llcombobox.h
    llconfirmationmanager.h
    llconsole.h
    llcontainerview.h
    lldraghandle.h
    lleditmenuhandler.h
    hbexternaleditor.h
    hbfileselector.h
    llfloater.h
    hbfloateruserauth.h
    llfocusmgr.h
    llfunctorregistry.h
    llhtmlhelp.h
    lliconctrl.h
    llkeywords.h
    lllineeditor.h
    lllocale.h
    llmemberlistener.h
    llmenugl.h
    llmimetypes.h
    llmodaldialog.h
    llmultisliderctrl.h
    llmultislider.h
    llnamebox.h
    llnameeditor.h
    llnamelistctrl.h
    llnotifications.h
    llpanel.h
    llprogressbar.h
    llradiogroup.h
    llresizebar.h
    llresizehandle.h
    llrootview.h
    llscrollbar.h
    llscrollcontainer.h
    llscrollingpanellist.h
    llscrolllistctrl.h
    llsliderctrl.h
    llslider.h
    llspellcheck.h
    llspinctrl.h
    llstatbar.h
    llstatgraph.h
    llstatview.h
    llstyle.h
    llstylemap.h
    lltabcontainer.h
    lltextbox.h
    lltexteditor.h
    lltrans.h
    lluictrlfactory.h
    lluictrl.h
    llui.h
    lluistring.h
    llundo.h
    lluploaddialog.h
    llview.h
    llviewborder.h
    llviewquery.h
    llvirtualtrackball.h
    llxyvector.h
    )

set_source_files_properties(${llui_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND llui_SOURCE_FILES ${llui_HEADER_FILES})

add_library (llui ${llui_SOURCE_FILES})
# Libraries on which this library depends, needed for Linux builds
# Sort by high-level to low-level
target_link_libraries(llui
    llrender
    llwindow
    llimage
    llfilesystem	# ugh, just for LLDir
    llxml
    llcommon		# must be after llimage, llwindow, llrender
    llmath
    )

if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
  target_precompile_headers(llui PRIVATE <linden_common.h> <llfasttimer.h> llview.h)
  if (USEUNITYBUILD)
    set_target_properties(llui PROPERTIES UNITY_BUILD ON)
  endif (USEUNITYBUILD)
endif ()

get_directory_property(ALLDEFINES COMPILE_DEFINITIONS)
message("llui COMPILE_DEFINITIONS = ${ALLDEFINES}")
