#!/bin/bash

# Registers a protocol handler for URLs of the form secondlife:// and hop://

HANDLER="$1"

RUN_PATH=`dirname "$0" || echo .`
cd "$RUN_PATH"

if [ -z "$HANDLER" ]; then
    HANDLER=`pwd`/handle_secondlifeprotocol.sh
fi

if ! [ -x "$HANDLER" ]; then
	echo "Warning: could not find the protocol handler script: $HANDLER"
	exit 0
fi

# Register handler for GNOME-aware apps (deprecated ?)
LLGCONFTOOL2=gconftool-2
if which $LLGCONFTOOL2 &>/dev/null; then
    ($LLGCONFTOOL2 -s -t string /desktop/gnome/url-handlers/secondlife/command "$HANDLER \"%s\"" && $LLGCONFTOOL2 -s -t bool /desktop/gnome/url-handlers/secondlife/enabled true) || echo "Warning: did not register secondlife:// handler with GNOME: $LLGCONFTOOL2 failed."
    ($LLGCONFTOOL2 -s -t string /desktop/gnome/url-handlers/hop/command "$HANDLER \"%s\"" && $LLGCONFTOOL2 -s -t bool /desktop/gnome/url-handlers/hop/enabled true) || echo "Warning: did not register hop:// handler with GNOME: $LLGCONFTOOL2 failed."
    ($LLGCONFTOOL2 -s -t string /desktop/gnome/url-handlers/x-grid-info/command "$HANDLER \"%s\"" && $LLGCONFTOOL2 -s -t bool /desktop/gnome/url-handlers/x-grid-info/enabled true) || echo "Warning: did not register x-grid-info:// handler with GNOME: $LLGCONFTOOL2 failed."
    ($LLGCONFTOOL2 -s -t string /desktop/gnome/url-handlers/x-grid-location-info/command "$HANDLER \"%s\"" && $LLGCONFTOOL2 -s -t bool /desktop/gnome/url-handlers/x-grid-location-info/enabled true) || echo "Warning: did not register x-grid-info:// handler with GNOME: $LLGCONFTOOL2 failed."
else
    echo "Warning: did not register secondlife:// and hop:// handlers with GNOME: $LLGCONFTOOL2 not found."
fi

# Register handler for KDE-aware apps (still valid ?)
for LLKDECONFIG in kde-config kde4-config; do
    if which $LLKDECONFIG &>/dev/null ; then
        LLKDEPROTODIR=`$LLKDECONFIG --path services | cut -d ':' -f 1`
        if [ -d "$LLKDEPROTODIR" ]; then
            LLKDEPROTOFILE=$LLKDEPROTODIR/secondlife.protocol
            cat > $LLKDEPROTOFILE <<EOF1 || echo "Warning: did not register secondlife:// handler with KDE: Could not write $LLKDEPROTOFILE"
[Protocol]
exec=$HANDLER '%u'
protocol=secondlife
input=none
output=none
helper=true
listing=
reading=false
writing=false
makedir=false
deleting=false
EOF1
            LLKDEPROTOFILE=$LLKDEPROTODIR/hop.protocol
            cat > $LLKDEPROTOFILE <<EOF2 || echo "Warning: did not register hop:// handler with KDE: Could not write $LLKDEPROTOFILE"
[Protocol]
exec=$HANDLER '%u'
protocol=hop
input=none
output=none
helper=true
listing=
reading=false
writing=false
makedir=false
deleting=false
EOF2
            LLKDEPROTOFILE=$LLKDEPROTODIR/x-grid-info.protocol
            cat > $LLKDEPROTOFILE <<EOF3 || echo "Warning: did not register hop:// handler with KDE: Could not write $LLKDEPROTOFILE"
[Protocol]
exec=$HANDLER '%u'
protocol=x-grid-info
input=none
output=none
helper=true
listing=
reading=false
writing=false
makedir=false
deleting=false
EOF3
            LLKDEPROTOFILE=$LLKDEPROTODIR/x-grid-location-info.protocol
            cat > $LLKDEPROTOFILE <<EOF4 || echo "Warning: did not register hop:// handler with KDE: Could not write $LLKDEPROTOFILE"
[Protocol]
exec=$HANDLER '%u'
protocol=x-grid-location-info
input=none
output=none
helper=true
listing=
reading=false
writing=false
makedir=false
deleting=false
EOF4
        else
            echo "Warning: did not register handlers with KDE: directory $LLKDEPROTODIR does not exist."
        fi
    fi
done

#
# Register our handler in local MIME database (should be working with all
# modern FreeDesktop-compatible environments)
#

# Make sure the local application desktop entries directory exists
APPSDIR="$HOME/.local/share/applications"
mkdir -p "$APPSDIR"

# Add local mimeapps.list if not yet here
MIMEAPPSLIST="$APPSDIR/mimeapps.list"
touch "$MIMEAPPSLIST"

# Remove any old association fors the secondlife and hop URI protocols
grep -v "x-scheme-handler/secondlife" "$MIMEAPPSLIST" >"$MIMEAPPSLIST.tmp"
grep -v "x-scheme-handler/hop" "$MIMEAPPSLIST.tmp" >"$MIMEAPPSLIST"
grep -v "x-scheme-handler/x-grid-info" "$MIMEAPPSLIST" >"$MIMEAPPSLIST.tmp"
grep -v "x-scheme-handler/x-grid-location-info" "$MIMEAPPSLIST.tmp" >"$MIMEAPPSLIST"
rm -f "$MIMEAPPSLIST.tmp"

# Add our own protocol handler
echo "x-scheme-handler/secondlife=secondlife-url-handler.desktop" >>"$MIMEAPPSLIST"
echo "x-scheme-handler/hop=secondlife-url-handler.desktop" >>"$MIMEAPPSLIST"
echo "x-scheme-handler/x-grid-info=secondlife-url-handler.desktop" >>"$MIMEAPPSLIST"
echo "x-scheme-handler/x-grid-location-info=secondlife-url-handler.desktop" >>"$MIMEAPPSLIST"
cat >"$APPSDIR/secondlife-url-handler.desktop" <<EOF5
[Desktop Entry]
Encoding=UTF-8
Version=1.0
Name="Second Life and OpenSim URL handler"
Comment="secondlife:// and hop:// protocols handler"
Type=Application
Exec="$HANDLER" %u
Terminal=false
StartupNotify=false
NoDisplay=true
MimeType=x-scheme-handler/secondlife;x-scheme-handler/hop;x-scheme-handler/x-grid-info;x-scheme-handler/x-grid-location-info
Categories=Network;
EOF5

# Update the MIME database cache
if which update-desktop-database &>/dev/null ; then
	update-desktop-database $APPSDIR
fi
