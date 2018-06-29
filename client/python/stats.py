#!/usr/bin/env python

import sys
import requests

# Check for good python version, otherwise there will be problems with requests package
assert sys.version_info >= (2,7,5)

def getStats():
    r  = requests.get('http://htcp40:8080/stats')
    print r,"\n",r.text

    #r  = requests.get('http://htcp40:8080/stats', params={'runnumber': '123'})
    #print r,"\n",r.text

getStats()
