#!/bin/bash

# Sends an URL of the form secondlife:// to any running viewer via D-Bus, or
# starts the Cool VL Viewer if no viewer is running, passing it the URL as the
# login location.

URL="$1"

if [ -z "$URL" ]; then
    echo Usage: $0 secondlife://...
    exit 1
fi

sent="no"

if which dbus-send &>/dev/null ; then
	if dbus-send --session --type=method_call --print-reply --dest=com.secondlife.ViewerAppAPIService /com/secondlife/ViewerAppAPI com.secondlife.ViewerAppAPI.GoSLURL string:"$URL" ; then
		sent="yes"
	fi
fi

if [ "$sent" == "no" ] ; then
	RUN_PATH=`dirname "$0" || echo .`
	cd "${RUN_PATH}"
	cd ..
	exec ./cool_vl_viewer -url \'"${URL}"\'
fi
