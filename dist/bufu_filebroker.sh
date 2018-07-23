#!/bin/sh
## 
## Service helper script 
##

## Configuration

# The service executable
SERVICE="bin/main"

# Path to all required libraries
LIB="lib"

################################################################################

export LD_LIBRARY_PATH="${LIB}:$LD_LIBRARY_PATH"
$SERVICE &
PID="$!"

# Not really necessary, but prevents misaligned output 
sleep 0.3

# Check if the services started successfully 
kill -0 $PID

if [ $? -ne 0 ]; then
#if [ ! kill -0 $PID 2>/dev/null; ] then
    echo "$0: ERROR: ${SERVICE} didn't start succesfully!"
    # Notify systemd about error
    exit 1
fi

echo "$0: INFO: ${SERVICE} started."
