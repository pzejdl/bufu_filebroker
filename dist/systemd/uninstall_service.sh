#!/bin/sh
SERVICE1="bufu_filebroker.service"
SERVICE2="bufu_filebroker_crash_notify.service"

TARGET="/usr/lib/systemd/system/"

systemctl stop $SERVICE1

rm -f "$TARGET/$SERVICE1"
rm -f "$TARGET/$SERVICE2"

systemctl daemon-reload
