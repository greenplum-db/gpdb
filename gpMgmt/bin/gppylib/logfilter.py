#!/usr/bin/env python3
# $Id: $
# $Change: $
# $DateTime: $
# $Author: $
"""
Toolkit for filtering input from GPDB server logs

Module contents:

    ---- High level filters
    FilterLogEntries() - the one-stop filter shop

    ---- Identity filters which observe a stream without altering it
    Count() - count items fetched from a stream
    TimestampSpy() - count lines and note timestamp range

    ---- Grouping and ungrouping filters
    GroupByTimestamp() - group lines with same timestamp
    Ungroup() - decompose groups into lines
    EnumerateUngroup() - number the groups and decompose into lines

    ---- Pattern matching filters
    MatchRegex() - select lines or groups in which a regular expr is matched
    NoMatchRegex() - select lines or groups in which a regular expr has no match
    MatchInFirstLine() - select groups in which regex has a match in first line
    NoMatchInFirstLine() - select groups in which regex doesn't match in first line

    ---- Slicing filters
    Slice() - select items in Pythonesque slice of stream[begin:end]
    FirstNItems() - select the first n items of a stream
    LastNItems() - select the last n items of a finite stream
    SkipNItems() - select all but the first n items of a stream
    SkipLastNItems() - select all but the last n items of a finite stream
    IntersectionOfHeadAndTail() - select items within first m and last n

    ---- Miscellaneous filters
    NotNull() - drop items which are equivalent to False

    ---- Utilities for setting up filter parameters
    filterize() - wrap a filter for use in FilterLogEntries filter list
    spiffInterval() - get begin/end datetime given any subset of begin/end/duration
"""

import io
import csv
from datetime import date, datetime
import re
import sys
import time

csvDelimeter = '|'

timestampPattern = re.compile(r'\d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d(\.\d*)?')
# This pattern matches the date and time stamp at the beginning of a line
# in a GPDB log file.  The timestamp format is: YYYY-MM-DD HH:MM:SS[.frac]
# A timezone specifier may follow the timestamp, but we ignore that.


def FilterLogEntries(iterable,
                     msgfile=sys.stderr,
                     verbose=False,
                     beginstamp=None,
                     endstamp=None,
                     include=None,
                     exclude=None,
                     filters=[],
                     ibegin=0,
                     jend=None):
    """
    Generator to consume the lines of a GPDB log file from iterable,
    yield the lines which satisfy the given criteria, and skip the rest.

    iterable should be a sequence of strings, an already-open input file,
    or some object which supports iteration and yields strings.

    verbose, if True, causes status messages to be written to msgfile,
    which should be an already-open output file.

    For our purposes, a log entry consists of a line which starts with a
    timestamp in YYYY-MM-DD HH:MM:SS[.fraction] format, followed by zero
    or more lines having the same timestamp or no timestamp.

    beginstamp should be a datetime.datetime or datetime.date object, or None.
    Log entries are skipped if their timestamp is less than the specified
    date and time.  Fractional seconds and timezones are ignored.  If a
    date object is given, it is converted to a datetime with time 00:00:00.

    endstamp is like beginstamp, except that it causes log entries to be
    skipped if their timestamp is greater than or equal to the specified
    date and time.

    include should be a regular expression object returned by the re.compile()
    method; or a string specifying a regular expression according to the rules
    of the re package in the Python standard library; or a list of such objects
    and/or strings; or None.  A log entry is skipped if there is an include
    regex which - in every line of the entry - fails to match.

    exclude is like include, except that it causes a log entry to be skipped
    if there is an exclude regex which matches in some line of the entry.

    filters is a sequence of callables.  Each callable will be called just
    once, with one argument: an input stream, which will be an iterator
    yielding groups.  (Here a 'group' is a sequence of strings: the lines
    of a log entry.)  The callable should return an iterator yielding
    filtered groups.  The filters are applied in the order given,
    downstream of the begin/end/include/exclude filters.  For example,
    this filter selects log entries with 'DEBUGn:' in the first line...
        lambda(iterable): MatchInFirstLine(iterable, r'DEBUG\d:')
    The filterize() function, defined later in this module, is useful for
    building the list of filters.

    ibegin and jend should be integers or None.  They can be specified like
    the bounds of a Python slice, to select a subrange of the log entries
    which satisfy all the preceding criteria.  Values >= 0 are counted from
    the beginning of the stream; values < 0 are counted from the end of the
    stream.  0 is before the first qualifying log entry; 1 is after the first
    and before the second; -1 is before the last.  Entries coming before the
    ibegin point or after the jend point are skipped.  For example, jend=3
    to select only the first 3 qualifying log entries; or ibegin=-3 to
    extract the last 3 entries.

    Regular expression syntax is at http://docs.python.org/lib/re-syntax.html

    At the beginning of a log file before the first timestamped line there
    could be some lines with no timestamp.  If beginstamp or endstamp
    is not None, any such lines are skipped.  Otherwise they are grouped
    together and treated as one log entry.
    """
    iterable = iter(iterable)
    spyIn = countIn = spyMid = spyMatch = countOut = None
    if jend is not None and jend == sys.maxsize:
        jend = None

    # Collect unfiltered input statistics
    if verbose:
        iterable = spyIn = TimestampSpy(iterable)

    # Build filter pipeline
    if include or exclude or filters or ibegin or (jend is not None):
        # We want patterns to be tested entry-by-entry rather than line-by-line,
        # so group together the lines of each entry.
        iterable = GroupByTimestamp(iterable)

        # Count the unfiltered log entries
        if verbose:
            iterable = countIn = Count(iterable)

        # Select log entries such that beginstamp <= timestamp < endstamp
        if beginstamp or endstamp:
            iterable = TimestampInBounds(iterable, beginstamp, endstamp)
            if verbose:
                iterable = spyMid = TimestampSpy(iterable)

        # Include matching log entries.
        if (isinstance(include, str) or   # one string
            hasattr(include, 'search')):         # or compiled regex
            include = [include]
        if include:
            for regex in include:
                iterable = MatchRegex(iterable, regex)

        # Exclude non-matching log entries.
        if (isinstance(exclude, str) or   # one string
            hasattr(exclude, 'search')):         # or compiled regex
            exclude = [exclude]
        if exclude:
            for regex in exclude:
                iterable = NoMatchRegex(iterable, regex)

        # Append caller's filters to the pipeline.
        for func in filters:
            iterable = func(iterable)

        # Collect match/filter statistics
        if verbose and iterable is not (spyMid or countIn):
            iterable = spyMatch = TimestampSpy(iterable)

        # After all other filtering, extract slice of qualifying log entries.
        if ibegin or jend is not None:
            iterable = Slice(iterable, ibegin, jend)

        # Count final output log entries
        if verbose:
            iterable = countOut = Count(iterable)

        # Break the groups back down into lines for output.
        iterable = Ungroup(iterable)

        # Collect final statistics
        if verbose:
            iterable = spyOut = TimestampSpy(iterable)

    elif beginstamp or endstamp:
        # Select log entries such that beginstamp <= timestamp < endstamp
        iterable = TimestampInBounds(iterable, beginstamp, endstamp)

        # Collect final statistics
        if verbose:
            iterable = spyOut = spyMid = TimestampSpy(iterable)

    else:
        # Caller didn't request any filtering.
        spyOut = spyIn

    # Pull filtered lines out of the pipeline and yield them to caller
    for line in iterable:
        yield line

    # Display statistics if requested
    if verbose:
        # Did we even try to read any input?
        if spyIn.items == 0 and spyOut.items == 0 and not spyIn.eod:
            print(('%7d lines processed; an unsatisfiable condition '
                              'was specified' % 0), file=msgfile)
            return

        # Unfiltered input statistics
        srange = spyIn.str_range()
        msg = '       in: %7d lines' % spyIn.lines
        if countIn:
            msg += ', %7d log entries' % countIn.count()
        if srange:
            msg += '; timestamps from %s to %s' % srange
        else:
            msg += '; no timestamps found'
        if not spyIn.eod:
            msg += '; stopped before end of input'
        print(msg, file=msgfile)

        # Entries where begin <= timestamp < end
        if spyMid:
            srange = spyMid.str_range()
            msg = '  time ok: %7d lines' % spyMid.lines
            if spyMid.groups:
                msg += ', %7d log entries' % spyMid.groups
            if srange:
                msg += '; timestamps from %s to %s' % srange
            print(msg, file=msgfile)

        # After applying include/exclude/filters
        if spyMatch:
            srange = spyMatch.str_range()
            msg = '    match: %7d lines' % spyMatch.lines
            if spyMatch.groups:
                msg += ', %7d log entries' % spyMatch.groups
            if srange:
                msg += '; timestamps from %s to %s' % srange
            print(msg, file=msgfile)

        # Final output statistics
        srange = spyOut.str_range()
        msg = '      out: %7d lines' % spyOut.lines
        if countOut:
            msg += ', %7d log entries' % countOut.count()
        if srange:
            msg += '; timestamps from %s to %s' % srange
        print(msg, file=msgfile)



#------------------------------- Spying --------------------------------
class CsvFlatten(object):
    """
    Used to flatten a CSV parsed log line into something that looks like the 
    old format.
    """

    def __init__(self,iterable):
        self.source = iter(iterable)
        self.buffer = io.StringIO()
        self.writer = csv.writer(self.buffer, delimiter=csvDelimeter, quotechar='"', quoting=csv.QUOTE_MINIMAL)

    def __iter__(self):
        return self

    def __next__(self):
        item = next(self.source)
        #we need to make a minor format change to the log level field so that
        # our single regex will match both.
        item[16] = item[16] + ": "

        self.buffer.truncate(0)
        self.writer.writerow(item)

        return self.buffer.getvalue()

#------------------------------- Spying --------------------------------

class Count(object):
    """
    Iterator to pass through a stream of items while counting them.

    Count(iterable) -> iterator
        iterable -- a sequence, iterator, file, or other object which
            supports iteration, yielding items of any type.
    """
    def __init__(self, iterable):
        self.source = iter(iterable)
        self.n = 0

    def __iter__(self):
        return self

    def __next__(self):
        item = next(self.source)
        self.n += 1
        return item

    def count(self):
        return self.n


class TimestampSpy(object):
    """
    Iterator to pass through a stream of GPDB log entries while noting the
    lowest and highest timestamps, the number of lines and groups, etc.

    TimestampSpy(iterable) -> iterator
        iterable -- a sequence, iterator, file, or other object which
            supports iteration.  Each item returned by its next() method
            must be either a string (a line of GPDB log data) or a group
            (a sequence of strings where the timestamp, if any, is at the
            beginning of the first string).
    """
    def __init__(self, iterable):
        self.source = iter(iterable)
        self.minstamp = '\xff'
        self.maxstamp = ''
        self.items = 0
        self.lines = 0
        self.groups = 0
        self.eod = False

    def __iter__(self):
        return self

    def __next__(self):
        try:
            item = next(self.source)
        except StopIteration as e:
            self.eod = True
            raise e
        self.items += 1

        if isinstance(item, str):     # ungrouped input
            s = item                         # item is a string
            self.lines += 1
        elif len(item) > 0:                  # grouped input
            s = item[0]                      # item is a sequence of strings
            self.lines += len(item)
            self.groups += 1
        else:                                # item is an empty sequence
            s = ''
            self.groups += 1

        if self.minstamp > s:
            m = timestampPattern.match(s)
            if m:
                self.minstamp = m.group(0)
        if self.maxstamp < s:
            m = timestampPattern.match(s)
            if m:
                self.maxstamp = m.group(0)
        return item

    def str_range(self):
        if self.maxstamp == '':
            return None
        return (self.minstamp, self.maxstamp)

    def datetime_range(self):
        if self.maxstamp == '':
            return None
        minstruct = time.strptime(self.minstamp, '%Y-%m-%d %H:%M:%S')[:6]
        maxstruct = time.strptime(self.maxstamp, '%Y-%m-%d %H:%M:%S')[:6]
        return (datetime(*minstruct), datetime(*maxstruct))


#------------------------------- Grouping --------------------------------

def GroupByTimestamp(iterable, skipnull=True):
    """
    Generator to consume lines of a GPDB log and group them into lists.
    A new group is started whenever the timestamp changes.

    The first call to next() yields a (possibly empty) list of all lines
    found before the first timestamped line.  Subsequently, each call
    yields a list of one or more lines, in which the first line starts
    with a timestamp, and the rest have the same timestamp or no timestamp.

    The inverse of GroupByTimestamp is Ungroup.

    GroupByTimestamp(iterable, skipnull) -> iterator
        iterable -- a sequence, iterator, file, or other object which
            supports iteration.  Each item returned by its next() method
            must be a string.
        skipnull -- True to omit the first group if it is empty.

    Example:
        # Print log entries from stdin having 'ERROR:' in the first line.
        from logfilter import GroupByTimestamp
        from sys import stdin
        for lines in GroupByTimestamp(stdin):
            if lines[0].find('ERROR:'):
                for line in lines:
                    print line
    """
    source = iter(iterable)
    more = True

    # Build list of lines preceding the first timestamped line, and yield
    # it as the first group (could be empty).
    lines = []
    while more:
        try:
            s = next(source)
        except StopIteration:
            more = False
            break
        tsmatch = timestampPattern.match(s)
        if tsmatch:
            break
        lines.append(s)

    if lines or not skipnull:
        yield lines

    while more:
        # Start a new group.  Save timestamp from its first line.
        lines = [s]
        timestamp = tsmatch.group(0)
        tsmatch = None

        # Any more lines with same (or no) timestamp?  Add them to the list.
        while True:
            try:
                s = next(source)
            except StopIteration:            # end of data
                more = False
                break
            if not s.startswith(timestamp):
                tsmatch = timestampPattern.match(s)
                if tsmatch:                  # line has a different timestamp
                    break
            lines.append(s)

        # Current group is finished... output it
        yield lines


def Ungroup(iterable):
    """
    Generator which takes a stream of groups and flattens it by one level,
    yielding the elements of the groups.  Here, the term 'group' means
    a sequence, iterator, or some object which supports iteration.
    Empty groups are skipped.

    Ungroup(iterable) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration.  Each item returned by its next() method must be
            a sequence, iterator, or iterable object.

    Example:
        # Print the numbers from 1 to 7, the list [8, [], 9], and 10
        from gplogfilter import Ungroup
        for i in Ungroup([[1, 2], [3, 4, 5], [], (6, 7), [[8, [], 9], 10]]):
            print i
    """
    for group in iterable:
        for item in group:
            yield item


def EnumerateUngroup(iterable):
    """
    Generator which takes a stream of groups and flattens by one level,
    yielding pairs (i, e) where i is a group counter (from zero) and
    e is an element of the i'th group.  Here, the term 'group' means
    a sequence, iterator, or some object which supports iteration.
    Empty groups are counted and skipped.

    EnumerateUngroup(iterable) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration.  Each item returned by its next() method must be
            a sequence, iterator, or iterable object.
    """
    i = 0
    for group in iterable:
        for item in group:
            yield i, item
        i += 1


#-------------------------------------------------------------------------

def TimestampInBounds(iterable, begin, end):
    """
    Generator to extract GPDB log entries in a datetime interval:
        begin <= timestamp < end

    TimestampInBounds(iterable, begin, end) -> iterator
        iterable -- a sequence, iterator, file, or other object which
            supports iteration.  Each item returned by its next() method
            must be either a string (a line of GPDB log data) or a group
            (a sequence of strings where the timestamp, if any, is at the
            beginning of the first string).
        begin -- a date or datetime giving the lower bound of the interval;
            or None for no lower bound.
        end -- a date or datetime giving the upper bound of the interval;
            or None for no upper bound.

    Example:
        # Print log entries from stdin dated 2008-05-01.
        from logfilter import FilterByTimestamp
        from datetime import date
        from sys import stdin
        for s in TimestampInBounds(stdin, date(2008,5,1), date(2008,5,2)):
            print s

    At the beginning of a log file there may be some lines with no timestamp,
    preceding the first timestamped entry.  Any such lines are skipped.
    """
    # Prepare lower bound
    if begin is None:
        begin = '0000-00-00'
    elif hasattr(begin, 'hour'):
        begin = begin.strftime('%Y-%m-%d %H:%M:%S')   # 'YYYY-MM-DD HH:MM:SS'
    else:
        begin = begin.strftime('%Y-%m-%d')

    # Prepare upper bound
    if end is None:
        end = '9999-99-99'
    elif hasattr(end, 'hour'):
        end = end.strftime('%Y-%m-%d %H:%M:%S')
    else:
        end = end.strftime('%Y-%m-%d')

    # Quit immediately if there cannot be timestamps within the interval.
    if begin >= end:
        return

    # Fetch first item from input stream.
    source = iter(iterable)
    item = next(source)

    # If first item is a string, assume input consists of individual lines.
    # Yield lines which start with a timestamp within the given bounds, plus
    # any following lines which do not have timestamps.
    if isinstance(item, str):
        withinbounds = False
        for item in source:
            if begin <= item < end:
                withinbounds = True
                yield item
            elif timestampPattern.match(item):
                withinbounds = False
            elif withinbounds:
                yield item

    # Else assume input consists of groups (i.e. sequences) of lines.
    # Yield groups in which the first line starts with a timestamp within
    # the given bounds.  Skip groups which are empty or have no timestamp.
    for item in source:
        if (len(item) > 0 and
            begin <= item[0] < end):
            yield item

#--------------------------- Pattern Matching ----------------------------
    
def MatchRegex(iterable, regex):
    """
    Generator to filter a stream, selecting items in which there is a match
    for a regular expression.

    MatchRegex(iterable, include) -> iterator
        iterable -- a sequence, iterator, file, or other object which
            supports iteration.  Each item returned by its next() method
            must be either a string or a group.  Here the term 'group'
            means a sequence of strings or an iterable yielding strings.
        regex -- a regular expression string, or a regular expression
            object returned by re.compile().  The filter excludes items
            in which there is no match for the regex.

    Example:
        # Print lines from stdin which contain the string 'ERROR:'
        from logfilter import MatchRegex
        import re, sys
        for s in MatchRegex(sys.stdin, re.compile('ERROR:')):
            print s
    """
    if isinstance(regex, str):
        regex = re.compile(regex)

    # Yield items in which a match is found for the 'include' pattern.
    for item in iterable:
        if isinstance(item, str):     # item is a string
            if regex.search(item):
                yield item
        else:                                # item is a group of strings
            for s in item:
                if regex.search(s):
                    yield item
                    break


def NoMatchRegex(iterable, regex):
    """
    Generator to filter a stream, selecting items in which there is no match
    for the regular expression.

    NoMatchRegex(iterable, regex) -> iterator
        iterable -- a sequence, iterator, file, or other object which
            supports iteration.  Each item returned by its next() method
            must be either a string or a group.  Here the term 'group'
            means a sequence of strings or an iterable yielding strings.
        regex -- a regular expression string, or a regular expression
            object returned by re.compile().  The filter excludes items
            in which there is a match for the regex.

    Example:
        # Print log entries from stdin which don't contain the string 'HINT'
        from logfilter import FilterNoMatch, GroupByTimestamp, Ungroup
        import sys
        for s in Ungroup(NoMatchRegex(TimestampInBounds(sys.stdin), 'HINT')):
            print s
    """
    if isinstance(regex, str):
        regex = re.compile(regex)

    # Yield items in which no match is found for the 'exclude' pattern.
    for item in iterable:
        if isinstance(item, str):     # item is a string
            if not regex.search(item):
                yield item
        else:                                # item is a group of strings
            for s in item:
                if regex.search(s):
                    break
            else:
                yield item


def MatchInFirstLine(iterable, regex):
    """
    Generator to filter a stream of groups.  Yields those groups whose
    first line contains a match for the given regex.

    MatchInFirstLine(iterable, regex) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration.  Each item returned by its next() method must be
            a group.  Here the term 'group' means a sequence of strings.
        include -- either a string specifying a regular expression, or a
            regular expression object returned by re.compile()

    Example:
        # Print log entries whose first line contains the string 'ERROR:'
        for s in Ungroup(MatchInFirstLine(GroupByTimestamp(sys.stdin), 'ERROR:')):
            print s
    """
    if isinstance(regex, str):
        regex = re.compile(regex)
    for group in iterable:
        if (len(group) > 0 and
            regex.search(group[0])):
            yield group


def NoMatchInFirstLine(iterable, regex):
    """
    Generator to filter a stream of groups.  Skips those groups whose
    first line contains a match for the given regex; yields all other
    groups.

    NoMatchInFirstLine(iterable, regex) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration.  Each item returned by its next() method must be
            a group.  Here the term 'group' means a sequence of strings.
        regex -- either a string specifying a regular expression, or a
            regular expression object returned by re.compile()

    Example:
        # Print log entries whose first line does not contain 'DEBUGn:'
        for s in Ungroup(NoMatchInFirstLine(GroupByTimestamp(sys.stdin), r'DEBUG\d:')):
            print s
    """
    if isinstance(regex, str):
        regex = re.compile(regex)
    for group in iterable:
        if (len(group) == 0 or
            regex.search(group[0]) is None):
            yield group

def MatchColumns(iterable, cols):
    if isinstance(cols, str):
        cols = cols.split(',')
        cols = [int(x) for x in cols]

    # Yield items in which a match is found for the 'include' pattern.
    for item in iterable:
        if 1:
            #print "item\n%s\nitem" % item
            ret = []
            for s in item:
                n = 1
                out = []

                for c in csv.reader(s, delimiter=csvDelimeter, quotechar='"'):
                    if n in cols:
                        out.append(c)
                    n += 1
                if len(out):
                    #print out
                    ret.append(csvDelimeter.join(out) + "\n")
            yield ret

#-------------------------------- Slicing --------------------------------

def Slice(iterable, begin=0, end=None):
    """
    Generator yielding the items of iterable[begin:end], like a Python slice.

    Slice(iterable, begin, end) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration, yielding items of any type.  If begin or end is
            negative, the iteration must be finite.
        begin -- integer or None.  If >=0, number of initial items to skip.
            If <0, skip items preceding this number of final items.
        end -- integer or None.  If >=0, maximum number of items to be
            fetched from the head of the input iterable.  If <0, number
            of items to be excluded at the end of the input stream.
    """
    if begin is None:
        begin = 0
    if begin >= 0:
        iterable = SkipNItems(iterable, begin)
        if end is None or end == sys.maxsize:
            pass
        elif end >= 0:
            iterable = FirstNItems(iterable, end-begin)
        else:
            iterable = SkipLastNItems(iterable, -end)
    elif end is None or end == sys.maxsize:
        iterable = LastNItems(iterable, -begin)
    elif end < 0:
        iterable = LastNItems(iterable, -begin, -end)
    else:
        iterable = IntersectionOfHeadAndTail(iterable, end, -begin)
    return iterable


def FirstNItems(iterable, n):
    """
    Generator yielding the first n items of a stream.

    FirstNItems(iterable, n) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration, yielding items of any type.
        n -- an integer or None

    Example:
        # Read and print 5 lines from standard input
        for s in FirstNItems(sys.stdin, 5):
            print s,
    """
    def FNI(iterable, n):
        source = iter(iterable)
        while n > 0:
            yield next(source)
            n -= 1

    if n is None:
        pass
    elif n <= 0:
        iterable = []
    else:
        iterable = FNI(iterable, n)
    return iterable


def LastNItems(iterable, n, dropLastN=0):
    """
    Generator yielding the final n items of a finite stream.  Of those
    final items, the last dropLastN items are omitted if dropLastN > 0.

    The number of items yielded is max(0, min(n, N) - dropLastN)) where
    N is the number of items yielded by the input iterable.

    LastNItems(iterable, n) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration, yielding items of any type.
        n -- integer or None
        dropLastN -- integer

    Example:
        # Read a file and print its last 5 lines
        f = open(filename1, 'r')
        for line in LastNItems(f, 5):
            print line.rstrip()

        # Read the last 5 lines of a file.  Print them, excluding the last 3.
        f = open(filename2, 'r')
        for line in LastNItems(f, 5, 3):
            print line.rstrip()
    """
    def listOfLastNItems(iterable, n):
        items = []
        for item in iterable:
            if len(items) == n:
                del items[:1]
            items.append(item)
        return items

    def LNI(iterable, n, dropLastN):
        items = listOfLastNItems(iterable, n)
        if dropLastN > 0:
            del items[-dropLastN:]
        while items:
            yield items.pop(0)

    if n is None:
        pass
    elif n <= 0 or n <= dropLastN:
        iterable = []
    else:
        iterable = LNI(iterable, n, dropLastN)
    return iterable


def SkipNItems(iterable, n):
    """
    Generator yielding all but the first n items of a stream.

    SkipNItems(iterable, n) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration, yielding items of any type.
        n -- an integer or None

    Example:
        # Read and print lines 5 and 6 of a file
        f = open(filename, 'r')
        for line in FirstNItems(SkipNItems(f, 4), 2):
            print line.rstrip()
    """
    def SNI(iterable, n):
        source = iter(iterable)
        while n > 0:
            next(source)
            n -= 1
        while True:
            yield next(source)

    if n and n > 0:
        iterable = SNI(iterable, n)
    return iterable


def SkipLastNItems(iterable, n):
    """
    Generator yielding all but the final n items of a finite stream.

    SkipLastNItems(iterable, n) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration, yielding items of any type.
        n -- an integer or None
    """
    def SLNI(iterable, n):
        items = list(iterable)[:-n]
        while items:
            yield items.pop(0)

    if n and n > 0:
        iterable = SLNI(iterable, n)
    return iterable


def IntersectionOfHeadAndTail(iterable, nhead, ntail):
    """
    Generator yielding those items of a finite stream which belong to both
    the first 'nhead' items and the last 'ntail' items of the stream.
    """
    def IHT(iterable, nhead, ntail):
        items = []
        n = 0
        for item in iterable:
            if n < nhead:
                items.append(item)
            if n >= ntail:
                del items[:1]
                if not items:
                    break
            n += 1
        while items:
            yield items.pop(0)

    if nhead <= 0 or ntail <= 0:
        iterable = []
    else:
        iterable = IHT(iterable, nhead, ntail)
    return iterable


#------------------------ Miscellaneous Filters ------------------------

def NotNull(iterable):
    """
    Generator to filter out items which are equivalent to False, such as
    empty groups.

    NotNull(iterable) -> iterator
        iterable -- a sequence, iterator, or some object which supports
            iteration, yielding items of any type.
    """
    return (item for item in iterable if item)


#-------------------------- Utility Functions --------------------------

def filterize(Filter, *args, **kwargs):
    """
    Return a function of one argument (an input stream) which, when called,
    will return the result of applying the given Filter function to that
    argument together with any additional args and kwargs.

    This is useful for building a 'filters' list to be passed to FilterLogFile.

    Example:
        # Print log entries containing the string 'Detail:'
        from logfilter import FilterLogEntries, MatchRegex, filterize
        import sys
        filters = []
        filters.append(filterize(MatchRegex, 'Detail:'))
        for line in FilterLogFile(sys.stdin, filters=filters):
            print line.rstrip()
    """
    if args or kwargs:
        return lambda stream: Filter(stream, *args, **kwargs)
    else:
        return Filter


def spiffInterval(begin=None, end=None, duration=None):
    """
    Determine a datetime interval given zero or more of the parameters
    begin, end, duration.

    The begin and end parameter values should be instances of the
    datetime.datetime or datetime.date class, or None.  Any dates
    (datetime.date) are converted to datetimes (datetime.datetime)
    with time set to 00:00:00.

    The duration parameter value should be an instance of the
    datetime.timedelta class, or None.  The duration is used to
    calculate a missing endpoint in case begin or end is None.
    The duration parameter is ignored if the caller specifies
    both begin and end.

    Returns a pair (begin, end) in which each element is either
    an instance of the datetime.datetime class or None.
    """
    if begin and not hasattr(begin, 'hour'):
        begin = datetime(begin.year, begin.month, begin.day)
    if end and not hasattr(end, 'hour'):
        end = datetime(end.year, end.month, end.day)

    if (begin is None or end is None) and duration is not None:
        if begin:
            end = begin + duration
        elif end:
            begin = end - duration
        else:
            # Neither begin nor end was given.  Let default interval
            # begin at the current date and time minus the duration.
            # Let the interval remain unbounded at the high end.
            begin = datetime.now() - duration

    return begin, end

