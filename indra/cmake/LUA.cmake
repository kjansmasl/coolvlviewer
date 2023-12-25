# -*- cmake -*-
if (LUA_CMAKE_INCLUDED)
  return()
endif (LUA_CMAKE_INCLUDED)
set (LUA_CMAKE_INCLUDED TRUE)

include(Prebuilt)

set(LUA_FIND_QUIETLY ON)
set(LUA_FIND_REQUIRED OFF)

if (USESYSTEMLIBS)
  include(FindLua)
  if (LUA_FOUND AND (LUA_VERSION_MAJOR LESS 5 OR LUA_VERSION_MINOR LESS 4))
    set(LUA_FOUND "NO")
  endif ()
endif (USESYSTEMLIBS)

if (NOT LUA_FOUND)
  use_prebuilt_binary(liblua)
  set(LUA_INCLUDE_DIR ${LIBS_PREBUILT_DIR}/include)
  if (LINUX)
    set(LUA_LIBRARIES ${LIBS_PREBUILT_DIR}/lib/release/liblua.a)
  elseif (WINDOWS)
    set(LUA_LIBRARIES ${LIBS_PREBUILT_DIR}/lib/release/lua54.lib)
  elseif (DARWIN)
    set(LUA_LIBRARIES ${LIBS_PREBUILT_DIR}/lib/release/liblua.a)
  endif (LINUX)
  set(LUA_FOUND "YES")
endif (NOT LUA_FOUND)

include_directories(SYSTEM LUA_INCLUDE_DIR)
