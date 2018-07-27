#!/bin/bash

# hltd configuration file
HLTD="/etc/hltd.conf"

# -- Some checks

if [ -z $1 ] || [ "$1" != "--stdin" ]; then
    echo "usage: $0 --stdin  for reading the input from stdin"
    exit 1
fi

# --- Read hltd configuration

if [ ! -f "$HLTD" ]; then
    echo "ERROR: Cannot read hltd configuration from '$HLTD'."
    exit 1
fi

elastic_runindex_name="$(sed -ne 's/elastic_runindex_name[[:space:]]*=[[:space:]]*\([[:graph:]]*\).*/\1/p' /etc/hltd.conf)"
elastic_runindex_url="$(sed -ne 's/elastic_runindex_url[[:space:]]*=[[:space:]]*\([[:graph:]]*\).*/\1/p' /etc/hltd.conf)"
use_elasticsearch="$(sed -ne 's/use_elasticsearch[[:space:]]*=[[:space:]]*\([[:graph:]]*\).*/\1/p' /etc/hltd.conf)"


if [ -z "$elastic_runindex_name" ] || [ -z "$elastic_runindex_url" ] || [ "$use_elasticsearch" != "True" ]; then
    echo "ERROR: Cannot use F3Mon, something wrong happened when reading hltd configuration:"
    echo "elastic_runindex_name = '$elastic_runindex_name'"
    echo "elastic_runindex_url = '$elastic_runindex_url'"
    echo "use_elasticsearch = '$use_elasticsearch'"
    exit 1
fi

# --- Capture the input

# Capture the input and do some basic escaping to JSON can survive :)
MSG="$(cat /dev/stdin | awk 'BEGIN{ORS="\\n"} {gsub( "\"","\\\"" ); print}')"

#----

URL="${elastic_runindex_url}/hltdlogs_${elastic_runindex_name}_write/hltdlog"

# UNIX epoch in miliseconds
TST=$(date +%s%3N)

# User friendly time 
MSGTIME=$(date --date="@${TST::-3}" '+%Y-%m-%d %H:%M:%S')

HOST=$(hostname)

# Set the severity to ERROR
SEVERITY="ERROR"
SEVERITY_VAL="3"
TYPE="4"

# RUN is optional
#RUN="1000030434"


# Send the message
#curl -XPOST "$URL" -d"{\"run\" : $RUN,\"severity\" : \"$SEVERITY\", \"severityVal\": $SEVERITY_VAL, \"lexicalId\" : 0, \"host\" : \"$HOST\", \"date\" : $TST, \"msgtime\" : \"$MSGTIME\", \"type\" : $TYPE, \"message\" : \"$MSG\"}"
curl -XPOST "$URL" -d"{\"severity\" : \"$SEVERITY\", \"severityVal\": $SEVERITY_VAL, \"lexicalId\" : 0, \"host\" : \"$HOST\", \"date\" : $TST, \"msgtime\" : \"$MSGTIME\", \"type\" : $TYPE, \"message\" : \"$MSG\"}"
