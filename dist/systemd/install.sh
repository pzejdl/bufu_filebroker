#!/bin/sh
SERVICE1="bufu_filebroker.service"
SERVICE2="bufu_filebroker_crash_notify.service"

SOURCE="/globalscratch/pzejdl/bufu_filebroker/systemd/"
TARGET="/usr/lib/systemd/system/"

cp "$SOURCE/$SERVICE1" "$TARGET/$SERVICE1"
cp "$SOURCE/$SERVICE2" "$TARGET/$SERVICE2"

chown root:root "$TARGET/$SERVICE1"
chown root:root "$TARGET/$SERVICE1"

systemctl daemon-reload
