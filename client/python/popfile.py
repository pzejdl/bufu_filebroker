#!/usr/bin/env python

import sys
import requests

# Check for good python version, otherwise there will be problems with requests package
assert sys.version_info >= (2,7,5)

def getFile():
    r  = requests.get('http://htcp40:8080/popfile', params={'runnumber': '1000030354'})
    print r,"\n",r.text

getFile()
