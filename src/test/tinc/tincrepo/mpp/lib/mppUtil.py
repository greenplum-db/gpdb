"""
Copyright (c) 2004-Present Pivotal Software, Inc.

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

import errno
import os
import socket
import warnings
import string

"""
This file contains miscellaneous utility for mpp testing.
"""

def getOpenPort(port=8080, host='localhost', user=os.environ.get('USER')):
    '''
    host: the server where an open port will be retrieved from
    user: the user which has done gpssh-exkeys within the cluster
    port: a given base port number to calculate, if failed, plus one until pass or checked all
    Author: Johnny, modified by Jason
    '''
    # first 1000 ports are reserved for specific applications
    # max port number is 65535
    if port <= 1000 or port > 65535:
        warnings.warn("Warning: BasePort is not a valid one, ignoring, try default port: 8080!")
        port = 8080
    for portNum in range(port, 65336):
        try:
            myClientSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            myClientSocket.connect((host,portNum))
            myClientSocket.close()
        except socket.error as serr:
            myClientSocket.close()
            if serr.errno == errno.ECONNREFUSED:
                return portNum
    raise Exception('Faild to find any available ports')

def hasExpectedStr( out, expectedStr ):
    """
    Check whether the output contains the expected string.
    
    @type  out:         list
    @param out:         The command output
    @type  expectedStr: string
    @param expectedStr: The expected string
    @rtype:             bool
    @return:            True if the output contains the expected string.
                        False otherwise.
    """        
    for line in out:
        if string.find( line, expectedStr ) >= 0:
            return True
    return False



def replaceStringInFile(path, substList, baseFileName, fileExt1, fileExt2 ): 
    """
    PURPOSE: Replace specified strings in a file.
        E.g. read foo.ans.t, replace "%PATH%" with PATH, 
        then write the file out as foo.ans (without the ".t"). 
    INPUTS:
        substList: a list of tuples, where the first value in the tuple is 
            the text that you want to search for and replace, and the second 
            value is the replacement value e.g. 
               [ ("%datatype%", "CHAR(2)"), 
                 ("%PATH%", "/export/home/build/cdbfast/partition") ]
        baseFileName: The name (including the path) of the file that you 
            want to replace text in, minus the extensions.  E.g. if the 
            file is "query1.ans.t" then baseFileName is "query1".  
        fileExt1: the first file extension (e.g. ".ans" in "query1.ans.t").
        fileExt2: the second (and last) file extension (e.g. ".t" in 
            "query1.ans.t").
    WARNINGS:
        1) !!! Ought to add error handling.
    """

    # The template (.t) file that we read from.
    fromFile = path + os.sep + baseFileName + fileExt1 + fileExt2
    # The file that we write to.
    toFile = path + os.sep + baseFileName + fileExt1
    if os.path.exists( fromFile ):
        input = open( fromFile )
        output = open( toFile, 'w' )
        for s in input.xreadlines():
            for t in substList:
                From = t[0]
                To = t[1]
                s = string.replace( s, From, To )
            output.write( s )
        output.close()
    else:
        print "---------"
        print os.getcwd()
        print path, baseFileName, fileExt1, fileExt2
        print "---------"


def generateFileFromEachTemplate(path, listOfTemplateFiles, listOfSubstitutions ):
    """
    PURPOSE: Given a list of template files, update them.
    INPUTS:
        path: The path/directory that contains the .t files.
        listOfTemplateFiles: A list of the files to update, for example:
        [ "query02.sql.t", "query02.ans.t" ]
        listOfSubstitutions: A list of tuples, where each tuple contains 
        the string to search for and the search to replace it with, 
        for example:
                [ ("%PATH%", path), ("%username%", userName) ]
    """

    for fileName in listOfTemplateFiles:
        # Save and remove the last extension (".t").
        ext2 = fileName[-2:]
        fileName = fileName[0:-2]
        # Save and remove the second-to-last extension (e.g. ".ans").
        position = string.rfind( fileName, "." )
        ext1 = fileName[position:]
        fileName = fileName[0:position]
        replaceStringInFile( path, listOfSubstitutions, fileName, ext1, ext2 )
