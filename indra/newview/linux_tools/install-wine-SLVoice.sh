#!/bin/bash

# install-wine-SLVoice.sh v1.41 (c)2020-2023 Henri Beauchamp.
# Released under the GPL license. https://www.gnu.org/licenses/gpl-3.0.txt

WIN32_SLVOICE="http://automated-builds-secondlife-com.s3.amazonaws.com/ct2/55968/524423/slvoice-4.10.0000.32327.5fc3fe7c.539691-windows-539691.tar.bz2"
WIN64_SLVOICE="http://automated-builds-secondlife-com.s3.amazonaws.com/ct2/55967/524409/slvoice-4.10.0000.32327.5fc3fe7c.539691-windows64-539691.tar.bz2"

# This is our custom Wine prefix for SLVoice
export WINEPREFIX="$HOME/.wine-slvoice"
# This is where the SLVoice client will be installed
WINE_INSTALL_DIR="$WINEPREFIX/drive_c/Vivox"
# Prevents Wine from asking to install Mono and Gecko...
export WINEDLLOVERRIDES="mscoree,mshtml="
# No verbose debugging, please !
export WINEDEBUG="-all"

# If the ~/.wine-slvoice prefix was already installed and this script is passed
# an option, then run any corresponding Wine program on that prefix. We support
# any option containing "-c" for winecfg or "-r" for regedit.
if [ -f "$WINEPREFIX/system.reg" ] ; then
	if echo "$1" | grep "\-c" &>/dev/null ; then
		exec winecfg
	elif echo "$1" | grep "\-r" &>/dev/null ; then
		exec regedit
	fi
fi

# If we are not ran from a terminal, try and find a terminal on this system,
# then launch it and restart ourselves within it.
if ! [ -t 1 ] ; then
	cmd="-e"
	terminal=""
	if which xterm &>/dev/null ; then
		terminal="xterm"
	elif which rxvt &>/dev/null ; then
		terminal="rxvt"
	elif which mate-terminal &>/dev/null ; then
		terminal="mate-terminal"
	elif which gnome-terminal &>/dev/null ; then
		terminal="gnome-terminal"
	elif which kde-terminal &>/dev/null ; then
		terminal="kde-terminal"
	elif which xfce4-terminal &>/dev/null ; then
		terminal="xfce4-terminal"
	elif which qterminal &>/dev/null ; then
		terminal="qterminal"
	elif which Eterm &>/dev/null ; then
		terminal="Eterm"
	elif which x-terminal-emulator &>/dev/null ; then
		terminal="x-terminal-emulator"
	elif which xdg-terminal &>/dev/null ; then
		# NOTE: xdg-terminal may fail (due to a bug in failing to unquote
		# gsettings returned strings)...
		terminal="xdg-terminal"
		cmd=""
	else
		echo "This script must be ran from a terminal !"
		exit 1
	fi
	exec $terminal $cmd $0
fi

# Use colors, if stdout is emitted onto a terminal supporting them
white_on_blue=""
black_on_yellow=""
black_on_green=""
white_on_red=""
white_on_black=""
default_colors=""
colors=`tput colors`
if [ "$colors" != "" ] && (( $colors >= 8 )) ; then
	white_on_blue=$'\e[48;2;0;0;160m\e[38;2;255;255;255m'
	black_on_yellow=$'\e[48;2;255;255;0m\e[38;2;0;0;0m'
	black_on_green=$'\e[48;2;0;255;0m\e[38;2;0;0;0m'
	white_on_red=$'\e[48;2;255;0;0m\e[38;2;255;255;255m'
	white_on_black=$'\e[48;2;0;0;0m\e[38;2;255;255;255m'
	default_colors=$'\033[0m'
fi

function pause()
{
	echo
	read -s -n 1 -p "${white_on_blue} $1 ${default_colors}"
	echo
}

function success()
{
	echo "${black_on_green} ==> $1 ${default_colors}"
}

function warning()
{
	echo "${black_on_yellow} WARNING: $1 ${default_colors}"
}

function error()
{
	echo "${white_on_red} ERROR: $1 ${default_colors}"
}

echo
echo "==============================================================================="
echo "                            install-wine-SLVoice.sh                            "
echo "==============================================================================="
echo
echo "This script will (try and) (re)install a \"Wine prefix\" for SLVoice.exe, i.e."
echo "a specific Wine installation only for the purpose of running SLVoice.exe from"
echo "the Linux version of the Cool VL Viewer. This is a per-account installation (in"
echo "your home directory), not a system-wide one."
echo "Any other Wine prefixes (such as the default one residing in ~/.wine) will be"
echo "left untouched and have no impact whatsoever on ours."
echo
echo "Once installed, the SLVoice.exe client will be usable with any forthcoming"
echo "version of the Cool VL Viewer and won't need to be reinstalled whenever you"
echo "udpate the viewer."
echo
echo "SLVoice.exe will be installed in: ${white_on_black}$WINE_INSTALL_DIR${default_colors}"
echo
echo "The script will also verify that your system can run Windows binaries via"
echo "Wine and the binfmt_misc mechanism of the Linux kernel or a wrapper script."
echo
echo "Finally, it will configure the Cool VL Viewer to run the installed Windows"
echo "voice client (via appropriate variables in ~/.secondlife/cool_vl_viewer.conf)."
pause "Press any key to continue (or CTRL C to abort)."

scriptsrc=`readlink -f "$0" || echo "$0"`
run_path=`dirname "$scriptsrc" || echo .`
if [ ! -f "$run_path/app_settings/ca-bundle.crt" ] ; then
	echo
	error "could not find app_settings/ca-bundle.crt !"
	echo "This script must be placed in and ran from the viewer installation directory !"
	pause "Press any key to exit."
	exit 1
fi

echo
echo "Checking for Wine availability..."
if ! which winecfg &>/dev/null ; then
	echo
	error "No Wine installation found ('winecfg' not found) !"
	echo
	echo "Please make sure to install Wine (either 32 or 64 bits) on your system,"
	echo "after which step you can re-run this script..."
	pause "Press any key to exit."
	exit 1
fi

echo
echo "Checking for Wine type (32 or 64 bits)..."
wine_binary=""
export WINEARCH="win32"
wine_binary=`which wine64 2>/dev/null`
if (( $? == 0 )) ; then
	export WINEARCH="win64"
else
	wine_binary=`which wine 2>/dev/null`
	if (( $? == 0 )) ; then
		if file $wine_binary | grep "64-bit" &>/dev/null ; then
			export WINEARCH="win64"
		fi
	else
		wine_binary=`which wine32 2>/dev/null`
	fi
fi
if [ "$wine_binary" == "" ] ; then
	echo
	warning "no 'wine[32|64]' executable found; binfmt_misc will be required."
else
	echo "Found wine binary: $wine_binary"
fi
echo
echo "Will use the $WINEARCH architecture for our Wine prefix."

# Allow forcing the use of a wrapper script to launch SLVoice.exe, when a "-w"
# option is passed to this script. NOTE: the wrapper can only be created if
# either of "wine64", "wine" or "wine32" Wine executables are present on the
# system.
needs_wrapper=0
if echo "$1" | grep "\-w" &>/dev/null ; then
	needs_wrapper=1
fi

echo
echo "Checking that binfmt_misc exists and is configured to allow the"
echo "execution of Windows binaries..."
has_binfmt_misc=0
binfmt_misc_enabled=0
if [ -d /proc/sys/fs/binfmt_misc ] ; then
	for i in /proc/sys/fs/binfmt_misc/* ; do
		if [ "$i" != "status" ] && [ "$i" != "register" ]  && (( $binfmt_misc_enabled == 0 )) ; then
			if grep -m 1 "magic 4d5a" "$i" &>/dev/null ; then
				has_binfmt_misc=1
				temp=`head -1 "$i" 2>/dev/null`
				if [ "$temp" == "enabled" ] ; then
					binfmt_misc_enabled=1
					temp=`grep -m 1 interpreter "$i" 2>/dev/null | cut -d ' ' -f 2`
					if [ "$temp" == "" ] || [ ! -x "$temp" ] ; then
						binfmt_misc_enabled=0
						echo
						warning "found a Wine entry ($i) but not its '$temp' interpreter."
						echo "You might need to create (as root) a link to the actual Wine executable, e.g."
						echo "ln -s /usr/bin/wine64 $temp"
					fi
				else
					echo
					warning "found a Wine entry ($i) but it is disabled."
				fi
			fi
		fi
	done
	if (( $binfmt_misc_enabled == 1 )) ; then
		echo
		success "Good !  Wine is registered in binfmt_misc."
	elif [ "$wine_binary" == "" ] ; then
		warning "you will need to enable the existing Wine entry."
	else
		warning "no enabled entry found to launch Wine via binfmt_misc."
		echo "${white_on_black}A wrapper script will be created to call $wine_binary${default_colors}"
		echo "${white_on_black}and launch the voice client...${default_colors}"
		needs_wrapper=1
	fi
elif [ "$wine_binary" == "" ] ; then
	echo
	error "binfmt_misc is not available on this system !"
	echo "The viewer would not be able to launch directly a Windows binary"
	echo "and no 'wine' executable was found on this system either..."
	echo
	echo "Please, enable binfmt_misc (review your Linux distribution FAQ) and"
	echo "once enabled, re-run this script."
	pause "Press any key to exit."
	exit 1
else
	needs_wrapper=1
	echo
	warning "binfmt_misc is not available on this system."
	echo "${white_on_black}A wrapper script will be created to call $wine_binary${default_colors}"
	echo "${white_on_black}and launch the voice client...${default_colors}"
fi
if [ "$needs_wrapper" == "0" ]; then
	if [ "$has_binfmt_misc" == "0" ]; then
		echo
		error "binfmt_misc not configured to run Windows binaries on this system."
		echo "The viewer would not be able to launch the Windows voice client."
		echo
		echo "Please, enable binfmt_misc for Wine. Example of valid enabling (as root):"
		echo "echo \":windows:M::MZ::/usr/bin/wine:\" >/proc/sys/fs/binfmt_misc/register"
		echo "(supposing the Wine binary is indeed /usr/bin/wine)"
		echo "Once this is done, you can re-run this script."
		pause "Press any key to exit."
		exit 1
	elif [ "$binfmt_misc_enabled" == "1" ] ; then
		binfmt_misc_enabled=`cat /proc/sys/fs/binfmt_misc/status`
		if [ "$binfmt_misc_enabled" != "enabled" ] ; then
			echo
			warning "binfmt_misc is currently disabled, system wide."
			if [ "$wine_binary" == "" ] ; then
				echo "You will have to enable it so that the viewer can launch the Windows"
				echo "SLVoice client..."
			else
				needs_wrapper=1
				echo "${white_on_black}A wrapper script will be created to call $wine_binary${default_colors}"
				echo "${white_on_black}and launch the voice client...${default_colors}"
			fi
		fi
	fi
fi

if [ -d "$WINEPREFIX" ] ; then
	echo
	echo "Removing the old SLVoice Wine installation..."
	rm -rf "$WINEPREFIX"
fi

echo
echo "We are now going to create a brand new Wine prefix... Launching winecfg."
echo
echo "${white_on_blue} Review the Audio settings and close the window with 'OK' when done... ${default_colors}"
winecfg &>/dev/null
# As long as Wine services are running, system.reg may appear missing while it
# is there. Probably some weird file locking issue...
if [ ! -f "$WINEPREFIX/system.reg" ] ; then
	echo
	echo "${white_on_black}Waiting for Wine to shut down...${default_colors}"
	let delay=20
	while [ ! -f "$WINEPREFIX/system.reg" ]  && (( $delay > 0 )) ; do
		let delay=$delay-1
		sleep 1
	done
fi
if [ ! -f "$WINEPREFIX/system.reg" ] ; then
	echo
	error "Wine prefix creation failed !  Aborting."
	pause "Press any key to exit."
	exit 1
fi

echo
slvoice_package=""
wine_arch=`cat $WINEPREFIX/system.reg | grep -m 1 '#arch' | cut -d '=' -f2`
if [ "$wine_arch" == "win32" ] ; then
	echo "Win32 prefix detected. Installing the 32 bits SLVoice version..."
	slvoice_package="$WIN32_SLVOICE"
else
	echo "Win64 prefix detected. Installing the 64 bits SLVoice version..."
	slvoice_package="$WIN64_SLVOICE"
fi

mkdir -p "$WINE_INSTALL_DIR"
pushd "$WINE_INSTALL_DIR" &>/dev/null
echo
echo "Downloading the corresponding package..."
echo
if which wget &>/dev/null ; then
	wget "$slvoice_package"
elif which curl &>/dev/null ; then
	curl -O "$slvoice_package"
else
	error "cannot download the file !"
	echo "Please install either wget or curl and re-run this script..."
	pause "Press any key to exit."
	exit 1
fi
echo
echo "Extracting files..."
tar xjf *.tar.bz2
rm -f *.tar.bz2 *.xml
mv -f bin/release/SLVoice.exe .
chmod +x SLVoice.exe
mv -f lib/release/*.dll .
# Note: dbghelp.dll is already part of/adapted to Wine, so no need for the one
# bundled with Vivox.
rm -f DbgHelp.dll
rm -rf bin/ lib/
echo
echo "Copying the certificates bundle..."
cp -a $run_path/app_settings/ca-bundle.crt .
exec="SLVoice.exe"
if [ "$needs_wrapper" == "1" ]; then
	echo
	echo "Creating a wrapper script..."
	exec="slvoice-wine-wrapper.sh"
	echo "#!/bin/bash" >$exec
	echo "exec $wine_binary $WINE_INSTALL_DIR/SLVoice.exe \$*" >>$exec
	chmod +x $exec
fi
echo
success "SLVoice installation done."
popd &>/dev/null

# Default path for SL viewers application directory:
sl_app_dir="$HOME/.secondlife"
if [ "$SECONDLIFE_USER_DIR" != "" ] ; then
	sl_app_dir="$SECONDLIFE_USER_DIR"
	echo
	echo "Adopting user-set SL viewers application directory: ${white_on_black}$sl_app_dir${default_colors}"
fi

echo
mkdir -p "$sl_app_dir"
conf_file="$sl_app_dir/cool_vl_viewer.conf"
if [ -f "$conf_file" ] ; then
	echo "Updating your ${white_on_black}$conf_file${default_colors} file..."
	grep -v WINEPREFIX "$conf_file" >"$conf_file.$$"
	grep -v WINEDLLOVERRIDES "$conf_file.$$" >"$conf_file"
	grep -v WINEDEBUG "$conf_file" >"$conf_file.$$"
	grep -v LL_WINE_SLVOICE "$conf_file.$$" >"$conf_file"
	rm -f "$conf_file.$$"
else
	echo "Creating a ${white_on_black}$conf_file${default_colors} file..."
fi
echo "export WINEPREFIX=\"$WINEPREFIX\"" >>"$conf_file"
echo "export WINEDLLOVERRIDES=\"mscoree,mshtml=\"" >>"$conf_file"
echo "export WINEDEBUG=\"-all\"" >>"$conf_file"
echo "export LL_WINE_SLVOICE=\"$WINE_INSTALL_DIR/$exec\"" >>"$conf_file"
echo
success "Viewer configuration done."

pause "All done !  Press any key to exit."
