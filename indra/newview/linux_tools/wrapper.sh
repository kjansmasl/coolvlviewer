#!/bin/bash

# Cool VL Viewer wrapper script v3.60 (c)2007-2023 Henri Beauchamp.
# Released under the GPL license (https://www.gnu.org/licenses/gpl.html).
#
# Any options passed to this script on the command line get transmitted to the
# viewer, with the exception of the following options, that must appear *first*
# in the command line:
#  --check-libs : verifies the system dependencies (libraries) for running the
#                 viewer and its components (this option does not launch the
#                 viewer, instead exiting this script when done).
#  --debug      : launches gdb (if found), to debug the viewer in it. (*)
#  --edb        : launches edb (if found), to debug the viewer in it.
#  --lldb       : launches lldb (if found), to debug the viewer in it. (*)
# (*) When present/found, the CoolVLViewer.debug symbols file is also loaded
#     automatically into the debugger.

# Determine what directory we are running from
scriptsrc=`readlink -f "$0" || echo "$0"`
run_path=`dirname "$scriptsrc" || echo .`
echo "Running from $run_path"
cd "$run_path"

# SAVED_LD_PRELOAD and SAVED_LD_LIBRARY_PATH are used by our wrapper scripts
# (currently launch_url.sh and messagebox.sh), themselves launched by the
# viewer, and thus inheriting its environments variables, to restore the
# original LD_PRELOAD and LD_LIBRARY_PATH values before launching third party
# so to avoid polluting them with libraries such as libjemalloc.so (they may be
# incompatible with it) or libcef.so (which is badly linked and re-exports
# functions from static libraries it got linked with, such as libnssutil.a).
export SAVED_LD_PRELOAD="$LD_PRELOAD"
export SAVED_LD_LIBRARY_PATH="$LD_LIBRARY_PATH"

# Source the installation default configuration file
echo "Sourcing $run_path/cool_vl_viewer.conf..."
source $run_path/cool_vl_viewer.conf

# Source any system-wide configuration file when it exists
if [ -f /etc/cool_vl_viewer.conf ] ; then
	echo "Sourcing /etc/cool_vl_viewer.conf..."
	source /etc/cool_vl_viewer.conf
fi

# Default user application directory:
user_app_dir="$HOME/.secondlife"
# Did the user setup a different user application directory ?
if [ "$SECONDLIFE_USER_DIR" != "" ] ; then
	if [ -d "$SECONDLIFE_USER_DIR" ] ; then
		# If it is a valid directory, adopt it
		user_app_dir="$SECONDLIFE_USER_DIR"
	else
		mkdir -p "$SECONDLIFE_USER_DIR" &>/dev/null
		if (( $? != 0 )) ; then
			echo "WARNING: invalid user application directory: $SECONDLIFE_USER_DIR"
			echo "Resetting to default: $user_app_dir"
			unset SECONDLIFE_USER_DIR
		else
			# If it is a valid directory, adopt it
			user_app_dir="$SECONDLIFE_USER_DIR"
		fi
	fi
fi

# If the user did setup a configuration include, then source it now.
if [ -f "$user_app_dir/cool_vl_viewer.conf" ] ; then
	echo "Sourcing $user_app_dir/cool_vl_viewer.conf..."
	source "$user_app_dir/cool_vl_viewer.conf"
fi

# This will be used later on...
logs_dir="$user_app_dir/logs"

# Count the number of CPU cores (including virtual ones when SMT is used)
ncores=`cat /proc/cpuinfo | grep processor | wc -l`

# Adjust exported environment variables set to "auto"

# Mesa threading
if [ "$mesa_glthread" == "auto" ] ; then
	if (( $ncores == 1 )) ; then
		export mesa_glthread=false
	else
		export mesa_glthread=true
	fi
fi

# NVIDIA threading
if [ "$__GL_THREADED_OPTIMIZATIONS" == "auto" ] ; then
	if (( $ncores == 1 )) ; then
		export __GL_THREADED_OPTIMIZATIONS=0
	else
		export __GL_THREADED_OPTIMIZATIONS=1
	fi
fi

# Adjust X11/OpenGL settings based on some exported variables
if [ "$LL_AUTO_DISABLE_MONITOR_SYNC" == "1" ] ; then
	# AMD Freesync disabling when vblank_mode is set to 0
	if (( $vblank_mode == 0 )) ; then
		echo "Forcing FreeSync off..."
		video_ports=`xrandr | grep ' connected ' | cut -d ' ' -f 1`
		for p in $video_ports; do
			xrandr --output "$p" --set "freesync" 0 &>/dev/null
		done
	fi
	# NVIDIA G-sync disabling when __GL_SYNC_TO_VBLANK is set to 0
	if (( $__GL_SYNC_TO_VBLANK == 0 )) ; then
		if which nvidia-settings &>/dev/null ; then
			echo "Forcing G-Sync off..."
			nvidia-settings -a AllowGSYNC=0 -a AllowVRR=0 &>/dev/null
		fi
	fi
fi

## Only the X11 SDL driver is supported by the viewer code: in case of a viewer
## binary compiled against system libraries, this export guarantees that any
## system SDL library v2.0.22 or newer will not attempt Wayland and fail...
export SDL_VIDEODRIVER="x11"

## - The 'scim' GTK IM module widely crashes the viewer. Avoid it.
if [ "$GTK_IM_MODULE" == "scim" ]; then
    export GTK_IM_MODULE=xim
fi

# Unset any proxy environment variables (the viewer got its own proxy settings)
unset http_proxy https_proxy no_proxy HTTP_PROXY HTTPS_PROXY

viewer_binary='./bin/cool_vl_viewer-bin'
export LD_LIBRARY_PATH="$run_path/lib:$LD_LIBRARY_PATH"

if [ "$1" == "--check-libs" ] ; then
	# If we are not ran from a terminal, try and find a terminal on this
	# system, then launch it and restart ourselves within it.
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
		exec $terminal $cmd $scriptsrc --check-libs
	fi

	export LC_ALL="C"
	export LANG="C"
	export LANGUAGE="C"
	echo
	echo "Checking for missing system libraries for the viewer:"
	echo "  - dependencies for $viewer_binary..."
	cando="yes"
	if ldd $viewer_binary 2>&1 | grep 'not ' &>/dev/null ; then
		ldd $viewer_binary 2>&1 | grep 'not ' | sort | uniq
		cando="no"
	fi
	echo "Checking for missing system libraries for the viewer libraries:"
	files=`find ./lib -type f -name "*.so*" -print | sort`
	libs32=""
	for f in $files ; do
		# Gives 01 for 32 bits ELF binaries, 02 for 64 bits ones, possibly with
		# leading space(s)...
		result=`od -An -t x1 -j 4 -N 1 $f`
		# Trims leading spaces from the result
		result=${result#"${result%%[![:space:]]*}"}
		if [ "$result" == "01" ] ; then
			if [ "$libs32" == "" ] ; then
				libs32=$f
			else
				libs32="$libs32 $f"
			fi
		else
			echo "  - dependencies for $f..."
			if ldd $f 2>&1 | grep 'not ' &>/dev/null ; then
				ldd $f 2>&1 | grep 'not ' | sort | uniq
				cando="no"
			fi
		fi
	done
	if [ "$cando" == "no" ] ; then
		echo "==> The viewer cannot run on this system !"
		echo "Please, install the missing libraries using your sistribution package manager."
	else
		echo "==> The viewer itself should run just fine on this system."
	fi
	echo
	echo "Checking for missing system libraries for the plugins:"
	cando="yes"
	files=`find ./bin/llplugin -type f -print | grep -v '\.pak' | sort`
	for f in $files ; do
		echo "  - dependencies for $f..."
		if ldd $f 2>&1 | grep 'not ' &>/dev/null ; then
			ldd $f 2>&1 | grep 'not ' | sort | uniq
			cando="no"
		fi
	done
	if [ "$cando" == "no" ] ; then
		echo "==> The media plugins cannot run on this system !"
		echo "Please, install the missing libraries using your sistribution package manager."
	else
		echo "==> The media plugins should run just fine on this system."
	fi
	cando="cannot"
	if [ -x ./bin/tracy ] ; then
		echo
		echo "Checking for missing system libraries for the Tracy profiler:"
		echo "  - dependencies for ./bin/tracy..."
		if ldd $f 2>&1 | grep 'not ' &>/dev/null ; then
			ldd $f 2>&1 | grep 'not ' | sort | uniq
		else
			cando="can"
		fi
		echo "==> The bundled Tracy profiler $cando be used on this system."
	fi
	cando="cannot"
	if [ -x ./bin/SLVoice ] ; then
		echo
		echo "Checking for missing system libraries for the 32 bits Linux voice client:"
		echo "  - dependencies for ./bin/SLVoice..."
		if ldd $f 2>&1 | grep 'not ' &>/dev/null ; then
			ldd $f 2>&1 | grep 'not ' | sort | uniq
		else
			cando="may"
		fi
	fi
	if [ "$libs32" != "" ] ; then
		echo "Checking for missing system libraries for the 32 bits voice client libraries:"
		for f in $libs32 ; do
			echo "  - dependencies for $f..."
			if ldd $f 2>&1 | grep 'not ' &>/dev/null ; then
				ldd $f 2>&1 | grep 'not ' | sort | uniq
				cando="cannot"
			fi
		done
	fi
	echo "==> The deprecated native 32 bits Linux SLVoice client $cando be used."
	if [ "$LL_WINE_SLVOICE" != "" ] && [ -f "$LL_WINE_SLVOICE" ] ; then
		echo "==> The Wine SLVoice.exe client is configured and will be used instead."
	else
		echo "You may use the install-wine-SLVoice.sh script to install the"
		echo "Wine SLVoice.exe client (recommended)."
	fi
	echo
	read -s -n 1 -p "Press any key to exit."
	echo
	exit 0
fi

# Make it so that CEF will find its various executables...
export PATH="$PATH:$run_path/bin:$run_path/bin/llplugin"
# We do not use a sandbox any more (disabled in our custom Dullahan), since it
# only causes troubles.
#if [ "$UID" != "0" ] ; then
#	export CHROME_DEVEL_SANDBOX=$run_path/bin/llplugin/chrome-sandbox
#else
#	# Disable the sandbox when the viewer is ran as root, else CEF v3.257 or
#	# newer fails to initialize.
#	export CHROME_DEVEL_SANDBOX=""
#fi

# Preload any jemalloc shared library (preload is needed for the CEF plugin to
# work properly).
if [ -f "$run_path/lib/libjmalloc.so.2" ] ; then
	export LD_PRELOAD="$run_path/lib/libjmalloc.so.2 $LD_PRELOAD"
fi
# Because of the *insane* TLS space it uses, we need to preload CEF v119+ now,
# otherwise we get an error from apr_dso_load() when SLPlugin loads Dullahan:
# libcef.so: "cannot allocate memory in static TLS block".
# We must also preload libcef.so *last*, in order to have it using jemalloc.
export LD_PRELOAD="$LD_PRELOAD $run_path/lib/libcef.so"

# Run under gdb when --debug was passed as the first option in the command
# line. If you have a symbols file in bin/ (that you can obtain via:
# eu-strip --strip-debug --remove-comment -o /dev/null -f CoolVLViewer.debug CoolVLViewer
# ), then it will be auto-loaded as well.
gdb_init="/tmp/gdb.init.$$"
if [ "$TMP" != "" ] && [ -d $TMP ] ; then
	gdb_init="$TMP/gdb.init.$$"
elif [ "$TEMPDIR" != "" ] && [ -d $TEMPDIR ] ; then
	gdb_init="$TEMPDIR/gdb.init.$$"
fi
if [ "$1" == "--debug" ] ; then
	shift
	if ! which gdb &>/dev/null ; then
		echo "Sorry, the 'gdb' debugger cannot be found on your system."
		exit 1
	fi
	symbols=""
	if [ -f "$run_path/../CoolVLViewer.debug" ] ; then
		# Symbols file location when the viewer is ran from the source tree it
		# was compiled from.
		symbols="$run_path/../CoolVLViewer.debug"
	elif [ -f "$run_path/bin/CoolVLViewer.debug" ] ; then
		# Traditional case where the symbols file is placed along the binary.
		symbols="$run_path/bin/CoolVLViewer.debug"
	elif [ -f "$run_path/.buildir" ] ; then
		# If the hidden .buildir file is present, then recover the build
		# directory path from it, in case CoolVLViewer.debug can still be
		# found there...
		symbols=`cat "$run_path/.buildir"`
		symbols="$symbols/CoolVLViewer.debug"
	fi
	if [ ! -f "$run_path/bin/CoolVLViewer.debug" ] && [ -f $symbols ] ; then
		# If there is no symbols file in the bin/ directory (i.e. where gdb
		# will try and load it from), use the other symbols file we found.
		# Alas, quoting as "-ex \"symbol-file $symbols\"" in the gdb command
		# line would not work (symbol-file and $symbols would be counted as two
		# separate options instead of a quoted entity)... bash is stupid !
		# We therefore use a temporary init file instead...
		echo "set verbose on" >$gdb_init
		echo "symbol-file $symbols" >>$gdb_init
		echo "set verbose off" >>$gdb_init
		LL_WRAPPER="gdb -x $gdb_init --args"
	else
		LL_WRAPPER='gdb --args'
	fi
elif [ "$1" == "--edb" ] ; then
	shift
	if ! which edb &>/dev/null ; then
		echo "Sorry, the 'edb' debugger cannot be found on your system."
		exit 1
	fi
	LL_WRAPPER='edb --run'
elif [ "$1" == "--lldb" ] ; then
	shift
	if ! which lldb &>/dev/null ; then
		echo "Sorry, the 'lldb' debugger cannot be found on your system."
		exit 1
	fi
	symbols=""
	if [ -f "$run_path/../CoolVLViewer.debug" ] ; then
		# Symbols file location when the viewer is ran from the source tree it
		# was compiled from.
		symbols="$run_path/../CoolVLViewer.debug"
	elif [ -f "$run_path/bin/CoolVLViewer.debug" ] ; then
		# Traditional case where the symbols file is placed along the binary.
		symbols="$run_path/bin/CoolVLViewer.debug"
	elif [ -f "$run_path/.buildir" ] ; then
		# If the hidden .buildir file is present, then recover the build
		# directory path from it, in case CoolVLViewer.debug can still be
		# found there...
		symbols=`cat "$run_path/.buildir"`
		symbols="$symbols/CoolVLViewer.debug"
	fi
	if [ -f $symbols ] ; then
		# If there is a symbols file use it. We must do this as a command at
		# lldb command prompt, so we need an auxiliary command file, just like
		# for gdb, and ask lldb to source it...
		echo "add-dsym $symbols" >$gdb_init
		LL_WRAPPER="lldb --source $gdb_init -- "
	else
		LL_WRAPPER='lldb -- '
	fi
fi

if [ "$LL_WRAPPER" != "" ] ; then
	# Never display reports when using a wrapper, because we cannot recover the
	# actual viewer pid in this case.
	LL_REPORT=""
fi

default_log_file_is_ours="yes"
if [ "$LL_REPORT" != "" ] ; then
	# Note: lsof can be slow, so do not check if there is no viewer running at
	# all, i.e. if no marker file is present.
	if [ -f "$logs_dir/SecondLife.exec_marker" ] ; then
		# Remember if a primary Cool VL Viewer session is already running
		if which lsof &>/dev/null ; then
			lsof | grep "$logs_dir/CoolVLViewer.log" &>/dev/null
			if (( $? == 0 )); then
				default_log_file_is_ours="no"
			fi
		else
			echo "WARNING:"
			echo "You requested a report on exit but I cannot tell if another viewer session"
			echo "is running, because the 'lsof' utility is not installed on your system..."
			# Better not attempting to present the warnings from a log we are not
			# sure to own...
			default_log_file_is_ours="no"
		fi
	fi
fi

# Always register the secondlife:// protocol handler as being our viewer
if [ -x ./bin/register_secondlifeprotocol.sh ] ; then
	echo "Registering the secondlife:// protocol handler..."
	./bin/register_secondlifeprotocol.sh
fi

# Used by SDL2 to set our viewer window name and class.
export SDL_VIDEO_X11_WMCLASS="Cool VL Viewer"
# Install our application icons, when not already done or at least attempted.
if ! [ -f "$user_app_dir/.cvlv_icons_installed" ] ; then
	touch "$user_app_dir/.cvlv_icons_installed"
	# Match the icon name with the window name.
	icon_name="$SDL_VIDEO_X11_WMCLASS"
	if which xdg-icon-resource &>/dev/null ; then
		for size in 32 48 64 128 256; do
			icon_file="$run_path/res-sdl/cvlv_icon${size}.png"
			xdg-icon-resource install --novendor --context apps --size "$size" "$icon_file" "$icon_name" &>/dev/null
		done
	elif [ -d "$HOME/.local/share/icons" ] ; then
		for size in 32 48 64 128 256; do
			icon_file="$run_path/res-sdl/cvlv_icon${size}.png"
			dest_dir="$HOME/.local/share/icons/hicolor/${size}x${size}/apps"
			mkdir -p "$dest_dir"
			cp -af "$icon_file" "$dest_dir/$icon_name.png"
		done
	elif [ -d "$HOME/.icons" ] ; then
		cp -af "$run_path/cvlv_icon.png" "$HOME/.icons/$icon_name.png"
	fi
fi

# Disable core dumps (especially useful for dullahan_host...).
ulimit -c 0

sl_cmd='$LL_WRAPPER $viewer_binary'
sl_opt="$LL_SUP_OPT $@"

echo "Launching the viewer..."
# Run the viewer
eval $sl_cmd $sl_opt
# Recover the viewer exit error code
err=$?

# Remove the gdb init file if it was created
if [ -f $gdb_init ] ; then
	rm -f $gdb_init
fi

# Handle any resulting error.
if  [ "$LL_REPORT" == "" ] && (( $err == 127 || $err == 1 )) ; then
	echo '*** Bad shutdown. ***'
	if ! [ -f "$logs_dir/stack_trace.log" ] ; then
		cat << EOFMARKER
The most common problem when the viewer fails to launch and you get
'error while loading shared libraries'
can be solved by installing the missing libraries from your Linux distribution
packages. Try:
./cool_vl_viewer --check-libs
to spot any missing dependencies.
EOFMARKER
	fi
fi

if [ "$LL_REPORT" == "" ] ; then
	# Do not go farther if reports are off and exit the wrapper script with the
	# viewer exit code.
	exit $err
fi

# Try to find a X11 dialog
dialogcmd=""
if which Xdialog &>/dev/null ; then
	dialogcmd="Xdialog"
elif which zenity &>/dev/null ; then
	dialogcmd="zenity"
elif which kdialog &>/dev/null ; then
	dialogcmd="kdialog"
elif which gxmessage &>/dev/null ; then
	dialogcmd="gxmessage"
elif which kmessage &>/dev/null ; then
	dialogcmd="kmessage"
elif which xmessage &>/dev/null ; then
	dialogcmd="xmessage"
fi

# This function displays a report with Xdialog, zenity, kdialog or xmessage
# (or equivalent gxmessage, kmessage) when present on the system, or via a
# simple echo on sdtout. It also presents the stack trace log if it exists.
function report() {
	title='Cool VL Viewer report'
	stacktrace="$logs_dir/stack_trace.log"
	logfile="$logs_dir/CoolVLViewer.log"
	if [ "$default_log_file_is_ours" == "no" ] ; then
		# Do not display warnings in this case, for we cannot be sure that the
		# CoolVLViewer.old file (which is the log name for secondary sessions)
		# actually pertains to this closed session (if more than two sessions
		# have been running before we exited ours).
		logfile=""
	fi
	if [ "$dialogcmd" == "Xdialog" ]; then
		if [ -f "$stacktrace" ]; then
			Xdialog --title "$title" --backtitle "$1" --no-cancel --textbox "$stacktrace" 0 0
		elif [ "$logfile" != "" ] && grep -a " WARNING: " "$logfile" &>/dev/null ; then
			grep -a " WARNING: " "$logfile" | Xdialog --title "$title" --backtitle "$1" --no-cancel --textbox "-" 0 0
		else
			Xdialog --title "$title" --msgbox "$1" 0 0
		fi
	elif [ "$dialogcmd" == "zenity" ]; then
		if [ -f "$stacktrace" ]; then
			zenity --title="$1" --text-info --no-wrap --filename="$stacktrace"
		else
			zenity --title="$title" --info --no-wrap --text="$1"
		fi
	elif [ "$dialogcmd" == "kdialog" ]; then
		if [ -f "$stacktrace" ]; then
			kdialog --title "$title" --textbox "$stacktrace" 640 480
		else
			kdialog --title "$title" --msgbox "$1"
		fi
	elif [ "$dialogcmd" == "xmessage" ] || [ "$dialogcmd" == "gxmessage" ] ||  [ "$dialogcmd" == "kmessage" ]; then
		if [ -f "$stacktrace" ]; then
			$dialogcmd -title "$title" -center -file "$stacktrace" &
		fi
		$dialogcmd -title "$title" -center "$1"
	else
		echo "$1"
		if [ -f "$stacktrace" ]; then
			echo "Stactrace:"
			cat "$stacktrace"
		fi
	fi
}

if [ -f "$logs_dir/CoolVLViewer.logout_marker" ]; then
	report "A crash occurred during logout."
elif [ -f "$logs_dir/CoolVLViewer.llerror_marker" ]; then
	report "A crash was triggered (llerrs) during the session due to an unrecoverable error."
elif [ -f "$logs_dir/CoolVLViewer.error_marker" ]; then
	report "A crash occurred during the session."
elif [ -f "$logs_dir/SecondLife.exec_marker" ]; then
	# Check to see if another session is still holding the exec marker.
	# Note 1: in case of a viewer freeze we reach this point only after the
	# user killed the session manually (via a 'kill' for example), so it is
	# safe to assume our own session is not holding the exec marker any more.
	# Note 2: if 'lsof' is not available on the system, this detection will
	# not work, so we tell the user what we can...
	if which lsof &>/dev/null ; then
		lsof | grep "$logs_dir/SecondLife.exec_marker" &>/dev/null
		if (( $? == 0 )); then
			report "The session has terminated normally. Another session is still running."
		else
			report "The viewer froze during the session."
		fi
	else
		report "The viewer froze during the session (or another session is still running)."
	fi
else
	report "The session has terminated normally."
fi

# Exit the wrapper script with the viewer exit code.
exit $err
