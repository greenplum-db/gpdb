----Case 0 begin----
-- error handling process.
CREATE or REPLACE function pltest.PLPERLerrorhandling1_0 (  ) RETURNS TEXT as $$  
        spi_exec_query('CREATE TABLE pltest.errorhandlingtmptable(id1 int4) DISTRIBUTED BY(id1);');
        spi_exec_query('INSERT INTO pltest.errorhandlingtmptable VALUES (1);');
        spi_exec_query('INSERT INTO pltest.errorhandlingtmptable VALUES (2);');
        spi_exec_query('INSERT INTO pltest.errorhandlingtmptable VALUES (4);');
	
	eval
	{
        	spi_exec_query("INSERT INTO pltest.errorhandlingtmptable VALUES ('fjdk');");
	};
	if ($@)
	{
		elog(NOTICE,"error occurs but ignore...\n");
                # hide exception
	}
        my $rv = spi_exec_query('SELECT id1 from pltest.errorhandlingtmptable order by id1');
        my $status = $rv->{status};
        my $nrows = $rv->{processed};
        my $result = '';
        foreach my $rn (0 .. $nrows - 1) {
                my $row = $rv->{rows}[$rn];
                $result = $result . $row->{id1};
        }
	spi_exec_query('DROP TABLE pltest.errorhandlingtmptable');
        return $result;
$$ language PLPERL;

-- select pltest.PLPERLerrorhandling1_0() from pltest.tableTEXT;

select pltest.PLPERLerrorhandling1_0();

select * from pltest.PLPERLerrorhandling1_0();

DROP function pltest.PLPERLerrorhandling1_0 (  );
----Case 0 end----


----Case 1 begin----
-- spi support
CREATE or REPLACE function pltest.PLPERLspisupport_1 (  ) RETURNS TEXT as $$  
        spi_exec_query('CREATE TABLE pltest.myspitable(id1 int4) DISTRIBUTED BY(id1);');
        spi_exec_query('INSERT INTO pltest.myspitable VALUES (1);');
        spi_exec_query('INSERT INTO pltest.myspitable VALUES (2);');
        spi_exec_query('INSERT INTO pltest.myspitable VALUES (4);');
		
		my $rv = spi_exec_query('SELECT id1 from pltest.myspitable order by id1');
        my $status = $rv->{status};
        my $nrows = $rv->{processed};
        my $result = '';
        foreach my $rn (0 .. $nrows - 1) {
                my $row = $rv->{rows}[$rn];
                $result = $result . $row->{id1};
        }
	spi_exec_query('DROP TABLE pltest.myspitable');
        return $result;
$$ language PLPERL;

-- select pltest.PLPERLspisupport_1() from pltest.tableTEXT;

select pltest.PLPERLspisupport_1();

select * from pltest.PLPERLspisupport_1();

DROP function pltest.PLPERLspisupport_1 (  );
----Case 1 end----


----Case 2 begin----
-- log facilities
CREATE or REPLACE function pltest.PLPERLelog_2 ( IN  TEXT ) RETURNS VOID as $$  
  my $msg = shift;
  elog(NOTICE,$msg);
$$ language PLPERL;


select pltest.PLPERLelog_2('QAZWSXEDCRFVTGB'::TEXT);

select pltest.PLPERLelog_2(NULL);

select * from pltest.PLPERLelog_2('QAZWSXEDCRFVTGB'::TEXT);

select * from pltest.PLPERLelog_2(NULL);

DROP function pltest.PLPERLelog_2 ( IN  TEXT );
----Case 2 end----



----Case 3 begin----
-- test quote_literal.
CREATE or REPLACE function pltest.PLPERLquote_literal_3 (  ) RETURNS SETOF TEXT as $$  
	return_next "undef: ".quote_literal(undef);
	return_next sprintf"$_: ".quote_literal($_)
			for q{foo}, q{a'b}, q{a"b}, q{c''d}, q{e\f}, q{};
	return undef;
$$ language PLPERL;

select pltest.PLPERLquote_literal_3() as id from pltest.tableTEXT order by id;

select pltest.PLPERLquote_literal_3() as id order by id;

select * from pltest.PLPERLquote_literal_3() plperlquote_literal_3;

DROP function pltest.PLPERLquote_literal_3 (  );
----Case 3 end----


----Case 4 begin----
-- global facilities test
CREATE or REPLACE function pltest.PLPERLsetme_4 ( IN key TEXT , IN val TEXT ) RETURNS VOID as $$  
  my $key = shift;
  my $val = shift;
  $_SHARED{$key}= $val;
$$ language PLPERL;


CREATE or REPLACE function pltest.PLPERLgetme ( IN  TEXT ) RETURNS TEXT as $$
  my $key = shift;
  return $_SHARED{$key};
$$ language PLPERL;



select pltest.PLPERLsetme_4('P1'::TEXT,'V1');
select pltest.PLPERLgetme('P1'::TEXT);
select pltest.PLPERLgetme('P2'::TEXT);
select pltest.PLPERLgetme(NULL);


select pltest.PLPERLsetme_4('P1'::TEXT,NULL);
select pltest.PLPERLgetme('P1'::TEXT);
select pltest.PLPERLgetme('P2'::TEXT);
select pltest.PLPERLgetme(NULL);

select pltest.PLPERLsetme_4(NULL,'P1'::TEXT);
select pltest.PLPERLgetme('P1'::TEXT);
select pltest.PLPERLgetme('P2'::TEXT);
select pltest.PLPERLgetme(NULL);

DROP function pltest.PLPERLsetme_4 ( IN key TEXT , IN val TEXT );
DROP function pltest.PLPERLgetme ( IN  TEXT );
----Case 4 end----




----Case 6 begin----
-- built-in function warn
CREATE or REPLACE function pltest.PLPERLbuildinwarn_7 (  ) RETURNS TEXT as $$  
    warn("hello");
	return "hello";
$$ language PLPERL;


select pltest.PLPERLbuildinwarn_7();

select * from pltest.PLPERLbuildinwarn_7();

DROP function pltest.PLPERLbuildinwarn_7 (  );
----Case 6 end----


