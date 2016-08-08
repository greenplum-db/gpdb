-- Install procedure language PL/PERL
-- CREATE LANGUAGE plperl;

-- Data preparation
\echo '-- start_ignore'
DROP FUNCTION IF EXISTS func_plperl();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
\echo '-- end_ignore'

CREATE TABLE func_call_modes_data ( c1 INT, c2 INT ) DISTRIBUTED BY (c1);
INSERT INTO func_call_modes_data SELECT i, i FROM generate_series(1,3) i;

-- PL/PERL
CREATE OR REPLACE FUNCTION func_plperl() RETURNS SETOF func_call_modes_data
AS $$
        my $rv = spi_exec_query('SELECT * FROM func_call_modes_data ORDER BY c1;');
        my $status = $rv->{status};
        my $nrows = $rv->{processed};
        foreach my $rn (0 .. $nrows-1)
        {
                my $row = $rv->{rows}[$rn];
                return_next($row);
        }
        return undef;
$$ LANGUAGE PLPERL;

SELECT func_plperl();
SELECT * FROM func_plperl();

-- Clean up
DROP FUNCTION IF EXISTS func_plperl();
DROP TABLE IF EXISTS func_call_modes_data CASCADE;
