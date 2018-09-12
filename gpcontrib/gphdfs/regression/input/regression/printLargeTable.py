#!/usr/bin/env python
"""
Copyright (C) 2004-2015 Pivotal Software, Inc. All rights reserved.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

generate random dataset with seed i
"""

############################################################################
# Set up some globals, and import gptest 
#    [YOU DO NOT NEED TO CHANGE THESE]
#
import sys,string, os, subprocess, signal,time
#MYD = os.path.abspath(os.path.dirname(__file__))
#mkpath = lambda *x: os.path.join(MYD, *x)
#UPD = os.path.abspath(mkpath('..'))
#if UPD not in sys.path:
#    sys.path.append(UPD)
#import gptest
#from gptest import psql, shell
#import re
#from time import sleep

LINE_NUM=int(sys.argv[1])

for x in range(LINE_NUM):
    numRecipes = 1664525 * x + 1013904223
    Borland = 22695477 * x + 1
    glibc = 1103515245 * x + 12345
    appCarbonLib = (16807 * x) % 2147483647
    vax = 69069 * x + 1
    javaRandomClass = 25214903917 * x + 11
    print x, '\t',hex(x),'\t',numRecipes,'\t',Borland,'\t',glibc,'\t',appCarbonLib,'\t',vax,'\t',javaRandomClass

