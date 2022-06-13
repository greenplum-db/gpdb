#!/bin/bash
#
# Portions Copyright (c) 2022 VMware, Inc. or its affiliates.
#
# Generate a multi field large file test dataset.
# It's used by readable_query33 test case and could avoid uploading a large file.

for x in {1..50000};do
    n=$(( $x % 2 ))
    if [ $n = 0 ] ; then
        echo `echo $x | awk '{printf("%010d2.7182.71828185",$0)}'`
    else
        echo `echo $x | awk '{printf("%010d3.1413.14159267",$0)}'`
    fi
done
