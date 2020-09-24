#!/usr/bin/env python3
# $Id: $
# $Change: $
# $DateTime: $
# $Author: $
"""
Date and time input conversion

Module contents:

    ---- common
    class DatetimeValueError - subclass of ValueError

    ---- datetime or date
    datetime_syntax_msg - a string briefly describing the input format
    str_to_datetime() - Interprets a string as a date and (optional) time.
    scan_datetime() - Consumes a date and time from a string.

    ---- timedelta
    signed_duration_syntax_msg - a string briefly describing the input format
    unsigned_duration_syntax_msg - a string briefly describing the input format
    str_to_duration() - Interprets a string as a duration or length of time.
    scan_duration() - Consumes a duration from a string.

Examples:
    from lib.datetimeutils import str_to_datetime
    import datetime
    dt = str_to_datetime('2008-07-13 14:15:16.1')
    dt = str_to_datetime('20080713 141516.123456')
    dt = str_to_datetime('  20080713T1415  ')
    d = str_to_datetime('2008-7-4')

    from lib.datetimeutils import str_to_duration
    import timedelta
    td = str_to_duration('1')            # one hour
    td = str_to_duration('  1:2:3  ')    # one hour, two minutes and three seconds
    td = str_to_duration(':2:3.4')       # two minutes and 3.4 seconds
    td = str_to_duration('-72')          # negative 72 hours (normalized to -3 days)

The datetime syntax is based on ISO 8601 and RFC 3339.  See...
    http://www.rfc.net/rfc3339.html
    http://hydracen.com/dx/iso8601.htm
    http://www.cl.cam.ac.uk/~mgk25/iso-time.html

In the docstrings, the examples preceded by '>>>' can be executed and verified
automatically by the 'doctest' package of the Python standard library.
"""

from datetime import date, datetime, timedelta
import re


#--------------------------------- common ---------------------------------

class DatetimeValueError (ValueError):
    """
    Error on conversion from string to date, datetime, or timedelta.

    DatetimeValueError fields:
        description
        pos
        endpos
        badness
    """
    def __init__(self, description, string, pos=None, endpos=None):
        # Save enough info in the exception object so that a caller's
        # exception handler can create a differently formatted message
        # in case they don't like the format we provide.
        global _spacepat
        if endpos is None:
            endpos = len(string or '')
        if pos and string:
            # avoid copying all of string[pos:endpos], in case it is big
            p = _spacepat.match(string, pos, endpos).end()
            if endpos > p+30:
                badness = string[p:p+27].rstrip(' \t') + '...'
            else:
                badness = string[p:endpos].rstrip(' \t')
        else:
            badness = string
        self.description, self.pos, self.endpos, self.badness = description, pos, endpos, badness
        if badness:
            description = '"%s" ... %s' % (badness, description)
        ValueError.__init__(self, description)



#-------------------------------- datetime --------------------------------

datetime_syntax_msg = ('Specify date and time as "YYYY-MM-DD[ HH:MM[:SS[.S]]" '
                       'or "YYYYMMDD[ HHMM[SS[.S]]]".  If both date and time '
                       'are given, a space or letter "T" must separate them.')

def str_to_datetime(string, pos=0, endpos=None):
    """
    Interprets string[pos:endpos] as a date and (optional) time.

    Returns a datetime.datetime object if string[pos:endpos] contains a valid
    date and time.  Returns a datetime.date object for a date with no time.

    Raises DatetimeValueError if string does not contain a valid date, or
    contains an invalid time or anything else other than whitespace.

    Examples...

    # delimited format
    >>> str_to_datetime('2008-7-13')
    datetime.date(2008, 7, 13)
    >>> str_to_datetime('2008-7-13 2:15')
    datetime.datetime(2008, 7, 13, 2, 15)
    >>> str_to_datetime(' 2008-07-13  14:15:16.123456789 ')
    datetime.datetime(2008, 7, 13, 14, 15, 16, 123456)

    # numeric format
    >>> str_to_datetime('  20080713  ')
    datetime.date(2008, 7, 13)
    >>> str_to_datetime('20080713 141516.123')
    datetime.datetime(2008, 7, 13, 14, 15, 16, 123000)

    # slicing
    >>> str_to_datetime('9200807139', 1, 9)
    datetime.date(2008, 7, 13)

    # 'T' can separate date and time
    >>> str_to_datetime('2008-7-3t2:15')
    datetime.datetime(2008, 7, 3, 2, 15)
    >>> str_to_datetime('  20080713T1415 ')
    datetime.datetime(2008, 7, 13, 14, 15)

    Errors...  (for more, see scan_datetime() below)

    >>> str_to_datetime('')
    Traceback (most recent call last):
    DatetimeValueError: Specify date and time as "YYYY-MM-DD[ HH:MM[:SS[.S]]" or "YYYYMMDD[ HHMM[SS[.S]]]".  If both date and time are given, a space or letter "T" must separate them.
    >>> str_to_datetime('nogood')
    Traceback (most recent call last):
    DatetimeValueError: "nogood" ... Specify date and time as "YYYY-MM-DD[ HH:MM[:SS[.S]]" or "YYYYMMDD[ HHMM[SS[.S]]]".  If both date and time are given, a space or letter "T" must separate them.
    >>> str_to_datetime(' 20080713 T1415 ')      # can't have spaces around 'T'
    Traceback (most recent call last):
    DatetimeValueError: " 20080713 T1415 " ... date is followed by unrecognized "T1415"
    >>> str_to_datetime('2008-7-13 2:15 &')
    Traceback (most recent call last):
    DatetimeValueError: "2008-7-13 2:15 &" ... date and time are followed by unrecognized "&"
    >>> str_to_datetime('2008-7-13 abcdefghijklmnop')
    Traceback (most recent call last):
    DatetimeValueError: "2008-7-13 abcdefghijklmnop" ... date is followed by unrecognized "abcdefghi"
    """
    global datetime_syntax_msg
    if endpos is None:
        endpos = len(string)
    value, nextpos = scan_datetime(string, pos, endpos)
    if value is None:
        # string is empty or doesn't conform to the date syntax we require
        raise DatetimeValueError(datetime_syntax_msg, string, pos, endpos)
    elif nextpos < endpos and not string[nextpos:endpos].isspace():
        # got valid date or datetime, but there is something more after it
        if hasattr(value, 'hour'):
            msg = 'date and time are followed by unrecognized "%s"'
        else:
            msg = 'date is followed by unrecognized "%s"'
        msg %= string[nextpos:min(nextpos+10,endpos)].strip()
        raise DatetimeValueError(msg, string, pos, endpos)
    return value


def scan_datetime(string, pos=0, endpos=None):
    """
    Consumes an initial substring of the slice string[pos:endpos] which
    represents a date and (optional) time.  Leading whitespace is ignored.

    Raises DatetimeValueError if the text syntactically resembles a date or
    datetime but fails semantic checks (e.g. field out of range, such as the
    day in '2008-02-30').

    Otherwise returns a tuple (value, nextpos) in which 'value' is either a
    datetime.datetime object, a datetime.date object, or None; and 'nextpos'
    is the index of the next character of string (pos <= nextpos <= endpos).

    (None, pos) is returned if the beginning of the string (or slice) does
    not conform to either the delimited date format YYYY-[M]M-[D]D or the
    numeric date format YYYYMMDD.

    (dt, nextpos) in which dt is a datetime.datetime object, is returned if
    a valid date and time are found.  At least one space or tab, or else the
    single letter 'T' or 't' with no whitespace, must separate date and time.
    Time must be delimited using colons [H]H:MM:[SS[.fraction]] if the
    date is delimited, or must be numeric HHMM[SS[.fraction]] if the date is
    numeric.  The fraction may be any number of digits (at most 6 are kept).

    (d, nextpos) in which d is a datetime.date object, is returned if a
    valid date is found but the subsequent characters do not syntactically
    resemble a time in the required format.

    Examples...  (for other syntax examples, see str_to_datetime() above)

    >>> scan_datetime('2008-6-30')
    (datetime.date(2008, 6, 30), 9)
    >>> scan_datetime(' 2008-07-13 14:15:16  ')
    (datetime.datetime(2008, 7, 13, 14, 15, 16), 20)
    >>> scan_datetime(' nogood ')
    (None, 0)

    Errors...

    >>> scan_datetime('2008-06-31')
    Traceback (most recent call last):
    DatetimeValueError: "2008-06-31" ... day is out of range for month
    >>> scan_datetime('2008-07-13 24:00')
    Traceback (most recent call last):
    DatetimeValueError: "2008-07-13 24:00" ... hour must be in 0..23
    >>> scan_datetime('2008-07-13 14:15:1  ')
    Traceback (most recent call last):
    DatetimeValueError: "2008-07-13 14:15:1  " ... second should have 2 digits
    >>> scan_datetime('2008-07-13 14:5:16  ')
    Traceback (most recent call last):
    DatetimeValueError: "2008-07-13 14:5:16  " ... minute should have 2 digits
    >>> scan_datetime('20080713 14151  ')
    Traceback (most recent call last):
    DatetimeValueError: "20080713 14151  " ... time should have 4 or 6 digits (HHMM or HHMMSS)
    >>> scan_datetime('20080713 1.234')
    Traceback (most recent call last):
    DatetimeValueError: "20080713 1.234" ... time should have 6 digits before decimal point (HHMMSS.sss)
    """
    global _datepat, _colontimepat, _numerictimepat
    if endpos is None:
        endpos = len(string)
    datematch = _datepat.match(string, pos, endpos)
    if datematch is None:
        return None, pos
    try:
        nextpos = datematch.end()
        yyyy, m, d, mm, dd = datematch.groups()
        if m:
            # delimited format
            if len(d) != 2 and len(d) != 1:
                raise ValueError('day should have 1 or 2 digits')
            year, month, day = int(yyyy), int(m), int(d)
            timepat = _colontimepat
        else:
            # numeric format
            if len(dd) > 2:
                raise ValueError('date should have 8 digits (YYYYMMDD)')
            year, month, day = int(yyyy), int(mm), int(dd)
            timepat = _numerictimepat

        timematch = timepat.match(string, nextpos, endpos)
        if timematch is None:
            return date(year, month, day), nextpos
        nextpos = timematch.end()

        if m:
            # delimited format
            hh, mm, ss, frac = timematch.groups()
            if len(hh) > 2:
                raise ValueError('hour should have 1 or 2 digits')
            if len(mm) != 2:
                raise ValueError('minute should have 2 digits')
            if ss is not None and len(ss) != 2:
                raise ValueError('second should have 2 digits')
        else:
            # numeric format
            hhmmss, frac = timematch.groups()
            if len(hhmmss) == 6:
                hh, mm, ss = hhmmss[:2], hhmmss[2:4], hhmmss[4:]
            elif frac:
                raise ValueError('time should have 6 digits before decimal point (HHMMSS.sss)')
            elif len(hhmmss) == 4:
                hh, mm, ss = hhmmss[:2], hhmmss[2:], None
            else:
                raise ValueError('time should have 4 or 6 digits (HHMM or HHMMSS)')

        if frac:
            microsecond = int((frac + '000000')[1:7])
            dt = datetime(year, month, day, int(hh), int(mm), int(ss), microsecond)
        elif ss:
            dt = datetime(year, month, day, int(hh), int(mm), int(ss))
        else:
            dt = datetime(year, month, day, int(hh), int(mm))
        return dt, nextpos
    except ValueError as e:
        # Nonsensical date or time (e.g. field out of range, such as month > 12)
        raise DatetimeValueError(str(e), string, pos, nextpos)


#------------------------------- timedelta -------------------------------

signed_duration_syntax_msg = 'Specify duration as [hours][:minutes[:seconds[.fraction]]]'
unsigned_duration_syntax_msg = 'Specify duration as [+|-][hours][:minutes[:seconds[.fraction]]]'

def str_to_duration(string, pos=0, endpos=None, signed=True):
    """
    Interprets string[pos:endpos] as a duration or length of time in the form:
        [+|-][hours][:minutes[:seconds[.fraction]]]

    If string[pos:endpos] contains a valid duration, it is converted to a
    datetime.timedelta object, which is returned.

    Raises DatetimeValueError if string[pos:endpos] does not contain a
    valid duration; or if the duration includes a '+' or '-' sign and the
    caller specifies signed=False; or if the duration is preceded or
    followed by anything but whitespace.

    Examples...

    >>> str_to_duration('48')
    datetime.timedelta(2)
    >>> str_to_duration(' :120 ')
    datetime.timedelta(0, 7200)
    >>> str_to_duration(':0:72')
    datetime.timedelta(0, 72)
    >>> str_to_duration(':1:30')
    datetime.timedelta(0, 90)
    >>> str_to_duration('1:2:3.123456789')
    datetime.timedelta(0, 3723, 123456)

    # slicing
    >>> str_to_duration('9249', 1, 3)
    datetime.timedelta(1)

    # duration can be negative if signed=True
    >>> str_to_duration('-1')
    datetime.timedelta(-1, 82800)
    >>> str_to_duration('-:1')
    datetime.timedelta(-1, 86340)

    Errors...  (for more, see scan_duration() below)

    >>> str_to_duration('')
    Traceback (most recent call last):
    DatetimeValueError: Specify duration as [hours][:minutes[:seconds[.fraction]]]
    >>> str_to_duration(':')
    Traceback (most recent call last):
    DatetimeValueError: ":" ... Specify duration as [hours][:minutes[:seconds[.fraction]]]
    >>> str_to_duration('1:2: 3')
    Traceback (most recent call last):
    DatetimeValueError: "1:2: 3" ... duration is followed by unrecognized ": 3"
    """
    global duration_syntax_msg
    if endpos is None:
        endpos = len(string)
    value, nextpos = scan_duration(string, pos, endpos, signed)
    if value is None:
        # string is empty or doesn't conform to the syntax we require
        if signed:
            raise DatetimeValueError(signed_duration_syntax_msg, string, pos, endpos)
        else:
            raise DatetimeValueError(unsigned_duration_syntax_msg, string, pos, endpos)
    elif nextpos < endpos and not string[nextpos:endpos].isspace():
        # got valid duration, but there is something more after it
        msg = ('duration is followed by unrecognized "%s"'
               % string[nextpos:min(nextpos+10,endpos)].strip())
        raise DatetimeValueError(msg, string, pos, endpos)
    return value


def scan_duration(string, pos=0, endpos=None, signed=True):
    """
    Consumes an initial substring of the slice string[pos:endpos]
    representing a duration of time.  Leading whitespace is ignored.

    If 'signed' is True, a '+' or '-' sign may precede the duration.
    Note that a negative duration stored in a datetime.timedelta object
    is normalized so that only the 'days' field is negative; for example,
    '-:0:0.000001' is represented as datetime.timedelta(days=-1,
    seconds=86399, microseconds=999999).

    (None, pos) is returned if the input does not conform to the
    [+|-][hours][:minutes[:seconds[.fraction]]] format.

    If a valid duration is found, a tuple (td, nextpos) is returned,
    where 'td' is a datetime.timedelta object, and 'nextpos' is the
    index of the next character of the string (pos <= nextpos <= endpos).

    Raises DatetimeValueError if the text syntactically resembles a
    duration but fails semantic checks (e.g. field out of range, such
    as minutes > 59 in '100:60:00'); or if 'signed' is False and a
    '+' or '-' sign is found.

    Examples...  (for other syntax examples, see str_to_duration() above)

    >>> scan_duration('1:2:3.4')
    (datetime.timedelta(0, 3723, 400000), 7)
    >>> scan_duration('')
    (None, 0)
    >>> scan_duration('bad')
    (None, 0)

    Errors...

    >>> scan_duration('100:200:300')
    Traceback (most recent call last):
    DatetimeValueError: "100:200:300" ... minutes should be in 0..59
    >>> scan_duration('-:1', signed=False)
    Traceback (most recent call last):
    DatetimeValueError: "-:1" ... duration should be unsigned
    """
    global _durationpat
    if endpos is None:
        endpos = len(string)
    match = _durationpat.match(string, pos, endpos)
    sign, h, m, s, f = match.groups()
    if not h and not m:
        return None, pos
    try:
        hours = minutes = seconds = microseconds = 0
        if h:
            hours = int(h)
        if m:
            minutes = int(m)
            if s:
                seconds = int(s)
                if f and len(f) > 1:
                    microseconds = int((f + '000000')[1:7])
        if hours > 0 and minutes > 59:
            raise ValueError('minutes should be in 0..59')
        minutes += hours * 60
        if minutes > 0 and seconds > 59:
            raise ValueError('seconds should be in 0..59')
        seconds += minutes * 60
        td = timedelta(seconds=seconds, microseconds=microseconds)
        if sign:
            if not signed:
                raise ValueError('duration should be unsigned')
            if sign == '-':
                td = -td
        return td, match.end()
    except OverflowError as e:
        # Nonsensical duration (e.g. field out of range)
        raise DatetimeValueError(str(e), string, pos, match.end())
    except ValueError as e:
        # Nonsensical duration (e.g. field out of range)
        raise DatetimeValueError(str(e), string, pos, match.end())



#-------------------------------- private --------------------------------

_datepat = (r'\s*'                           # skip any leading whitespace
            r'(\d\d\d\d)'                    # yyyy                     \1
            r'(?:'                           # followed by either
            r'(?:-(\d\d?)-(\d*))|'           # -m[m]-d[d] or            \2 \3
            r'(?:(\d\d)(\d\d+))'             # mmdd                     \4 \5
            r')')                            # note dd absorbs excess digits
_colontimepat = (r'(?:\s+|[Tt])'             # whitespace or 'T'
                 r'(\d+)'                    # [h]h                     \1
                 r':(\d+)'                   # :mm                      \2
                 r'(?::(\d*)(\.\d*)?)?')     # [:ss[.frac]]             \3 \4
                                             # hh, mm, ss absorb excess digits
_numerictimepat = (r'(?:\s+|[Tt])'           # whitespace or 'T'
                   r'(\d+)'                  # hhmmss                   \1
                   r'(\.\d*)?')              # [.frac]                  \2
                                             # hhmmss absorbs excess digits
_durationpat = (r'\s*'                       # skip any leading whitespace
                r'([+-])?'                   # [+|-]                    \1
                r'(\d*)'                     # [hours]                  \2
                r'(?:'                       # [
                r':(\d+)'                    # :minutes                 \3
                r'(?::(\d+)(\.\d*)?)?'       # [:seconds[.frac]]        \4 \5
                r')?')                       # ]
_spacepat = r'\s*'                           # whitespace

_datepat = re.compile(_datepat)
_colontimepat = re.compile(_colontimepat)
_numerictimepat = re.compile(_numerictimepat)
_durationpat = re.compile(_durationpat)
_spacepat = re.compile(_spacepat)

# If invoked as a script, execute the examples in each function's docstring
# and verify the expected output.  Produces no output if verification is
# successful, unless -v is specified.  Use -v option for verbose test output.
def _test():
    import doctest
    doctest.testmod()

if __name__ == "__main__":
    _test()

