#!/bin/bash

if [ -z $1 ] || [ "$1" != "--stdin" ]; then
    echo "usage: $0 --stdin  for reading the input from stdin"
    exit 1
fi

# Capture the input and do some basic escaping to JSON can survive :)
MSG="$(cat /dev/stdin | awk 'BEGIN{ORS="\\n"} {gsub( "\"","\\\"" ); print}')"

#----

# UNIX epoch in miliseconds
TST=$(date +%s%3N)

# User friendly time 
MSGTIME=$(date --date="@${TST::-3}" '+%Y-%m-%d %H:%M:%S')

HOST=$(hostname)
URL="http://es-cdaq:9200/hltdlogs_dv_write/hltdlog"
SEVERITY="ERROR"
SEVERITY_VAL="3"
TYPE="4"

# RUN is optional
#RUN="1000030434"

#curl -XPOST "$URL" -d"{\"run\" : $RUN,\"severity\" : \"$SEVERITY\", \"severityVal\": $SEVERITY_VAL, \"lexicalId\" : 0, \"host\" : \"$HOST\", \"date\" : $TST, \"msgtime\" : \"$MSGTIME\", \"type\" : $TYPE, \"message\" : \"$MSG\"}"
curl -XPOST "$URL" -d"{\"severity\" : \"$SEVERITY\", \"severityVal\": $SEVERITY_VAL, \"lexicalId\" : 0, \"host\" : \"$HOST\", \"date\" : $TST, \"msgtime\" : \"$MSGTIME\", \"type\" : $TYPE, \"message\" : \"$MSG\"}"
