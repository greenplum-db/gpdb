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

if len(sys.argv) < 2:
    print "usage: createData.py <number of lines> <datatype>\n"
    print "datatype:"
    print "all:regression:time:timestamp:date:bigint:int:smallest:real:float:boolean:varchar:bpchar:numeric:text\n"
    sys.exit()

LINE_NUM=int(sys.argv[1])
DATATYPE=sys.argv[2]

def printtimestr(x, datatype): 
    HH=str(x % 24).zfill(2)
    MM=str(x % 60).zfill(2)
    SS=str(x % 60).zfill(2)
    sss=str(x % 999)
    h=str(x % 12).zfill(2)
    ampm=x % 2
    ampmlist= ['AM','PM']
    timezone= x % 5
    #timezonelist= ['ACDT','ACT','PST','ADT','ACWST','GMT0','EST5EDT','zulu']
    timezonelist= ['ACT','PST','ADT','GMT0','zulu']

    year = str((x % 1000) + 1).zfill(4)
    month = str((x % 12) + 1).zfill(2)
    monthindex = x%24
    monthlist = ['January','Jan','February','Feb','March','Mar','April','Apr','May','May','June','Jun','July','Jul','August','Aug','September','Sept','October','Oct','November','Nov','December','Dec']
    day = str((x % 30) + 1).zfill(2)
    daynofill = str((x % 30) + 1) 

    if (datatype == 'time'):
        
        #col1 - HH:MM:SS     
        col1 = HH+ ':' +MM+ ':' +SS
        #col2 - HH:MM:SS.sss     
        col2 = col1+ '.' +sss
        #col3 - HHMMSS     
        col3 = HH+MM+SS
        #col4 - HH:MM AM/PM 
        col4 = h+ ':' +MM+ ' ' +ampmlist[ampm]
        #col5 - HH:MM:SS.sss-h (timeoffset)
        col5 = col2+ '-' +str(timezone)
        #col6 - HH:MM:SS-HH:MM(timeoffset) 
        col6 = col1+ '-' +h+ ':00'
        #col7 - HH:MM-HH:MM(timeoffset) 
        col7 = HH+':'+MM+ '-' +h+ ':00'
        #col8 - HHMMSS-HH(timeoffset) 
        col8 = col3+ '-' +h
        #col9 - HH:MM:SS XXX(timezone)    
        col9 = col1+ " " +timezonelist[timezone]
        
        return col1+'\t'+col2+'\t'+col3+'\t'+col4+'\t'+col5+'\t'+col6+'\t'+col7+'\t'+col8+'\t'+col9+'\t\\N'
    elif (datatype == 'timestamp'):
        #1999-01-08 04:05:06
        col1 = year+'-' +month+ '-' +day+ ' ' +HH+ ':' +MM+ ':' +SS
        #1999-01-08 04:05:06 -8:00
        col2 = col1+ ' -' +str(timezone)+ ':00'
        #January 8 04:05:06 1999 PST
        col3 = monthlist[monthindex]+ ' ' +daynofill+ ' ' +HH+ ':' +MM+ ':' +SS+ ' ' +year+ ' ' +timezonelist[timezone]

        return col1+'\t'+col2+'\t'+col3+'\t\\N'
        

    elif (datatype == 'date'):
        #1900-01-01
        col1 = year+ '-' +month+ '-' +day
        #September 01, 1999
        col2 = monthlist[monthindex]+ ' ' +day+ ', ' +year
        #1/8/1999
        col3 = month+ '/' +day+ '/' +year
        #1999-Jan-08
        col4 = year+ '-' +monthlist[monthindex]+ '-' +day
        #Jan-08-1999
        col5 = monthlist[monthindex]+ '-' +month+ '-' +year
        #08-Jan-1999
        col6 = month+ '-' +monthlist[monthindex]+ '-' +year
        #January 8, 99 BC
        col7 = monthlist[monthindex]+' ' +month+ ', ' +year+ ' BC'

        return col1+'\t'+col2+'\t'+col3+'\t'+col4+'\t'+col5+'\t'+col6+'\t'+col7+'\t\\N'
    

    

def regression(x):
    numRecipes = str(1664525 * x + 1013904223)
    Borland = str(22695477 * x + 1)
    glibc = str(1103515245 * x + 12345)
    appCarbonLib = str((16807 * x) % 2147483647)
    vax = str(69069 * x + 1)
    javaRandomClass = str(25214903917 * x + 11)
    return str(x)+'\t'+str(hex(x))+'\t'+numRecipes+'\t'+Borland+'\t'+glibc+'\t'+appCarbonLib+'\t'+vax+'\t'+javaRandomClass

def printintstr(x, max, min):
    if (x < max):
        m = x
    else:
        m = 0
    maxsubx = max - m
    minplusx = min + m
    return str(max)+'\t'+str(min)+'\t'+str(m)+'\t'+str(maxsubx)+'\t'+str(minplusx)+'\t\\N'

def printfloatstr(x,max,min):
    pi = float(22)/float(7)
    pimulti = pi*x
    return str(max)+'\t'+str(min)+'\t'+str(pi)+'\t'+str(pimulti)+'\t\\N'

def printbool(x):
    n = x % 2
    if n == 0:
        return 'true\t\\N'
    else:
        return 'false\t\\N'

def printchar(x):
    strx = ''
    currentchar = x%128
    for m in range(currentchar):
        strx = strx+chr(currentchar)
        if (currentchar == 9 or currentchar == 13 or currentchar == 10):
            strx = 'skip'
    return str(x)+'\t'+strx+'\t\\N'


for x in range(LINE_NUM):

    if (DATATYPE == 'regression'):
        print regression(x)
    elif (DATATYPE == 'time'):
        print 'time\t'+printtimestr(x,'time')
    elif (DATATYPE == 'timestamp'):
        print 'timestamp\t'+printtimestr(x,'timestamp')
    elif (DATATYPE == 'date'):
        print 'date\t'+printtimestr(x,'date')
    elif (DATATYPE == 'bigint'):
        print 'bigint\t'+printintstr(x,9223372036854775807,-9223372036854775808)
    elif (DATATYPE == 'int'):
        print 'int\t'+printintstr(x,2147483647,-2147483648)
    elif (DATATYPE == 'smallint'):
        print 'smallint\t'+printintstr(x,32767,-32768)
    elif (DATATYPE == 'real'):
        print 'real\t'+printfloatstr(x, 3.4028235E+38, -3.4028234E+38)
    elif (DATATYPE == 'float'):
        print 'float\t'+printfloatstr(x,+1.797693134862315E+308, -1.797693134862315E+308)
    elif (DATATYPE == 'boolean'):
        print 'boolean\t'+printbool(x)  
    elif (DATATYPE == 'varchar'):
        print 'varchar\t'+printchar(x)
    elif (DATATYPE == 'bpchar'):
        print 'bpchar\t'+printchar(x)
    elif (DATATYPE == 'numeric'):
        print 'numeric\t'+printintstr(x, 9223372036854775807000, -9223372036854775808000)
    elif (DATATYPE == 'text'):
        print 'text\t'+printchar(x)
    elif (DATATYPE == 'all'):
        print regression(x)+ '\t' +printtimestr(x,'time')+ '\t' +printtimestr(x,'timestamp')+ '\t' +printtimestr(x,'date')+ '\t' +printintstr(x,9223372036854775807,-9223372036854775808)+ '\t' +printintstr(x,2147483647,-2147483648)+ '\t' +printintstr(x,32767,-32768)+ '\t' +printfloatstr(x, 3.4028235E+38, -3.4028234E+38)+ '\t' +printfloatstr(x, +1.797693134862315E+308, -1.797693134862315E+308)+ '\t' +printbool(x)+ '\t' +printchar(x)+ '\t'+printchar(x)+ '\t'+printintstr(x,9223372036854775807000,-9223372036854775808000)+ '\t'+printchar(x)
