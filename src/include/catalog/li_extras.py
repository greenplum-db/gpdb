#! /usr/local/bin/python

import sys
import datetime

import pdb
#pdb.set_trace()

# Test and declaration generator for linear_interpolation

# Utility

def kwot(s):
    """Single quote a string, doubling contained quotes as needed."""
    return "'" + "''".join(s.split("'")) + "'"

# Following section is just fragile helper functions to produce triples 
# of values (v0, v, v1) such that, for any choice of scale and transform 
# (scale applied first, transform last):
#
#   (v-v0)/(v1-v) = p1/p2
#
# The idea is that, by keeping p1 and p2 uniform in a test set, we can 
# know the result of linear interpolation independently of the actual
# function linear_interpolate and include it in the regressions to make
# it easy to identify issues.

def int_triple(p1, p2, scale, transform):
    return tuple( [x*scale + transform for x in [0, p1, p1+p2]] )

def num_triple(p1, p2, scale, transform):
    return tuple( [str(x) for x in int_triple(p1, p2, scale, transform)] )

def date_triple(p1, p2, scale, transform, basedate):
    i = int_triple(p1, p2, scale, transform)
    g = basedate.toordinal()
    d = [datetime.date.fromordinal(x + g) for x in i]
    return tuple( [kwot(x.isoformat()) for x in d] ) 

def time_offset_by_minutes(t, d):
    x = t.hour * 60 + t.minute + d
    h = x / 60
    m = x - 60 * h
    h = h - 24 * (h / 24)
    return datetime.time(hour = h, minute = m)

def time_triple(p1, p2, scale, transform, basetime):
    i = int_triple(p1, p2, scale, transform)
    t = [ time_offset_by_minutes(basetime, x) for x in i]
    return tuple( [kwot(x.isoformat()) for x in t] ) 

def datetime_triple(p1, p2, scale, transform, basestamp):
    i = int_triple(p1, p2, scale, transform)
    d = [ datetime.timedelta(minutes=x) for x in i ]
    return [ kwot( (basestamp + x).isoformat() ) for x in d]
    
def interval_triple(p1, p2, scale, transform, baseminutes):
    i = int_triple(p1, p2, scale, transform+baseminutes)
    return tuple( [ kwot(str(x) + ' minutes') for x in i ] )


# The following table drives tests and declarations per data type.
# The keys are type SQL type names that are known to cattulus.pl.
# The values are tuples of
# - a 3-tuple of values in SQL form (no casts): low, middle, high.
# - a 3-tuple of proportionally spread values like the first.
# - an appropriate C type declarator (beware Interval's pointer-ness).
# - position as in index into array of types.
#
# The position value in important to ensure OID consistency.
#
# The delta values fix the proportions of the 3-tuple values.

delta1 = 1
delta2 = 4
    
type_dict = {
    'int8' : (
        num_triple(delta1, delta2,  100, 100), 
        num_triple(delta1, delta2,  50, 2000),
        'int64',
        0),
    'int4' : (
        num_triple(delta1, delta2,  100, 100), 
        num_triple(delta1, delta2,  50, 2000), 
        'int32',
        1),
    'int2' : (
        num_triple(delta1, delta2,  100, 100), 
        num_triple(delta1, delta2,  50, 2000), 
        'int2',
        2),
    'float8' : (
        num_triple(delta1, delta2,  100, 100), 
        num_triple(delta1, delta2,  50, 2000), 
        'float8',
        3),
    'float4' : (
        num_triple(delta1, delta2,  100, 100), 
        num_triple(delta1, delta2,  50, 2000), 
        'float4',
        4),
    'date' : (
        date_triple(delta1, delta2,  10, 10, datetime.date(2001, 1, 1)),
        date_triple(delta1, delta2,  10, 20, datetime.date(2010, 1, 1)), 
        'DateADT',
        5), 
    'time' : (
        time_triple(delta1, delta2,  5, 20, datetime.time(hour=10)),
        time_triple(delta1, delta2,  10, 300, datetime.time(hour=10)), 
        'TimeADT',
        6), 
    'timestamp' : (
        datetime_triple(delta1, delta2,  1000, 2000, datetime.datetime(2010, 1, 1)),
        datetime_triple(delta1, delta2,  5000, 1000, datetime.datetime(2012, 6, 1)), 
        'Timestamp',
        7), 
    'timestamptz' : (
        datetime_triple(delta1, delta2,  1000, 2000, datetime.datetime(2010, 1, 1)),
        datetime_triple(delta1, delta2,  5000, 1000, datetime.datetime(2012, 6, 1)), 
        'TimestampTz',
        8), 
    'interval' : (
        interval_triple(delta1, delta2,  20, 10, 55),
        interval_triple(delta1, delta2,  10, 20, 30), 
        'Interval',
        9),
    'numeric' : (
        num_triple(delta1, delta2,  100, 100), 
        num_triple(delta1, delta2,  50, 2000), 
        'Numeric',
        10),
    }


# For OID assignment we choose to order the types and assign ascending 
# OID values starting from a base.  The caller is responsible for setting
# base and maintaining the order of assignment.

oid_base = 6072

def ordered_types():
    """List of types in canonical order of OID assignment."""
    keys = type_dict.keys()
    ordered_keys = [None for k in keys]
    for key in keys:
        pos = type_dict[key][3]
        ordered_keys[pos] = key
    assert not None in ordered_keys
    return ordered_keys

# Convenient wrapper for one of the table's 3-tuples.
class Coordinate(object):

    def __init__(self, ctype, lmhtuple):
    
        self._type = ctype
        (self._low, self._mid, self._high) = lmhtuple
    
    def low(self):
        return self._low + '::' + self._type
    
    def mid(self):
        return self._mid + '::' + self._type
    
    def high(self):
        return self._high + '::' + self._type


def linear_interpolate(x, y, x0, y0, x1, y1):
    """Format a call to linear_interpolate for the given arguments and expected result.
    """
    
    fmt = """
select 
    linear_interpolate({x}, {x0}, {y0}, {x1}, {y1}),
     {y} as answer,
     {y} = linear_interpolate({x}, {x0}, {y0}, {x1}, {y1}) as match ;"""
    
    return fmt.format(x=x, y=y, x0=x0, y0=y0, x1=x1, y1=y1)

def preliminary_tests(verbose):
    """ Preliminary tests (SQL), optionally verbose, as '\n'-delimited string.
    """
    
    prelim = """
-- A "generic" unsupported type.

select linear_interpolate('x'::text, 'x0'::text, 0, 'x1'::text, 1000);
select linear_interpolate(5, 0, 'y0'::text, 100, 'y1'::text);


-- Check that  "divide by zero" returns null"""
    
    prelim = prelim.split('\n')
    fmt = """select linear_interpolate({x}, {x0}, {y0}, {x1}, {y1});"""
    
    for t in type_dict:
        
        a = Coordinate(t, type_dict[t][0])
        o = Coordinate('int4', type_dict['int4'][1])
        s = fmt.format(x=a.mid(), x0=a.low(), y0=o.low(), x1=a.low(), y1=o.high())
        prelim = prelim + s.split('\n')
    
    return '\n'.join(prelim)

def basic_tests(abscissa_type, ordinate_type, verbose):
    """Per-type tests (SQL), optionally verbose, as '\n'-delimited string.
    """
    
    abscissa = Coordinate(abscissa_type, type_dict[abscissa_type][0])
    ordinate = Coordinate(ordinate_type, type_dict[ordinate_type][1])
    
    if verbose:
        lst = [
            '',
            '\qecho',
            '\qecho Check interpolation correctness: %s --> %s' % (abscissa_type, ordinate_type),
            ]
        prolog = []
    else:
        lst = []
        prolog = [
            '',
            '',
            '-- Abscissa: %s, Ordinate: %s' % (abscissa_type, ordinate_type),
            '-- Check correctness - all results should have match = t'
            ]
    
    # Use the triples in all combinations to test outlying cases as well as "sweet
    # spot" cases.
    
    tst = linear_interpolate(
        abscissa.mid(), ordinate.mid(),
        abscissa.low(), ordinate.low(), 
        abscissa.high(), ordinate.high() )
    lst = lst + tst.split('\n')
    
    tst = linear_interpolate(
        abscissa.mid(), ordinate.mid(),
        abscissa.high(), ordinate.high(), 
        abscissa.low(), ordinate.low() )
    lst = lst + tst.split('\n')

    tst = linear_interpolate(
        abscissa.low(), ordinate.low(),
        abscissa.mid(), ordinate.mid(), 
        abscissa.high(), ordinate.high() )
    lst = lst + tst.split('\n')
    
    tst = linear_interpolate(
        abscissa.low(), ordinate.low(),
        abscissa.high(), ordinate.high(), 
        abscissa.mid(), ordinate.mid() )
    lst = lst + tst.split('\n')

    tst = linear_interpolate(
        abscissa.high(), ordinate.high(),
        abscissa.mid(), ordinate.mid(), 
        abscissa.low(), ordinate.low() )
    lst = lst + tst.split('\n')
    
    tst = linear_interpolate(
        abscissa.high(), ordinate.high(),
        abscissa.low(), ordinate.low(), 
        abscissa.mid(), ordinate.mid() )
    lst = lst + tst.split('\n')
    
    # Include one trivial case
    tst = linear_interpolate(
        abscissa.mid(), ordinate.mid(),
        abscissa.mid(), ordinate.mid(), 
        abscissa.mid(), ordinate.mid() )
    lst = lst + tst.split('\n')
    
    return '\n'.join(prolog + lst)

def all_tests(verbose):
    result = preliminary_tests(verbose)
    
    for abscissa_type in type_dict:
        result = result + basic_tests(abscissa_type, 'int4', verbose)
    
    for ordinate_type in type_dict:
        result = result + basic_tests('int4', ordinate_type, verbose)
    
    return result

def regression_tests():
    return all_tests(False)

def readable_tests():
    return all_tests(True)
    
declared_description = 'linear interpolation: x, x0,y0, x1,y1'

def pg_proc_declarations():
    template = """
CREATE FUNCTION linear_interpolate(
        anyelement, 
        anyelement, 
        {T}, 
        anyelement, 
        {T}
        ) 
    RETURNS {T} 
    LANGUAGE internal IMMUTABLE STRICT 
    AS 'linterp_{t}'
    WITH (OID={oid}, DESCRIPTION="{desc}");""" 
    


    result = ['-- for cdb_pg/src/include/catalog/pg_proc.sql', '']
    
    fmt = ' '.join([x.strip() for x in template.split('\n')])
    next_oid = oid_base
    
    all_types = ordered_types()
    
    for sql_type in all_types:
        c_type = type_dict[sql_type][2]
        result = result + [fmt.format(T=sql_type, t=c_type, oid=str(next_oid), desc=declared_description)]
        next_oid = next_oid + 1
        
    return '\n'.join(result)

def upgrade_declarations():
    upgrade_1 = """
CREATE FUNCTION @gpupgradeschemaname@.linear_interpolate(
        anyelement, 
        anyelement, 
        {T}, 
        anyelement, 
        {T}
        ) 
    RETURNS {T} 
    LANGUAGE internal IMMUTABLE STRICT 
    AS 'linterp_{t}'
    WITH (OID={oid}, DESCRIPTION="{desc}");"""
    
    upgrade_2 = """
COMMENT ON FUNCTION @gpupgradeschemaname@.linear_interpolate(
        anyelement, 
        anyelement, 
        {T}, 
        anyelement, 
        {T}
        ) 
    IS '{desc}';""" 
    
    result = ['-- for src/test/regress/data/upgradeXX/upg2_catupgrade_XXX.sql.in', '']
    
    upg_1 = ' '.join([x.strip() for x in upgrade_1.split('\n')])
    upg_2 = ' '.join([x.strip() for x in upgrade_2.split('\n')])
    next_oid = oid_base
    
    all_types = ordered_types()
    
    for sql_type in all_types:
        c_type = type_dict[sql_type][2]
        result.append( upg_1.format(T=sql_type, t=c_type, oid=str(next_oid), desc=declared_description) )
        result.append( upg_2.format(T=sql_type, t=c_type, oid=str(next_oid), desc=declared_description) )
        result.append( '' ) 
        next_oid = next_oid + 1

    return '\n'.join(result)
    
#
# Interpret the arguments, write result to standard out.

def main():
    argmap = {
        'readable' : readable_tests,
        'regression' : regression_tests,
        'pg_proc' : pg_proc_declarations,
        'upgrade' : upgrade_declarations
        }
    efmt = 'argument must be one of (%s), not "%s"' % (', '.join(argmap.keys()), "%s")

    if len(sys.argv) == 1:
        fn = argmap['readable']
    elif len(sys.argv) == 2 and sys.argv[1] in argmap:
        fn = argmap[sys.argv[1]]
    else:
        sys.exit(efmt % ' '.join(sys.argv[1:]))
    
    print fn()
       
if __name__ == '__main__':
    main()
 
