import shutil, filecmp,re
import os, fcntl, select, getpass, socket
import stat
from subprocess import *
from sys import *
from xml.dom import minidom
from xml.dom import Node

from gppylib.gplog import *

logger = get_default_logger()

_debug=0
#############
class ParseError(Exception):
    def __init__(self,parseType):
        self.msg = ('%s parsing error'%(parseType))
    def __str__(self):
        return self.msg

#############
class RangeError(Exception):
    def __init__(self, value1, value2):
        self.msg = ('%s must be less then %s' % (value1, value2))
    def __str__(self):
        return self.msg

#############
def createFromSingleHostFile(inputFile):
    """TODO: """
    rows=[]
    f = open(inputFile, 'r')
    for line in f:
      rows.append(parseSingleFile(line))
    
    return rows


#############
def toNonNoneString(value) :
    if value is None:
        return ""
    return str(value)

#
# if value is None then an exception is raised
#
# otherwise value is returned
#
def checkNotNone(label, value):
    if value is None:
        raise Exception( label + " is None")
    return value

#
# value should be non-None
#
def checkIsInt(label, value):
    if type(value) != type(0):
        raise Exception( label + " is not an integer type" )

def isNone( value):
    isN=False
    if value is None:
        isN=True 
    elif value =="":
        isN= True
    return isN 

def readAllLinesFromFile(fileName, stripLines=False, skipEmptyLines=False):
    """
    @param stripLines if true then line.strip() is called on each line read
    @param skipEmptyLines if true then empty lines are not returned.  Beware!  This will throw off your line counts
                             if you are relying on line counts
    """
    res = []
    f = open(fileName)
    try:
        for line in f:
            if stripLines:
                line = line.strip()

            if skipEmptyLines and len(line) == 0:
                # skip it!
                pass
            else:
                res.append(line)
    finally:
        f.close()
    return res

def writeLinesToFile(fileName, lines):
    f = open(fileName, 'w')
    try:
        for line in lines:
            f.write(line)
            f.write('\n')
    finally:
        f.close()

#############
def parseSingleFile(line):
    ph=None 
    if re.search(r"^#", line):
        #skip it, it's a comment
        pass
    else:
        ph=line.rstrip("\n").rstrip()  
    return ph

def openAnything(source):            
    """URI, filename, or string --> stream

    This function lets you define parsers that take any input source
    (URL, pathname to local or network file, or actual data as a string)
    and deal with it in a uniform manner.  Returned object is guaranteed
    to have all the basic stdio read methods (read, readline, readlines).
    Just .close() the object when you're done with it.
    
    Examples:
    >>> from xml.dom import minidom
    >>> sock = openAnything("http://localhost/kant.xml")
    >>> doc = minidom.parse(sock)
    >>> sock.close()
    >>> sock = openAnything("c:\\inetpub\\wwwroot\\kant.xml")
    >>> doc = minidom.parse(sock)
    >>> sock.close()
    >>> sock = openAnything("<ref id='conjunction'><text>and</text><text>or</text></ref>")
    >>> doc = minidom.parse(sock)
    >>> sock.close()
    """
    if hasattr(source, "read"):
        return source

    if source == '-':
        import sys
        return sys.stdin

    # try to open with urllib (if source is http, ftp, or file URL)
    import urllib                         
    try:                                  
        return urllib.urlopen(source)     
    except (IOError, OSError):            
        pass                              
    
    # try to open with native open function (if source is pathname)
    try:                                  
        return open(source)               
    except Exception, e: 
        print ("Exception occurred opening file %s Error: %s"  % (source, str(e)))                             
        
    
    # treat source as string
    import StringIO                       
    return StringIO.StringIO(str(source)) 
def getOs():
    dist=None
    fdesc = None
    RHId = "/etc/redhat-release"
    SuSEId = "/etc/SuSE-release"
    try: 
        fdesc = open(RHId)
        for line in fdesc: 
            line = line.rstrip()   
            if re.match('CentOS', line):
                dist = 'CentOS' 
            if re.match('Red Hat', line):
                dist = 'CentOS' 
    except IOError:
        pass
    finally:
        if fdesc :
            fdesc.close()
    try: 
        fdesc = open(SuSEId)
        for line in fdesc: 
            line = line.rstrip()   
            if re.match('SUSE', line):
                dist = 'SuSE' 
    except IOError:
        pass
    finally:
        if fdesc : 
            fdesc.close()
    return dist
def factory(aClass, *args):
    return apply(aClass,args)

def addDicts(a,b):
    c = dict(a)
    c.update(b)
    return c

def joinPath(a,b,parm=""):
    c=a+parm+b 
    return c

def debug(varname, o):
    if _debug == 1:
        print "Debug: %s -> %s" %(varname, o)

def loadXmlElement(config,elementName):
    fdesc = openAnything(config)
    xmldoc = minidom.parse(fdesc).documentElement
    fdesc.close()
    elements=xmldoc.getElementsByTagName(elementName) 
    return elements

def docIter(node):
    """
        Iterates over each node in document order, returning each in turn
    """
    #Document order returns the current node,
    #then each of its children in turn
    yield node
    if node.nodeType == Node.ELEMENT_NODE:
        #Attributes are stored in a dictionary and
        #have no set order. The values() call
        #gets a list of actual attribute node objects
        #from the dictionary
        for attr in node.attributes.values():
            yield attr
    for child in node.childNodes:
        #Create a generator for each child,
        #Over which to iterate
        for cn in docIter(child):
            yield cn
    return

def makeNonBlocking(fd):
    fl = fcntl.fcntl(fd, fcntl.F_GETFL)
    try:
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NDELAY)
    except IOError:
        fcntl.fcntl(fd, fcntl.F_SETFL, fl | os.O_NDELAY)


def getCommandOutput(command):
    child = os.popen(command)
    data = child.read( )
    err = child.close( ) 
    #if err :
    #    raise RuntimeError, '%r failed with exit code %d' % (command, err)
    return ''.join(data)


def touchFile(fileName):
    if os.path.exists(fileName):
            os.remove(fileName)
    fi=open(fileName,'w')
    fi.close()

def deleteBlock(fileName,beginPattern, endPattern):
    #httpdConfFile="/etc/httpd/conf/httpd.conf"
    fileNameTmp= fileName +".tmp"
    if beginPattern is None :
        beginPattern = '#gp begin'

    if endPattern is None :
        endPattern = '#gp end'

    beginLineNo = 0
    endLineNo = 0
    lineNo =1

    #remove existing gp existing entry
    if os.path.isfile(fileName):
        try:
            fdesc = open(fileName)
            lines = fdesc.readlines()
            fdesc.close()
            for line in lines:
                line = line.rstrip()
                if re.match(beginPattern, line):
                    beginLineNo = lineNo
                    #print line
                    #print beginLineNo
                if re.match(endPattern, line) and (beginLineNo != 0):
                    endLineNo = lineNo
                    #print endLineNo
                lineNo += 1
                #print lines[beginLineNo-1:endLineNo]
                del lines[beginLineNo-1:endLineNo]
                fdesc = open(fileNameTmp,"w")
                fdesc.writelines(lines)
                fdesc.close()
                os.rename(fileNameTmp,fileName)
        except IOError:
            print("IOERROR", IOError)
            sys.exit()
    else:
        print "***********%s  file does not exits"%(fileName)

def make_inf_hosts(hp, hstart, hend, istart, iend, hf=None):
    hfArr = []
    inf_hosts=[]
    if None != hf:
        hfArr=hf.split('-')
    print hfArr 
    for h in range(int(hstart), int(hend)+1):
        host = '%s%d' % (hp, h)
        for i in range(int(istart), int(iend)+1):
            if i != 0 :
                inf_hosts.append('%s-%s' % (host, i))
            else:
                inf_hosts.append('%s' % (host))
    return inf_hosts

def copyFile(srcDir,srcFile, destDir, destFile):
    result=""
    filePath=os.path.join(srcDir, srcFile)
    destPath=os.path.join(destDir,destFile)
    if not os.path.exists(destDir):
        os.makedirs(destDir)
    try:
        if os.path.isfile(filePath):
            #debug("filePath" , filePath)
            #debug("destPath" , destPath)
            pipe=os.popen("/bin/cp -avf  " +filePath +" "+destPath)
            result=pipe.read().strip()
            #debug ("result",result)
        else:
            print "no such file or directory " + filePath
    except OSError:
        print ("OS Error occurred")
    return result

def parseKeyColonValueLines(str):
    """
    Given a   string contain   key:value  lines, parse the lines and return a map of key->value
    Returns None if there was a problem parsing
    """
    res = {}
    for line in str.split("\n"):
        line = line.strip()
        if line == "":
            continue
        colon = line.find(":")
        if colon == -1:
            logger.warn("Error parsing data, no colon on line %s" % line)
            return None
        key = line[:colon]
        value = line[colon+1:]
        res[key] = value
    return res


def sortedDictByKey(di):
    return  [ (k,di[k]) for k in sorted(di.keys())]

def appendNewEntriesToHbaFile(fileName, segments):
    """
    Will raise Exception if there is a problem updating the hba file
    """

    try:
        #
        # Get the list of lines that already exist...we won't write those again
        #
        # Replace runs of whitespace with single space to improve deduping
        #
        def lineToCanonical(s):
            s = re.sub("\s", " ", s) # first reduce whitespace runs to single space
            s = re.sub(" $", "", s) # remove trailing space
            s = re.sub("^ ", "", s) # remove leading space
            return s
        existingLineMap = {}
        for line in readAllLinesFromFile(fileName):
            existingLineMap[lineToCanonical(line)] = True

        fp = open(fileName, 'a')
        try:
            for newSeg in segments:
                address = newSeg.getSegmentAddress()
                addrinfo = socket.getaddrinfo(address, None)
                ipaddrlist = list(set([ (ai[0], ai[4][0]) for ai in addrinfo]))
                haveWrittenCommentHeader = False
                for addr in ipaddrlist:
                    newLine = 'host\tall\tall\t%s/%s\ttrust' % (addr[1], '32' if addr[0] == socket.AF_INET else '128')
                    newLineCanonical = lineToCanonical(newLine)
                    if newLineCanonical not in existingLineMap:
                        if not haveWrittenCommentHeader:
                            fp.write('# %s\n' % address)
                            haveWrittenCommentHeader = True
                        fp.write(newLine)
                        fp.write('\n')
                        existingLineMap[newLineCanonical] = True
        finally:
            fp.close()
    except IOError, msg:
        raise Exception('Failed to open %s' % fileName)
    except Exception, msg:
        raise Exception('Failed to add new segments to template %s' % fileName)

class TableLogger:

    """
    Use this by constructing it, then calling warn, info, and infoOrWarn with arrays of columns, then outputTable
    """

    def __init__(self, logger=get_default_logger()):
        self.__lines = []
        self.__warningLines = {}
        self.logger = logger

        #
        # If True, then warn calls will produce arrows as well at the end of the lines
        # Note that this affects subsequent calls to warn and infoOrWarn
        #
        self.__warnWithArrows = False

    def setWarnWithArrows(self, warnWithArrows):
        """
        Change the "warn with arrows" behavior for subsequent calls to warn and infoOrWarn

        If warnWithArrows is True then warning lines are printed with arrows at the end

        returns self
        """
        self.__warnWithArrows = warnWithArrows
        return self

    def warn(self, line):
        """
        return self
        """
        self.__warningLines[len(self.__lines)] = True

        line = [s for s in line]
        if self.__warnWithArrows:
            line.append( "<<<<<<<<")
        self.__lines.append(line)

        return self

    def info(self, line):
        """
        return self
        """
        self.__lines.append([s for s in line])
        return self


    def infoOrWarn(self, warnIfTrue, line):
        """
        return self
        """
        if warnIfTrue:
            self.warn(line)
        else: self.info(line)
        return self

    def outputTable(self):
        """
        return self
        """
        lines = self.__lines
        warningLineNumbers = self.__warningLines

        lineWidth = []
        for line in lines:
            if line is not None:
                while len(lineWidth) < len(line):
                    lineWidth.append(0)

                for i, field in enumerate(line):
                    lineWidth[i] = max(len(field), lineWidth[i])

        # now print it all!
        for lineNumber, line in enumerate(lines):
            doWarn = warningLineNumbers.get(lineNumber)

            if line is None:
                #
                # separator
                #
                self.logger.info("----------------------------------------------------")
            else:
                outLine = []
                for i, field in enumerate(line):
                    if i == len(line) - 1:
                        # don't pad the last one since it's not strictly needed,
                        # and we could have a really long last column for some lines
                        outLine.append(field)
                    else:
                        outLine.append(field.ljust(lineWidth[i] + 3))
                msg = "".join(outLine)

                if doWarn:
                    self.logger.warn(msg)
                else:
                    self.logger.info("   " + msg) # add 3 so that lines will line up even with the INFO and WARNING stuff on front

        return self

    def addSeparator(self):
        self.__lines.append(None)

    def getNumLines(self):
        return len(self.__lines)

    def getNumWarnings(self):
        return len(self.__warningLines)

    def hasWarnings(self):
        return self.getNumWarnings() > 0

class ParsedConfigFile:
    """
    returned by call to parseMirroringConfigFile
    """

    def __init__( self, flexibleHeaders, rows):
        self.__flexibleHeaders = flexibleHeaders
        self.__rows = rows

    def getRows(self):
        """
        @return a non-None list of ParsedConfigFileRow
        """
        return self.__rows

    def getFlexibleHeaders(self):
        """
        @return a non-None list of strings
        """
        return self.__flexibleHeaders

class ParsedConfigFileRow:
    """
    used as part of ParseConfigFile, returned by call to parseMirroringConfigFile
    """

    def __init__(self, fixedValuesMap, flexibleValuesMap, line):
        self.__fixedValuesMap = fixedValuesMap
        self.__flexibleValuesMap = flexibleValuesMap
        self.__line = line

    def getFixedValuesMap(self):
        """
        @return non-None dictionary
        """
        return self.__fixedValuesMap

    def getFlexibleValuesMap(self):
        """
        @return non-None dictionary
        """
        return self.__flexibleValuesMap

    def getLine(self):
        """
        @return the actual line that produced this config row; can be used for error reporting
        """
        return self.__line

def parseMirroringConfigFile( lines, fileLabelForExceptions, fixedHeaderNames, keyForFlexibleHeaders,
                                linesWillHaveLineHeader, numberRequiredHeadersForRecoversegFormat=None):
    """
    Read a config file that is in the mirroring or recoverseg config format

    @params lines the list of Strings to parse
    @param staticHeaders a list of Strings, listing what should appear as the first length(staticHeaders)
                        values in each row
    @param keyForFlexibleHeaders if None then no extra values are read, otherwise it's the key for flexible
                        headers that should be passed.  If this is passed then the first line of the file
                        should look like keyValue=a1:a2:...a3
    @param numberRequiredHeadersForRecoversegFormat if not None then the line can be either this
                        many elements from fixedHeaderNames, or that many elements then a space separator and
                        then the remaining required ones.  If we consolidate formats then we could remove
                        this hacky option
    @return a list of values

    todo: should allow escaping of colon values, or switch to CSV and support CSV escaping
    """
    lines = [s.strip() for s in lines if len(s.strip()) > 0]

    # see if there is the flexible header
    rows = []
    flexibleHeaders = []
    if keyForFlexibleHeaders is not None:
        if len(lines) == 0:
            raise Exception("Missing header line with %s= values specified" % keyForFlexibleHeaders )

        flexHeaderLineSplit = lines[0].split("=")
        if len(flexHeaderLineSplit) != 2 or flexHeaderLineSplit[0] != keyForFlexibleHeaders:
            raise Exception('%s format error for first line %s' % (fileLabelForExceptions, lines[0]))

        str = flexHeaderLineSplit[1].strip()
        if len(str) > 0:
            flexibleHeaders = str.split(":")

        lines = lines[1:]

    # now read the real lines
    numExpectedValuesPerLine = len(fixedHeaderNames) + len(flexibleHeaders)
    for line in lines:
        origLine = line

        if linesWillHaveLineHeader:
            arr = line.split("=")
            if len(arr) != 2:
                raise Exception('%s format error for line %s' % (fileLabelForExceptions, line))
            line = arr[1]

        numExpectedThisLine = numExpectedValuesPerLine
        fixedToRead = fixedHeaderNames
        flexibleToRead = flexibleHeaders
        if numberRequiredHeadersForRecoversegFormat is not None:
            arr = line.split()
            if len(arr) == 1:
                numExpectedThisLine = numberRequiredHeadersForRecoversegFormat
                fixedToRead = fixedHeaderNames[0:numberRequiredHeadersForRecoversegFormat]
                flexibleToRead = []
            elif len(arr) == 2:
                # read the full ones, treat it like one big line
                line = arr[0] + ":" + arr[1]
            else: raise Exception('config file format error. %s' % line)

        arr = line.split(":")
        if len(arr) != numExpectedThisLine:
            raise Exception('%s format error for line (wrong number of values.  '
                            'Found %d but expected %d) : %s' %
                            (fileLabelForExceptions, len(arr), numExpectedThisLine, line))

        fixedValuesMap = {}
        flexibleValuesMap = {}

        index = 0
        for name in fixedToRead:
            fixedValuesMap[name] = arr[index]
            index += 1
        for name in flexibleToRead:
            flexibleValuesMap[name] = arr[index]
            index += 1

        rows.append(ParsedConfigFileRow(fixedValuesMap, flexibleValuesMap, origLine))

    return ParsedConfigFile( flexibleHeaders, rows)

def createSegmentSpecificPath(path, gpPrefix, segment):
    """
    Create a segment specific path for the given gpPrefix and segment

    @param gpPrefix a string used to prefix directory names
    @param segment a Segment value
    """
    return os.path.join(path, '%s%d' % (gpPrefix, segment.getSegmentContentId()))

class PathNormalizationException(Exception):
    pass

def normalizeAndValidateInputPath(path, errorMessagePathSource=None, errorMessagePathFullInput=None):
    """
    Raises a PathNormalizationException if the path is not an absolute path.  The exception msg will use
        errorMessagePathSource and errorMessagePathFullInput to build the error message.

    Does not check that the path exists

    @param errorMessagePathSource from where the path was read such as "by user", "in file"
    @param errorMessagePathFullInput the full input (line, for example) from which the path was read; for example,
              if the path is part of a larger line of input read then you can pass the full line here

    """
    path = path.strip()
    if not os.path.isabs(path):
        firstPart = " " if errorMessagePathSource is None else " " + errorMessagePathSource + " "
        secondPart = "" if errorMessagePathFullInput is None else " from: %s" % errorMessagePathFullInput
        raise PathNormalizationException("Path entered%sis invalid; it must be a full path.  Path: '%s'%s" %
                ( firstPart, path, secondPart ))
    return os.path.normpath(path)

def canStringBeParsedAsInt(str):
    """
    return True if int(str) would produce a value rather than throwing an error,
          else return False
    """
    try:
        int(str)
        return True
    except ValueError:
        return False

def shellEscape(string):
    """
    shellEscape: Returns a string in which the shell-significant quoted-string characters are
    escaped.
    This function escapes the following characters: '"', '$', '`', '\', '!'
    """
    res = []
    for ch in string:
        if ch in ['\\', '`', '$', '!', '"']:
            res.append('\\')
        res.append(ch)
    return ''.join(res)
