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

PURPOSE:
create a set of data in text format delimited by tabs
    :Parameters:
        LINE_NUM
            the number of lines to be generated
        DATATYPE
            datatype to be generated
            currently supported datatypes:all:regression:time:timestamp:date:bigint:int:smallest:real:float:boolean:varchar:bpchar:numeric:text
TODO: random columns generator,matching table creation statement

LAST MODIFIED:
------------------------------------------------------------------------------
"""

############################################################################
# Set up some globals, and import gptest 
#    [YOU DO NOT NEED TO CHANGE THESE]
#
import sys
import string
import os
import subprocess
import signal
import time
import getopt

if len(sys.argv) < 2:
    print "usage: createData.py <number of lines> <datatype>"
    print " or usage: createData.py <number of lines> custom,<datatypelist separated by ','>\n"
    print "datatype:"
    print "all:regression:time:timestamp:date:bigint:int:smallest:real:float:boolean:varchar:bpchar:numeric:text\n"
    sys.exit()

LINE_NUM=int(sys.argv[1])
DATATYPE=sys.argv[2]
custom = 'false'


def print_time_str(x, datatype): 
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
       
        if (custom== 'true'):
            return col1
        else: 
            return col1+'\t'+col2+'\t'+col3+'\t'+col4+'\t'+col5+'\t'+col6+'\t'+col7+'\t'+col8+'\t'+col9+'\t\\N'
    elif (datatype == 'timestamp'):
        #1999-01-08 04:05:06
        col1 = year+'-' +month+ '-' +day+ ' ' +HH+ ':' +MM+ ':' +SS
        #1999-01-08 04:05:06 -8:00
        col2 = col1+ ' -' +str(timezone)+ ':00'
        #January 8 04:05:06 1999 PST
        col3 = monthlist[monthindex]+ ' ' +daynofill+ ' ' +HH+ ':' +MM+ ':' +SS+ ' ' +year+ ' ' +timezonelist[timezone]

        if (custom== 'true'):
            return col1
        else:
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

        if (custom== 'true'):
            return col1
        else:
            return col1+'\t'+col2+'\t'+col3+'\t'+col4+'\t'+col5+'\t'+col6+'\t'+col7+'\t\\N'
    

    

def regression(x):
    numRecipes = str(1664525 * x + 1013904223)
    Borland = str(22695477 * x + 1)
    glibc = str(1103515245 * x + 12345)
    appCarbonLib = str((16807 * x) % 2147483647)
    vax = str(69069 * x + 1)
    javaRandomClass = str(25214903917 * x + 11)
    return str(x)+'\t'+str(hex(x))+'\t'+numRecipes+'\t'+Borland+'\t'+glibc+'\t'+appCarbonLib+'\t'+vax+'\t'+javaRandomClass

def print_int_str(x, max, min):
    if (x < max):
        m = x
    else:
        m = 0
    maxsubx = max - m
    minplusx = min + m

    if (custom== 'true'):
        return str(maxsubx)
    else:
        return str(max)+'\t'+str(min)+'\t'+str(m)+'\t'+str(maxsubx)+'\t'+str(minplusx)+'\t\\N'

def print_float_str(x,max,min):
    pi = float(22)/float(7)
    pimulti = pi*x
    
    if (custom== 'true'):
        return str(pimulti)
    else:
        return str(max)+'\t'+str(min)+'\t'+str(pi)+'\t'+str(pimulti)+'\t\\N'

def print_bool(x):
    n = x % 2

    if n == 0:
        if (custom== 'true'):
            return 'true'
        else:
            return 'true\t\\N'
    else:
        if (custom== 'true'):
            return 'false'
        else:
            return 'false\t\\N'

def print_char(x):
    strx = ''
    currentchar = x%128
    for m in range(currentchar):
        strx = strx+chr(currentchar)
        if (currentchar == 9 or currentchar == 13 or currentchar == 10):
            strx = 'skip'
    if (custom== 'true'):
        return strx
    else:
        return str(x)+'\t'+strx+'\t\\N'

def get_custom_type():
    thelist = DATATYPE.split(',')
    #currentstr = DATATYPE+ '\t' +str(x)
    currentstr = ''
    currentcolcnt = 1
   
    for y in (thelist):
        if (y == 'time'):
            currentstr += print_time_str(x,'time')
        elif (y == 'timestamp'):
            currentstr += print_time_str(x,'timestamp')
        elif (y == 'date'):
            currentstr += print_time_str(x,'date')
        elif (y == 'bigint'):
            currentstr += print_int_str(x,9223372036854775807,-9223372036854775808) 
        elif (y == 'int'):
            currentstr += print_int_str(x,2147483647,-2147483648)
        elif (y == 'smallint'):
            currentstr += print_int_str(x,32767,-32768)
        elif (y == 'real'):
            currentstr += print_float_str(x, 3.4028235E+38, -3.4028234E+38)
        elif (y == 'float'):
            currentstr += print_float_str(x,+1.797693134862315E+308, -1.797693134862315E+308)
        elif (y == 'boolean'):
            currentstr += print_bool(x)
        elif (y == 'numeric'):
            currentstr += print_int_str(x, 9223372036854775807000, -9223372036854775808000)
        elif (y != 'custom'): 
            currentstr += print_char(x)

        if (y != 'custom'):
            currentcolcnt += 1
            if currentcolcnt < len(thelist):
                currentstr += '\t'
    return currentstr


for x in range(LINE_NUM):

    current_line_num = str(x)
    regression_type_str = 'bigint,varchar,bigint,bigint,bigint,bigint,bigint,bigint'
    time_type_str = 'time,time,time,time,time,time,time,time,time,time'
    timestamp_type_str = 'timestamp,timestamp,timestamp,timestamp'
    date_type_str = 'date,date,date,date,date,date,date,date'
    bigint_type_str = 'bigint,bigint,bigint,bigint,bigint,bigint'
    int_type_str = 'int,int,int,int,int,int'
    smallint_type_str = 'smallint,smallint,smallint,smallint,smallint,smallint'
    real_type_str = 'real,real,real,real,real'
    float_type_str = 'float,float,float,float,float'
    bool_type_str = 'boolean,boolean'
    varchar_type_str = 'varchar,varchar,varchar'
    bpchar_type_str = 'bpchar,bpchar,bpchar'
    numeric_type_str = 'numeric,numeric,numeric,numeric,numeric,numeric'
    text_type_str = 'text,text,text'
    

    if (DATATYPE == 'regression'):
        print regression(x)
    elif (DATATYPE == 'time'):
        print time_type_str+'\t'+current_line_num+ '\t' +print_time_str(x,'time')
    elif (DATATYPE == 'timestamp'):
        print timestamp_type_str+'\t'+current_line_num+ '\t'+print_time_str(x,'timestamp')
    elif (DATATYPE == 'date'):
        print date_type_str+'\t'+current_line_num+ '\t'+print_time_str(x,'date')
    elif (DATATYPE == 'bigint'):
        print bigint_type_str+'\t'+current_line_num+ '\t'+print_int_str(x,9223372036854775807,-9223372036854775808)
    elif (DATATYPE == 'int'):
        print int_type_str+'\t'+current_line_num+ '\t'+print_int_str(x,2147483647,-2147483648)
    elif (DATATYPE == 'smallint'):
        print smallint_type_str+'\t'+current_line_num+ '\t'+print_int_str(x,32767,-32768)
    elif (DATATYPE == 'real'):
        print real_type_str+'\t'+current_line_num+ '\t'+print_float_str(x, 3.4028235E+38, -3.4028234E+38)
    elif (DATATYPE == 'float'):
        print float_type_str+'\t'+current_line_num+ '\t'+print_float_str(x,+1.797693134862315E+308, -1.797693134862315E+308)
    elif (DATATYPE == 'boolean'):
        print bool_type_str+'\t'+current_line_num+ '\t'+print_bool(x)  
    elif (DATATYPE == 'varchar'):
        print varchar_type_str+'\t'+current_line_num+ '\t'+print_char(x)
    elif (DATATYPE == 'bpchar'):
        print bpchar_type_str+'\t'+current_line_num+ '\t'+print_char(x)
    elif (DATATYPE == 'numeric'):
        print numeric_type_str+'\t'+current_line_num+ '\t'+print_int_str(x, 9223372036854775807000, -9223372036854775808000)
    elif (DATATYPE == 'text'):
        print text_type_str+'\t'+current_line_num+ '\t'+print_char(x)
    elif (DATATYPE == 'all'):
        firstcol = time_type_str+','+ timestamp_type_str+','+date_type_str+','+bigint_type_str+','+int_type_str+','+smallint_type_str+','+real_type_str+','+float_type_str+','+bool_type_str+','+varchar_type_str+','+bpchar_type_str+','+numeric_type_str+','+text_type_str
        print firstcol+'\t'+current_line_num+ '\t' +print_time_str(x,'time')+ '\t' +print_time_str(x,'timestamp')+ '\t' +print_time_str(x,'date')+ '\t' +print_int_str(x,9223372036854775807,-9223372036854775808)+ '\t' +print_int_str(x,2147483647,-2147483648)+ '\t' +print_int_str(x,32767,-32768)+ '\t' +print_float_str(x, 3.4028235E+38, -3.4028234E+38)+ '\t' +print_float_str(x, +1.797693134862315E+308, -1.797693134862315E+308)+ '\t' +print_bool(x)+ '\t' +print_char(x)+ '\t'+print_char(x)+ '\t'+print_int_str(x,9223372036854775807000,-9223372036854775808000)+ '\t'+print_char(x)
    else:
        custom = 'true' 
        print get_custom_type()
            
