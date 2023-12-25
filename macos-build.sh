#!/bin/bash

export LANG=C

COMMAND_LINE="$*"

function has_option()
{
	echo "$COMMAND_LINE" | grep "\-$1" &>/dev/null
	return $?
}

if has_option "h" ; then
	echo "Usage: $0 [-option|--long-build-option...]|[-h|--help]"
	echo "With  -d, --debug   : build the Debug viewer binary instead of Release."
	echo "      -s, --symbols : build with debugging symbols (RelWithDebInfo viewer binary)."
	echo "      -h, --help    : this help..."
	exit 0
fi

export PATH="/Applications/CMake.app/Contents/bin:$PATH"

if ! which cmake &>/dev/null; then
	echo
	echo "You need to have 'cmake' installed and in your PATH !".
	exit 1
fi

top=`pwd`

if ! [ -x "./scripts/develop.py" ] ; then
	echo
	echo "This script must be ran from the linden/ Cool VL Viewer sources directory !".
	exit 1
fi

BUILD_TYPE="Release"
if has_option "d" ; then
    BUILD_TYPE="Debug"
elif has_option "s" ; then
    BUILD_TYPE="RelWithDebInfo"
fi

./scripts/develop.py -t $BUILD_TYPE configure -DNO_FATAL_WARNINGS:BOOL=TRUE

if ! [ -d "$top/build-darwin-x86_64/CoolVLViewer.xcodeproj" ] ; then
	echo
	echo "Failed to generate the project files, sorry !"
	exit 1
fi

# Create a dummy (empty) directory to prevent warnings at link time since cmake
# apparently adds a spurious "Release" sub-directory search path for libraries...
mkdir -p "$top/lib/release/Release"
# Same thing for "RelWithDebInfo" and "Debug" build types, in case you would
# need them instead of "Release".
mkdir -p "$top/lib/release/RelWithDebInfo" "$top/lib/debug/RelWithDebInfo"
mkdir -p "$top/lib/release/Debug" "$top/lib/debug/Debug"

pushd build-darwin-x86_64
xcodebuild -project CoolVLViewer.xcodeproj -target ALL_BUILD
popd

if [ -d "build-darwin-x86_64/Cool VL Viewer.app" ] ; then
	echo "Packaged viewer available in $top/build-darwin-x86_64/Cool VL Viewer.app"
else
	echo "Build failed... :-("
fi
