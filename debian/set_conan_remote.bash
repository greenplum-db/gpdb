#!/bin/bash

LIST=`conan remote list`
STATUS=$(echo ${LIST} | grep conan-gpdb)
if [ $? -ne 0 ] ; then
    conan remote add conan-gpdb https://api.bintray.com/conan/greenplum-db/gpdb-oss
fi
