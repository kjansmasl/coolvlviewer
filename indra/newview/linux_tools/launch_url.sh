#!/bin/bash

# This script loads a web page in the 'default' graphical web browser.
# It MUST return immediately (or soon), so the browser should be launched in
# the background (thus no text-only browsers). This script does not trust the
# URL to be well-escaped or shell-safe.
#
# On Unixoids we try, in order of decreasing priority:
# - $BROWSER if set (preferred)
# - Default GNOME browser
# - Default KDE browser
# - x-www-browser
# - The first browser in $BROWSER_COMMANDS that is found.

URL="$1"

if [ -z "$URL" ]; then
	echo "Usage: $(basename "$0") URL"
	exit 1
fi

# Note: LL_LAUNCHER_LOGFILE may be exported from the viewer configuration
# file to any file you wish to log to, in case you encounter issues launching
# the system browser and want to diagnose it. HB
function log_message()
{
	DATE=`date "+%Y-%m-%d %H:%M:%S"`
	echo "$DATE: $1" >>"$LL_LAUNCHER_LOGFILE"
}
if [ "$LL_LAUNCHER_LOGFILE" != "" ]; then
	# Verify we can actually access this log file and create it when it does
	# not yet exist.
	if ! touch "$LL_LAUNCHER_LOGFILE" ; then
		# Use our message box wrapper script to inform the user...
		scriptsrc=`readlink -f "$0" || echo "$0"`
		run_path=`dirname "$scriptsrc" || echo .`
		if  [ -x "$run_path/messagebox.sh" ] ; then
			export MESSAGE_BOX_CAPTION=`basename "$scriptsrc"`
			"$run_path/messagebox.sh" "Cannot access log file:\n$LL_LAUNCHER_LOGFILE" &
		fi
		LL_LAUNCHER_LOGFILE="/dev/null"
	fi
else
	LL_LAUNCHER_LOGFILE="/dev/null"
fi

log_message "Invoked: $0"

# Restore LD_LIBRARY_PATH from SAVED_LD_LIBRARY_PATH if it exists
if [[ -v SAVED_LD_LIBRARY_PATH ]]; then
	export LD_LIBRARY_PATH="$SAVED_LD_LIBRARY_PATH"
	log_message "Restored LD_LIBRARY_PATH to: '$LD_LIBRARY_PATH'"
fi

# Restore LD_PRELOAD from SAVED_LD_PRELOAD if it exists
if [[ -v SAVED_LD_PRELOAD ]]; then
	export LD_PRELOAD="$SAVED_LD_PRELOAD"
	log_message "Restored LD_PRELOAD to: '$LD_PRELOAD'"
fi

# if $BROWSER is defined, use it.
XBROWSER=`echo "$BROWSER" | cut -f 1 -d ":"`
if [ ! -z "$XBROWSER" ]; then
	XBROWSER_CMD=`echo "$XBROWSER" | cut -f 1 -d " "`
	# look for $XBROWSER_CMD either literally or in PATH
	if [ -x "$XBROWSER_CMD" ] || which $XBROWSER_CMD &>/dev/null; then
		# Check for %s string and subsitute with URL if found
		if echo "$XBROWSER" | grep "%s" &>/dev/null; then
			cmd=${$XBROWSER_CMD/"%s"/"$URL"}
			log_message "Using: '$cmd'"
			# $XBROWSER has %s which needs substituting
			echo "$URL" | xargs -r -i%s $XBROWSER &>>$LL_LAUNCHER_LOGFILE &
		else
			log_message "Using: '$XBROWSER_CMD $URL'"
			# $XBROWSER has no %s, tack URL on the end instead
			$XBROWSER "$URL" &
		fi
		exit 0
	fi
	log_message "Could not find the browser specified by the BROWSER environment variable ($BROWSER)'. Trying some others..."
fi

# Launcher the default GNOME browser.
if [ ! -z "$GNOME_DESKTOP_SESSION_ID" ] && which gnome-open &>/dev/null; then
	log_message "Using: 'gnome-open $URL'"
	gnome-open "$URL" &>>$LL_LAUNCHER_LOGFILE &
	exit 0
fi

# Launch the default KDE browser.
if [ ! -z "$KDE_FULL_SESSION" ] && which kfmclient &>/dev/null; then
	log_message "Using: 'kfmclient openURL $URL'"
	kfmclient openURL "$URL" &>>$LL_LAUNCHER_LOGFILE &
	exit 0
fi

# List of browser commands that will be tried in the order listed. x-www-browser
# will be tried first, which is a Debian alternative.
BROWSER_COMMANDS="		\
	x-www-browser		\
	palemoon			\
	waterfox			\
	seamonkey			\
	firefox-esr			\
	firefox				\
	mozilla-firefox		\
	mozilla				\
	icecat				\
	opera				\
	epiphany-browser	\
	epiphany-gecko		\
	epiphany-webkit		\
	epiphany			\
	konqueror			\
	chromium			\
	chrome				\
	google-chrome"
for browser_cmd in $BROWSER_COMMANDS; do
	if which $browser_cmd &>/dev/null; then
		log_message "Using: '$browser_cmd $URL'"
		$browser_cmd "$URL" &>>$LL_LAUNCHER_LOGFILE &
		exit 0
   fi
done

log_message "Failed to find a known browser. Please consider exporting the BROWSER environment variable in cool_vl_viewer.conf."
exit 1
