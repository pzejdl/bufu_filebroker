#!/bin/bash
SERVICE="bufu_filebroker.service"
LOG_DIR="/cmsnfsscratch/globalscratch/pzejdl/bufu_filebroker/log"
LINES="30"

## ----
if [ ! -d "${LOG_DIR}" ]; then
    mkdir -p "${LOG_DIR}"
    chmod 777 "${LOG_DIR}"
fi


LOG="${LOG_DIR}/$(hostname)--failure.log"

MSG="bufu_filebroker: Service failure detected, dumping last $LINES lines of journal:"
BODY="$(echo "$MSG"; journalctl -n $LINES -u $SERVICE)"

echo "-- START ----------------------------------------------------------------- $(date): $(hostname): $0: $BODY" >> $LOG

## Notify F3Mon

echo "$BODY" | /opt/bufu_filebroker/scripts/f3mon_notify.sh --stdin
