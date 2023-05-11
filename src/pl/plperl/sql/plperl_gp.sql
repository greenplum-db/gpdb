-- Error handling process.
CREATE or REPLACE function PLPERLerrorhandling1_0 (  ) RETURNS TEXT as $$  

	spi_exec_query('CREATE TABLE errorhandlingtmptable(id1 int4) DISTRIBUTED BY(id1);');
	spi_exec_query('INSERT INTO errorhandlingtmptable VALUES (1);');
	spi_exec_query('INSERT INTO errorhandlingtmptable VALUES (2);');
	spi_exec_query('INSERT INTO errorhandlingtmptable VALUES (4);');
	
	eval
	{
		spi_exec_query("INSERT INTO errorhandlingtmptable VALUES ('fjdk');");
	};
	if ($@)
	{
		# hide exception
		elog(NOTICE,"error occurs but ignore...\n");
	}

	my $rv = spi_exec_query('SELECT id1 from errorhandlingtmptable order by id1');
	my $status = $rv->{status};
	my $nrows = $rv->{processed};
	my $result = '';
	foreach my $rn (0 .. $nrows - 1) {
		my $row = $rv->{rows}[$rn];
		$result = $result . $row->{id1};
	}
	spi_exec_query('DROP TABLE errorhandlingtmptable');

	return $result;

$$ language PLPERL;

select PLPERLerrorhandling1_0();
select * from PLPERLerrorhandling1_0();

CREATE FUNCTION tf_perl(anytable) returns int AS $$ return 1 $$ language plperl;
