#!/bin/bash
SERVICE="bufu_filebroker.service"
LOG_DIR="/cmsnfsscratch/globalscratch/pzejdl/bufu_filebroker/log/"
LINES="30"

## ----

LOG="${LOG_DIR}/$(hostname)--failure.log"

echo "-- START ----------------------------------------------------------------- $(date): $(hostname): $0: bufu_filebroker service failure detected, dumping last $LINES lines of journal:" >> $LOG
journalctl -n $LINES -u $SERVICE >> $LOG
