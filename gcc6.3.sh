#!/bin/sh
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
        echo "ERROR: Script ${BASH_SOURCE[0]} has to be sourced."
        exit
fi

source /afs/cern.ch/sw/lcg/external/gcc/6.3/x86_64-centos7/setup.sh

gcc --version
