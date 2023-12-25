#!/bin/bash

# update-version.sh v2.00 (c)2016-2023 Henri Beauchamp.
# Released under the GPL (v2 or later, at your convenience) License:
# http://www.gnu.org/copyleft/gpl.html

# Stop on error
set -e

script=`readlink -f "$0" || echo "$0"`
cd `dirname "$script" || echo .`
cd ..

if echo "$*" | grep "\-h" &>/dev/null ; then
	echo "This script syncs or increments the version numbers for the Cool VL Viewer"
	echo "sources. The numbers are held inside the scripts/VIEWER_VERSION file."
	echo "Usage: $0 [[-r|--release]|[-b|--branch]|[-m|--minor]|[-M|--major]|[-h|--help]]"
	echo "With: -r | --release : increment the release number."
	echo "      -b | --branch  : increment the branch number"
	echo "                       (also zeroes the release number)."
	echo "      -m | --minor   : increment the minor version number"
	echo "                       (also zeroes the release and branch numbers)."
	echo "      -M | --Major   : increment the major version number"
	echo "                       (also zeroes all other numbers)."
	echo "      -h, --help     : this help..."
	echo "If no option is passed, then the script will just sync the viewer version"
	echo "with the numbers held inside the scripts/VIEWER_VERSION file."
	exit 0
fi

if ! [ -f scripts/VIEWER_VERSION ] ; then
	echo "This script shall be ran from the sources tree of the Cool VL Viewer !"
	exit 1
fi
. scripts/VIEWER_VERSION
version_old_major=$LL_VERSION_MAJOR
version_old_minor=$LL_VERSION_MINOR
version_old_branch=$LL_VERSION_BRANCH

was_experimental="no"
if  [ $(( $version_old_minor % 2 )) -eq 1 ] || [ $(( $version_old_branch % 2 )) -eq 1 ] ; then
	was_experimental="yes"
fi
if [ -f scripts/FORMER_VERSIONS ] ; then
	. scripts/FORMER_VERSIONS
elif [ "$was_experimental" == "yes" ] ; then
	FORMER_EXPERIMENTAL_MAJOR=$version_old_major
	FORMER_EXPERIMENTAL_MINOR=$version_old_minor
	FORMER_EXPERIMENTAL_BRANCH=$version_old_branch
	FORMER_STABLE_MAJOR=x
	FORMER_STABLE_MINOR=y
	FORMER_STABLE_BRANCH=z
else
	FORMER_STABLE_MAJOR=$version_old_major
	FORMER_STABLE_MINOR=$version_old_minor
	FORMER_STABLE_BRANCH=$version_old_branch
	FORMER_EXPERIMENTAL_MAJOR=x
	FORMER_EXPERIMENTAL_MINOR=y
	FORMER_EXPERIMENTAL_BRANCH=z
fi

if echo "$*" | grep "\-M" &>/dev/null ; then
	let LL_VERSION_MAJOR=$LL_VERSION_MAJOR+1
	LL_VERSION_MINOR=0
	LL_VERSION_BRANCH=0
	LL_VERSION_RELEASE=0
	if [ "$was_experimental" == "yes" ] ; then
		FORMER_EXPERIMENTAL_MAJOR=$version_old_major
		FORMER_EXPERIMENTAL_MINOR=$version_old_minor
		FORMER_EXPERIMENTAL_BRANCH=$version_old_branch
	else
		FORMER_STABLE_MAJOR=$version_old_major
		FORMER_STABLE_MINOR=$version_old_minor
		FORMER_STABLE_BRANCH=$version_old_branch
		FORMER_EXPERIMENTAL_MAJOR=x
		FORMER_EXPERIMENTAL_MINOR=y
		FORMER_EXPERIMENTAL_BRANCH=z
	fi
elif echo "$*" | grep "\-m" &>/dev/null ; then
	let LL_VERSION_MINOR=$LL_VERSION_MINOR+1
	LL_VERSION_BRANCH=0
	LL_VERSION_RELEASE=0
	if [ "$was_experimental" == "yes" ] ; then
		FORMER_EXPERIMENTAL_MAJOR=$version_old_major
		FORMER_EXPERIMENTAL_MINOR=$version_old_minor
		FORMER_EXPERIMENTAL_BRANCH=$version_old_branch
	else
		FORMER_STABLE_MAJOR=$version_old_major
		FORMER_STABLE_MINOR=$version_old_minor
		FORMER_STABLE_BRANCH=$version_old_branch
		FORMER_EXPERIMENTAL_MAJOR=x
		FORMER_EXPERIMENTAL_MINOR=y
		FORMER_EXPERIMENTAL_BRANCH=z
	fi
elif echo "$*" | grep "\-b" &>/dev/null ; then
	let LL_VERSION_BRANCH=$LL_VERSION_BRANCH+1
	LL_VERSION_RELEASE=0
	if [ "$was_experimental" == "yes" ] ; then
		FORMER_EXPERIMENTAL_MAJOR=$version_old_major
		FORMER_EXPERIMENTAL_MINOR=$version_old_minor
		FORMER_EXPERIMENTAL_BRANCH=$version_old_branch
	else
		FORMER_STABLE_MAJOR=$version_old_major
		FORMER_STABLE_MINOR=$version_old_minor
		FORMER_STABLE_BRANCH=$version_old_branch
		FORMER_EXPERIMENTAL_MAJOR=x
		FORMER_EXPERIMENTAL_MINOR=y
		FORMER_EXPERIMENTAL_BRANCH=z
	fi
elif echo "$*" | grep "\-r" &>/dev/null ; then
	let LL_VERSION_RELEASE=$LL_VERSION_RELEASE+1
fi

cat <<EOF1 >scripts/VIEWER_VERSION
LL_VERSION_MAJOR=$LL_VERSION_MAJOR
LL_VERSION_MINOR=$LL_VERSION_MINOR
LL_VERSION_BRANCH=$LL_VERSION_BRANCH
LL_VERSION_RELEASE=$LL_VERSION_RELEASE
EOF1

cat <<EOF2 >scripts/FORMER_VERSIONS
FORMER_EXPERIMENTAL_MAJOR=$FORMER_EXPERIMENTAL_MAJOR
FORMER_EXPERIMENTAL_MINOR=$FORMER_EXPERIMENTAL_MINOR
FORMER_EXPERIMENTAL_BRANCH=$FORMER_EXPERIMENTAL_BRANCH
FORMER_STABLE_MAJOR=$FORMER_STABLE_MAJOR
FORMER_STABLE_MINOR=$FORMER_STABLE_MINOR
FORMER_STABLE_BRANCH=$FORMER_STABLE_BRANCH
EOF2

echo "New viewer version is: $LL_VERSION_MAJOR.$LL_VERSION_MINOR.$LL_VERSION_BRANCH.$LL_VERSION_RELEASE"
if [ "$FORMER_EXPERIMENTAL_MAJOR" != "x" ] ; then
	echo "Former experimental branch was: $FORMER_EXPERIMENTAL_MAJOR.$FORMER_EXPERIMENTAL_MINOR.$FORMER_EXPERIMENTAL_BRANCH"
fi
if [ "$FORMER_STABLE_MAJOR" != "x" ] ; then
	echo "Former stable branch was: $FORMER_STABLE_MAJOR.$FORMER_STABLE_MINOR.$FORMER_STABLE_BRANCH"
fi

year=`date +%Y`

find . -type f -name "*.in" | while read i ; do
	if grep "viewer_version_" "$i" &>/dev/null ; then
		echo "Processing file $i into ${i%.in}..."
		sed -e "s/viewer_version_major/$LL_VERSION_MAJOR/" \
			-e "s/viewer_version_minor/$LL_VERSION_MINOR/" \
			-e "s/viewer_version_branch/$LL_VERSION_BRANCH/" \
			-e "s/viewer_version_release/$LL_VERSION_RELEASE/" \
			-e "s/previous_experimental_major/$FORMER_EXPERIMENTAL_MAJOR/" \
			-e "s/previous_experimental_minor/$FORMER_EXPERIMENTAL_MINOR/" \
			-e "s/previous_experimental_branch/$FORMER_EXPERIMENTAL_BRANCH/" \
			-e "s/previous_stable_major/$FORMER_STABLE_MAJOR/" \
			-e "s/previous_stable_minor/$FORMER_STABLE_MINOR/" \
			-e "s/previous_stable_branch/$FORMER_STABLE_BRANCH/" \
			-e "s/viewer_copyright_year/$year/" \
			"$i" >"${i%.in}"
	fi
done

if echo "$*" | grep "\-r" &>/dev/null ; then
	echo "Removing unchanged *.mpi (InstallJammer) files:"
	find . -type f -name "Cool VL Viewer.mpi" -print -exec rm -f "{}" \;
	find . -type f -name "Cool VL Viewer-x86_64.mpi" -print -exec rm -f "{}" \;
else
	echo "Removing deprecated *.mpi (InstallJammer) files:"
	find . -type f -name "Cool VL Viewer-$version_old_major.*.mpi" -print -exec rm -f "{}" \;
	find . -type f -name "Cool VL Viewer-x86_64-$version_old_major.*.mpi" -print -exec rm -f "{}" \;
	new_suffix="-$LL_VERSION_MAJOR.$LL_VERSION_MINOR.$LL_VERSION_BRANCH.mpi"
	find . -type f -name "Cool VL Viewer*.mpi" | while read i ; do
		echo "Renaming $i into ${i%.mpi}${new_suffix}"
		mv -f "$i" "${i%.mpi}${new_suffix}"
	done
fi

echo "Done."
