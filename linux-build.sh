#!/usr/bin/env bash

# build-linux.sh v2.22 (c)2013-2023 Henri Beauchamp.
# Released under the GPL license. https://www.gnu.org/licenses/gpl-3.0.txt

export LANG=C

COMMAND_LINE="$*"

SCRIPTSRC=`readlink -f "$0" || echo "$0"`
RUN_PATH=`dirname "$SCRIPTSRC" || echo .`
cd "$RUN_PATH"

function has_option()
{
	echo "$COMMAND_LINE" | grep "\-$1" &>/dev/null
	return $?
}

function has_exec()
{
	which "$1" &>/dev/null
	return $?
}

GCC=""
GXX=""
function find_gcc()
{
	for i in $1".9" $1".8" $1".7" $1".6" $1".5" $1".4" $1".3" $1".2" $1".1" $1".0" $1 ; do
		if has_exec "gcc-$i" && has_exec "g++-$i" ; then
			echo "Will use gcc/g++ v$i to build the viewer"
			GCC="gcc-$i"
			GXX="g++-$i"
			return
		fi
		if has_exec "gcc$i" && has_exec "g++$i" ; then
			echo "Will use gcc/g++ v$i to build the viewer"
			GCC="gcc$i"
			GXX="g++$i"
			return
		fi
	done
	GCC=""
	GXX=""
}

PYTHON=""
function find_python()
{
	for i in "python3.12" "python3.11" "python3.10" "python3.9" "python3.8" "python3.7" "python3.6" "python3.5" "python3.4" "python3.3" "python3" "python2.7" "python2.6" "python2" "python"; do
		PYTHON=`which $i 2>/dev/null`
		if [ "$PYTHON" != "" ] && [ -x "$PYTHON" ] ; then
			return
		fi
	done
	PYTHON=""
}

if has_option "h" ; then
	echo "Usage: $0 [-option|--long-build-option...]|[-z|--zap]|[-h|--help]"
	echo "With  -v6.N         : use gcc v6.N (with N=0 to 5) if possible/found."
	echo "      -v7.N         : use gcc v7.N (with N=0 to 5) if possible/found."
	echo "      -v8.N         : use gcc v8.N (with N=0 to 5) if possible/found."
	echo "      -v9.N         : use gcc v9.N (with N=0 to 5) if possible/found."
	echo "      -v10.N        : use gcc v10.N (with N=0 to 4) if possible/found."
	echo "      -v11.N        : use gcc v11.N (with N=0 to 4) if possible/found."
	echo "      -v12.N        : use gcc v12.N (with N=0 to 3) if possible/found."
	echo "      -v13.N        : use gcc v13.N (with N=0 to 2) if possible/found."
	echo "      -c, --clang   : use clang (if found) instead of gcc."
	echo "      -i, --ignore-warnings : do not treat compiler warnings as errors."
	echo "      -t, --tune    : tune the optimizations for the build system CPU."
	echo "      -l, --lto     : use the link time optimization (not supported by all compilers)."
	echo "      --protect-stack : use canaries to protect from stack overflows (slower code)."
	echo "      --no-exec-stack : ask gcc not to make stack executable (WARNING: highly experimental)."
	echo "      -o, --openmp  : enable OpenMP optimizations (WARNING: highly experimental)."
	echo "      -n, --ninja   : use ninja (if found) to build instead of make."
	echo "      --unity-build : use cmake UNITY_BUILD for faster compilation (WARNING: highly experimental)."
	echo "      -d, --debug   : build the Debug viewer binary instead of Release."
	echo "      -s, --symbols : build with debugging symbols (RelWithDebInfo viewer binary)."
	echo "      --tracy       : enable Tracy profiling support (for devel builds only)."
	echo "      -g, --gprof   : enable gprof profiling (for devel builds only)."
	echo "      -u, --usesystemlibs : use system libs instead of pre-built ones where possible."
	echo "      -p, --patches : apply patches held inside ~/.secondlife/patches/cool_vl_viewer/"
	echo "      -z, --zap     : cleanup the source tree, removing any former build and library."
	echo "      -h, --help    : this help..."
	exit 0
fi

if ! [ -x scripts/develop.py ] ; then
	echo
	echo "This script must be ran from the linden/ Cool VL Viewer sources directory !".
	exit 1
fi

if has_option "z" ; then
	# Possible *.orig or *~ files produced during patching
	if [ -f applied_patches.txt ] ; then
		find . -name "*~" -exec rm -f {} \;
		find . -name "*.orig" -exec rm -f {} \;
	fi
	# Link to system python
	rm -f python
	# Compiled binaries
	rm -rf viewer-linux-*
	# Pre-built libraries and includes
	rm -rf bin docs include lib LICENSES autobuild-package.xml
	rm -f installed.xml
	# Static data, textures, icons and fonts
	rm -rf indra/newview/app_settings/dictionaries/ indra/newview/app_settings/windlight/
	rm -rf indra/newview/cursors_mac/ indra/newview/res-sdl/
	rm -f indra/newview/character/*.llm
	rm -f indra/newview/character/*.tga
	rm -rf indra/newview/character/anims/
	for i in indra/newview/res/*.BMP;do echo -n >$i;done
	for i in indra/newview/res/*.cur;do echo -n >$i;done
	for i in indra/newview/res/*.ico;do echo -n >$i;done
	rm -f indra/newview/res/*.png
	rm -rf indra/newview/fonts/
	rm -f indra/newview/cool_vl_viewer.icns
	# Skin textures
	find indra/newview/skins/ -name "*.png" -exec rm -f {} \;
	find indra/newview/skins/ -name "*.tga" -exec rm -f {} \;
	find indra/newview/skins/ -name "*.j2c" -exec rm -f {} \;
	# Build stuff
	find . -type d -name ".distcc" -exec rm -rf {} \; &>/dev/null
	echo "Source tree cleaned-up."
	exit 0
fi

arch=`uname -m`
if [ "$arch" == "i386" ] || [ "$arch" == "i486" ] || [ "$arch" == "i586" ] || [ "$arch" == "i686" ] ; then
	echo "Sorry, the viewer can only be built for 64 bits targets !"
	exit 1
fi

# Deal now with long --options that could be interpreted as/collide with short
# ones (this is due to our crude options parsing), and remove them from the
# command line when dealt with...
PROFILE=""
if has_option "-tracy" ; then
	PROFILE="-DTRACY:BOOL=TRUE"
	COMMAND_LINE=${COMMAND_LINE//--tracy/}
fi
STACK_OPTIONS=""
if has_option "-protect-stack" ; then
	STACK_OPTIONS="-DPROTECTSTACK:BOOL=TRUE"
	COMMAND_LINE=${COMMAND_LINE//--protect-stack/}
fi
if has_option "-no-exec-stack" ; then
	STACK_OPTIONS="$STACK_OPTIONS -DNOEXECSTACK:BOOL=TRUE"
	COMMAND_LINE=${COMMAND_LINE//--no-exec-stack/}
fi
UNITYBUILD=""
if has_option "-unity-build" ; then
	UNITYBUILD="-DUSEUNITYBUILD:BOOL=TRUE"
	COMMAND_LINE=${COMMAND_LINE//--unity-build/}
fi

BUILD_TYPE="Release"
PACKAGE_DIR="viewer-linux-$arch-release"
echo "==============================================================================="
if has_option "d" ; then
    BUILD_TYPE="Debug"
	PACKAGE_DIR="viewer-linux-$arch-debug"
elif has_option "s" ; then
    BUILD_TYPE="RelWithDebInfo"
	PACKAGE_DIR="viewer-linux-$arch-relwithdebinfo"
fi
echo "Building a $BUILD_TYPE viewer."
if [ "$arch" == "aarch64" ] ; then
	arch="arm64"
fi
echo "Building for architecture: $arch"

find_python
if [ "$PYTHON" != "" ] ; then
	ln -sf "$PYTHON" "$RUN_PATH/python"
	export PATH="$RUN_PATH:$PATH"
	echo "Forcing the use of python interpreter: $PYTHON"
else
	echo "ERROR: could not find a compatible Python interpreter. Please install Python version 3.9 to 3.3, 2.7 or 2.6."
	exit 1
fi

if has_option "v[16-9]" ; then
	if has_option "v6\.0" ; then
		find_gcc "6.0" 
	elif has_option "v6\.1" ; then
		find_gcc "6.1" 
	elif has_option "v6\.2" ; then
		find_gcc "6.2" 
	elif has_option "v6\.3" ; then
		find_gcc "6.3" 
	elif has_option "v6\.4" ; then
		find_gcc "6.4"
	elif has_option "v6\.5" ; then
		find_gcc "6.5"
	elif has_option "v7\.0" ; then
		find_gcc "7.0"
	elif has_option "v7\.1" ; then
		find_gcc "7.1" 
	elif has_option "v7\.2" ; then
		find_gcc "7.2" 
	elif has_option "v7\.3" ; then
		find_gcc "7.3" 
	elif has_option "v7\.4" ; then
		find_gcc "7.4"
	elif has_option "v7\.5" ; then
		find_gcc "7.5"
	elif has_option "v8\.0" ; then
		find_gcc "8.0"
	elif has_option "v8\.1" ; then
		find_gcc "8.1" 
	elif has_option "v8\.2" ; then
		find_gcc "8.2" 
	elif has_option "v8\.3" ; then
		find_gcc "8.3" 
	elif has_option "v8\.4" ; then
		find_gcc "8.4" 
	elif has_option "v8\.5" ; then
		find_gcc "8.5" 
	elif has_option "v9\.0" ; then
		find_gcc "9.0"
	elif has_option "v9\.1" ; then
		find_gcc "9.1" 
	elif has_option "v9\.2" ; then
		find_gcc "9.2" 
	elif has_option "v9\.3" ; then
		find_gcc "9.3" 
	elif has_option "v9\.4" ; then
		find_gcc "9.4" 
	elif has_option "v9\.5" ; then
		find_gcc "9.5" 
	elif has_option "v10\.0" ; then
		find_gcc "10.0" 
	elif has_option "v10\.1" ; then
		find_gcc "10.1" 
	elif has_option "v10\.2" ; then
		find_gcc "10.2" 
	elif has_option "v10\.3" ; then
		find_gcc "10.3" 
	elif has_option "v10\.4" ; then
		find_gcc "10.4" 
	elif has_option "v11\.0" ; then
		find_gcc "11.0" 
	elif has_option "v11\.1" ; then
		find_gcc "11.1" 
	elif has_option "v11\.2" ; then
		find_gcc "11.2" 
	elif has_option "v11\.3" ; then
		find_gcc "11.3" 
	elif has_option "v11\.4" ; then
		find_gcc "11.3" 
	elif has_option "v12\.0" ; then
		find_gcc "12.0" 
	elif has_option "v12\.1" ; then
		find_gcc "12.1" 
	elif has_option "v12\.2" ; then
		find_gcc "12.2" 
	elif has_option "v12\.3" ; then
		find_gcc "12.3" 
	elif has_option "v13\.0" ; then
		find_gcc "13.0" 
	elif has_option "v13\.1" ; then
		find_gcc "13.1" 
	elif has_option "v13\.2" ; then
		find_gcc "13.2" 
	fi
elif has_option "c" ; then
	if has_exec "clang" && has_exec "clang++" ; then
			clang_version1=`clang --version`
			clang_version2=`echo $clang_version1 | cut -d ' ' -f 3`
			echo "Will use clang/clang++ version $clang_version2 to build the viewer"
			GCC="clang"
			GXX="clang++"
	fi
fi
if [ "$GXX" != "" ] ; then
	GCC="-DCMAKE_C_COMPILER=$GCC"
	GXX="-DCMAKE_CXX_COMPILER=$GXX"
fi

NO_FATAL_WARNINGS=""
if has_option "i" ; then
    NO_FATAL_WARNINGS="-DNO_FATAL_WARNINGS:BOOL=TRUE"
	echo "The compiler warnings will not be treated as errors."
fi

TUNE_FLAGS=""
if has_option "t" ; then
	TUNE_FLAGS="-march=native"
	echo "Compiling with native system CPU optimizations."
fi

LTO=""
if has_option "l" ; then
	# Note: LINK_JOBS is used in indra/cmake/Linking.cmake to specify the
	# number of plugin jobs to use by ld, when using clang to compile the
	# viewer; we use as many jobs as we got virtual cores (failing to specify
	# it for clang would cause ld to use the number of physical cores for the
	# number of jobs, meaning SMT processors would be only half loaded).
    LTO="-DUSELTO:BOOL=TRUE -DLINK_JOBS=`grep processor /proc/cpuinfo | wc -l`"
	echo "Compiling with link-time optimizations (LTO)."
fi

OPENMP=""
if has_option "o" ; then
	OPENMP="-DOPENMP:BOOL=TRUE"
	echo "Compiling with OpenMP optimizations (WARNING: highly experimental)."
fi

if [ "$PROFILE" != "" ] ; then
	echo "Compiling with Tracy profiler support (relevant only to devel builds)."
fi
if has_option "g" ; then
	PROFILE="$PROFILE -DGPROF:BOOL=TRUE"
	echo "Compiling with gprof profiling (WARNING: slow viewer, for devel builds only)."
fi

GENERATOR=""
if has_option "n" && has_exec "ninja" ; then
	GENERATOR="-G Ninja"
	echo "Using ninja to build instead of make."
	# Note: ninja fails to notice the dependency with the pre-built library
	# downloads when looking for some static libraries... This might also be a
	# bug in the cmake generator for ninja. Here is a HACK to make ninja happy:
	LIBDIR="$RUN_PATH/lib/release"
	if ! [ -d "$LIBDIR" ] ; then
		mkdir -p "$LIBDIR"
		touch "$LIBDIR/libcef_dll_wrapper.a"
		touch "$LIBDIR/libdullahan.a"
		touch "$LIBDIR/libcef.so"
		touch "$LIBDIR/libminizip.a"
		touch "$LIBDIR/libxml2.a"
		touch "$LIBDIR/libpcrecpp.a"
		touch "$LIBDIR/libpcre.a"
		touch "$LIBDIR/liblua.a"
		touch "$LIBDIR/libz.a"
		touch "$LIBDIR/mimalloc.o"
	fi
fi

USESYSTEMLIBS=""
if has_option "u" ; then
	USESYSTEMLIBS="--systemlibs"
	echo "Using system libraries instead of pre-built ones when possible."
fi

PATCH_DIR="$HOME/.secondlife/patches/cool_vl_viewer"
if has_option "p" ; then
	if [ -d "$PATCH_DIR" ] && ! [ -f applied_patches.txt ] ; then
		PATCHES=`/bin/ls $PATCH_DIR/*.patch* 2>/dev/null`
		if [ "$PATCHES" != "" ] ; then
			echo "Applying patches from $PATCH_DIR:"
			for i in $PATCHES; do
				echo " - Patch: $i"
				if echo $i | grep ".gz" &>/dev/null ; then
					gzip -cd $i | patch -p1 -s
				elif echo $i | grep ".bz2" &>/dev/null ; then
					bzip2 -cd $i | patch -p1 -s
				elif echo $i | grep ".xz" &>/dev/null ; then
					xz -cd $i | patch -p1 -s
				else
					patch -p1 -s <$i
				fi
				echo "$i" >>applied_patches.txt
			done
		fi
	fi
fi

echo "==============================================================================="

start=`date +%s%2N`
if [ "$TUNE_FLAGS" != "" ] ; then
	./scripts/develop.py $USESYSTEMLIBS $GENERATOR -t $BUILD_TYPE configure $GCC $GXX $NO_FATAL_WARNINGS $STACK_OPTIONS $OPENMP $LTO $UNITYBUILD $PROFILE \
		-DCMAKE_C_FLAGS:STRING="$TUNE_FLAGS" \
		-DCMAKE_CXX_FLAGS:STRING="$TUNE_FLAGS" \
		-DCMAKE_CXX_FLAGS_RELEASE:STRING="$TUNE_FLAGS"
else
	./scripts/develop.py $USESYSTEMLIBS $GENERATOR -t $BUILD_TYPE configure $GCC $GXX $NO_FATAL_WARNINGS $STACK_OPTIONS $OPENMP $LTO $UNITYBUILD $PROFILE
fi
./scripts/develop.py $USESYSTEMLIBS $GENERATOR -t $BUILD_TYPE build
if (( $? != 0 )) ; then
	exit 1
fi

now=`date +%s%2N`
FINAL_DIR=`pwd`"/$PACKAGE_DIR/newview"
if [ -x "$FINAL_DIR/CoolVLViewer" ] ; then
	echo "==============================================================================="
	let delay=$now-$start
	let seconds=$delay/100
	let decimal=$delay-$seconds*100
	if (( $decimal < 10 )) ; then
		decimal="0$decimal"
	fi
	echo "Build successful. Total time taken by the building and packaging: $seconds.$decimal seconds."
	RUN_DIR=`find $FINAL_DIR -type d -name "CoolVLViewer-$arch*"`
	echo "Ready to run viewer available in : $RUN_DIR/"
	echo "Installation tarball available in: $FINAL_DIR/"
	echo "==============================================================================="
else
	exit 1
fi
