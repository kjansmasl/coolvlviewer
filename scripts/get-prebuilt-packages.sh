#!/bin/bash

# get-prebuilt-packages.sh v1.00 (c)2016 Henri Beauchamp.
# Released under the GPL (v2 or later, at your convenience) License:
# http://www.gnu.org/copyleft/gpl.html

script=`readlink -f "$0" || echo "$0"`
cd `dirname "$script" || echo .`
cd ..

if echo "$*" | grep "\-h" &>/dev/null ; then
	echo "This script allows to download the pre-built library packages used to build"
	echo "the Cool VL Viewer, thus ensuring you will be able to build it even after"
	echo "the packages would be pulled off from the corresponding sites (simply place"
	echo "the downloaded packages inside the temporary directory where the build system"
	echo "normally stores them)."
	echo "Usage: $0 [-s]|[-f]|[-c]|[-l]|[-m]|[-w][-t]|[-h|--help]]"
	echo "With: -s         : skip packages from Second Life site (amazonaws.com)."
	echo "      -c         : skip packages from Cool VL Viewer site (sldev.free.fr)."
	echo "      -l         : skip Linux packages."
	echo "      -m         : skip MacOS-X packages."
	echo "      -w         : skip Windows packages."
	echo "      -t         : downloads go into /var/tmp$HOME/install.cache/"
	echo "      -h, --help : this help..."
	echo "Unless the -t option is specified, the packages will be downloaded into a"
	echo "'prebuilt-packages' directory that will be created (if not already present)"
	echo "inside the sources tree."
	exit 0
fi

if ! [ -f install.xml ] ; then
	echo "This script shall be ran from the sources tree of the Cool VL Viewer !"
	exit 1
fi

if ! which curl &>/dev/null ; then
	echo "Could not find curl !"
	exit 1
fi

NOGREP1="xxxxx"
NOGREP2="xxxxx"
NOGREP3="xxxxx"
NOGREP4="xxxxx"
NOGREP5="xxxxx"
if echo "$*" | grep "\-s" &>/dev/null ; then
	NOGREP1="amazonaws\.com"
fi
if echo "$*" | grep "\-c" &>/dev/null ; then
	NOGREP2="sldev\.free\.fr"
fi
if echo "$*" | grep "\-l" &>/dev/null ; then
	NOGREP3="\-linux"
fi
if echo "$*" | grep "\-m" &>/dev/null ; then
	NOGREP4="\-darwin"
fi
if echo "$*" | grep "\-w" &>/dev/null ; then
	NOGREP5="\-win"
fi

files=`grep '\<uri\>' install.xml | grep -v "$NOGREP1" | grep -v "$NOGREP2" | grep -v "$NOGREP3" | grep -vi "$NOGREP4" | grep -vi "$NOGREP5" | sort | uniq | sed -e 's/ //g' -e 's/<uri>//' -e 's:</uri>::'`
if [ "$files" == "" ] ; then
	echo "Nothing to download..."
	exit 0
fi

DEST="prebuilt-packages"
if echo "$*" | grep "\-t" &>/dev/null ; then
	DEST="/var/tmp$HOME/install.cache"
fi
mkdir -p $DEST
if ! [ -d "$DEST" ] ; then
	echo "Cannot create directory $DEST !"
	exit 1
fi
cd $DEST

for i in $files ; do
	echo "-------------------------------------------------------------------------------"
	echo "Downloading: $i"
	curl -O "$i"
	if [ "$?" != "0" ] ; then
		echo "Failed to download: $i"
	fi
done

echo "-------------------------------------------------------------------------------"
echo "Done !"
