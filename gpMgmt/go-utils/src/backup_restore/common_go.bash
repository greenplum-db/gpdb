#!/usr/bin/env bash

function set_gopath() {
    #Set GOPATH
    pushd ../..
    if [ -z $GOPATH ]
    then
        GOPATH=$PWD
    else
        GOPATH=$GOPATH:$PWD
    fi
    popd
}
