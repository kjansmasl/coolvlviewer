#!/bin/bash

# Message box wrapper script (c)2015-2023 Henri Beauchamp
#
# This wrapper script allows to emulate OSMessageBox() without needing to make
# the viewer dependent upon the GTK libraries. The script will call any of
# Xdialog, zenity, gxmessage, kmessage or xmessage (in this preference order)
# if present on the system to display the OK, OK/Cancel or Yes/No boxes.

# Restore LD_LIBRARY_PATH from SAVED_LD_LIBRARY_PATH if it exists
if [[ -v SAVED_LD_LIBRARY_PATH ]]; then
	export LD_LIBRARY_PATH="$SAVED_LD_LIBRARY_PATH"
fi

# Restore LD_PRELOAD from SAVED_LD_PRELOAD if it exists
if [[ -v SAVED_LD_PRELOAD ]]; then
	export LD_PRELOAD="$SAVED_LD_PRELOAD"
fi

# Try to find a X11 dialog.
DIALOG=""
if which Xdialog &>/dev/null ; then
	DIALOG="Xdialog"
elif which zenity &>/dev/null ; then
	DIALOG="zenity"
elif which gxmessage &>/dev/null ; then
	DIALOG="gxmessage"
elif which kmessage &>/dev/null ; then
	DIALOG="kmessage"
elif which xmessage &>/dev/null ; then
	DIALOG="xmessage"
else
	exit -1
fi

TYPE=$MESSAGE_BOX_TYPE
if [ "$TYPE" == "" ] ; then
	TYPE=0
fi

CAPTION=$MESSAGE_BOX_CAPTION
if [ "$CAPTION" == "" ] ; then
	CAPTION="Cool VL Viewer"
fi

TEXT="$@"
if [ "$TEXT" == "" ] ; then
	exit -1
fi

RETVAL=-1

if [ "$DIALOG" == "Xdialog" ] ; then
	case $TYPE in
		1)
			$DIALOG --title "$CAPTION" --ok-label OK --cancel-label Cancel --left --yesno "$TEXT" 0 0
			RETVAL=$?
			if [[ $RETVAL == 0 ]] ; then
				RETVAL=2
			elif [[ $RETVAL == 1 ]] ; then
				RETVAL=3
			fi
			;;
		2)
			$DIALOG --title "$CAPTION" --left --yesno "$TEXT" 0 0
			RETVAL=$?
			;;
		*)
			$DIALOG --title "$CAPTION" --left --msgbox "$TEXT" 0 0
			RETVAL=$?
			if [[ $RETVAL == 0 ]] ; then
				RETVAL=2
			fi
			;;
	esac
elif [ "$DIALOG" == "zenity" ] ; then
	case $TYPE in
		1)
			$DIALOG --title="$CAPTION" --ok-label=OK --cancel-label=Cancel --question --no-wrap --text="$TEXT"
			RETVAL=$?
			if [[ $RETVAL == 0 ]] ; then
				RETVAL=2
			elif [[ $RETVAL == 1 ]] ; then
				RETVAL=3
			fi
			;;
		2)
			$DIALOG --title="$CAPTION" --question --no-wrap --text="$TEXT"
			RETVAL=$?
			;;
		*)
			$DIALOG --title="$CAPTION" --info --no-wrap --text="$TEXT"
			RETVAL=$?
			if [[ $RETVAL == 0 ]] ; then
				RETVAL=2
			fi
			;;
	esac
elif [ "$DIALOG" != "" ]; then
	case $TYPE in
		1)
			$DIALOG -title "$CAPTION" -center -buttons "OK:2,Cancel:3" "$TEXT"
			RETVAL=$?
			;;
		2)
			$DIALOG -title "$CAPTION" -center -buttons "Yes:0,No:1" "$TEXT"
			RETVAL=$?
			;;
		*)
			$DIALOG -title "$CAPTION" -center -buttons "OK:2" "$TEXT"
			RETVAL=$?
			;;
	esac
fi

exit $RETVAL
