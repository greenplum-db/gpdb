#!/bin/bash
# start a python http server (port is $1) in background

which python3 > /dev/null
if [ $? -eq 0 ] 
then
    python3 -m http.server $1 >/dev/null 2>&1 &
    echo "started a http server"
    exit 0
fi

which python2 > /dev/null
if [ $? -eq 0 ] 
then
    python2 -m SimpleHTTPServer $1 >/dev/null 2>&1 &
    echo "started a http server"
    exit 0
fi

echo "no python found"
exit 1
