#!/bin/sh
/usr/bin/dbus-send --system --type=method_call --print-reply --dest=org.formatique.MediaPlayer /org/formatique/MediaPlayer org.freedesktop.DBus.Introspectable.Introspect
