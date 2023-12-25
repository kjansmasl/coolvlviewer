#!/bin/bash

if ! which ctags &>/dev/null ; then
	echo "Could not find the 'ctags' command !"
	exit 1
fi

if ! [ -f indra/CMakeLists.txt ] ; then
	echo "This script shall be ran from the sources tree of the Cool VL Viewer !"
	exit 1
fi

source_dirs=`grep add_subdirectory indra/CMakeLists.txt | grep -v cmake | sed -e 's:add_subdirectory(:indra/:' -e 's/)//'`
echo "Generating the 'tags' file..."
ctags --languages=C,C++ --recurse $source_dirs
ret=$?
if (( $ret == 0 )) ; then
	echo "Done."
else
	echo "Failed."
	exit $ret
fi
