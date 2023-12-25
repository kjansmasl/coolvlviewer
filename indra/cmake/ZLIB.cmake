# -*- cmake -*-
if (ZLIB_CMAKE_INCLUDED)
  return()
endif (ZLIB_CMAKE_INCLUDED)
set (ZLIB_CMAKE_INCLUDED TRUE)

include(Prebuilt)

# Note: We do not allow USESYSTEMLIBS here, because we are using zlib-ng,
# which is very unlikely to be used on any Linux system (for now, at least),
# and if present, just as unlikely with the same compile-time configuration
# as ours... HB
#if (USESYSTEMLIBS)
#  set(ZLIB_FIND_QUIETLY ON)
#  set(ZLIB_FIND_REQUIRED OFF)
#  include(FindZLIBNG)
#  find_library(MINIZIP_LIBRARY NAMES minizip)
#  set(MINIZIP_FIND_QUIETLY ON)
#  set(MINIZIP_FIND_REQUIRED OFF)
#  include(FindPackageHandleStandardArgs)
#  find_package_handle_standard_args(MiniZip DEFAULT_MSG MINIZIP_LIBRARY)
#endif (USESYSTEMLIBS)

#if (NOT ZLIB_FOUND OR NOT MINIZIP_FOUND)
  use_prebuilt_binary(zlib)
  if (WINDOWS)
    set(ZLIB_LIBRARIES zlib)
    set(MINIZIP_LIBRARIES libminizip)
    set(ZLIB_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/zlib-ng)
  elseif (LINUX)
    set(ZLIB_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/libz.a)
    set(MINIZIP_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/libminizip.a)
    #
    # When we have updated static libraries in competition with older shared
    # libraries and we want the former to win, we need to do some extra work.
    # The *_PRELOAD_ARCHIVES settings are invoked early and will pull in the
    # entire archive to the binary giving it priority in symbol resolution.
    # Beware of cmake moving the archive load itself to another place on the
    # link command line. If that happens, you can try something like -Wl,-lz
    # here to hide the archive. Also be aware that the linker will not tolerate
    # a second whole-archive load of the archive. See viewer's CMakeLists.txt
    # for more information.
    #
    set(ZLIB_PRELOAD_ARCHIVES "-Wl,--whole-archive ${ZLIB_LIBRARIES} -Wl,--no-whole-archive")
    set(ZLIB_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/zlib-ng)
  elseif (DARWIN)
    set(ZLIB_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/libz.a)
    set(MINIZIP_LIBRARIES ${ARCH_PREBUILT_DIRS_RELEASE}/libminizip.a)
    set(ZLIB_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/zlib-ng)
  endif (WINDOWS)
#else (NOT ZLIB_FOUND OR NOT MINIZIP_FOUND)
#  set(MINIZIP_LIBRARIES ${MINIZIP_LIBRARY})
#endif (NOT ZLIB_FOUND OR NOT MINIZIP_FOUND)

include_directories(SYSTEM ${ZLIB_INCLUDE_DIRS})
