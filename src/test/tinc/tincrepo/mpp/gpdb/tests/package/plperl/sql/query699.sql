-- MPP-7581: Simple MapReduce program produces an out of memory error
-- Added by: Johnny Soedomo

-- start_ignore
create language plperlu;
-- end_ignore

CREATE TABLE mpp7581 (
	eventID char(10)
);

CREATE TYPE mpp7581_type_test AS
(
	uuid varchar(50),
	x0600_1 varchar(50),
	x0600_2 varchar(50),
	x0600_3 varchar(50),
	x0600_4 varchar(50),
	x0603_1 varchar(50),
	x0700_1 varchar(50),
	x0701_1 varchar(50),
	x0701_2 varchar(50),
	x0701_3 varchar(50),
	x0702_1 varchar(50),
	outliers char(1)
);

CREATE OR REPLACE FUNCTION mpp7581_perl_test() RETURNS setof mpp7581_type_test AS $$
	my $rv = spi_exec_query('select * from mpp7581;');
        my $status = $rv->{status};
        my $nrows = $rv->{processed};

        foreach my $rn (0 .. $nrows - 1)
        {
		my $row = $rv->{rows}[$rn];
		my $atom_group_id = $row->{atom_group_id};
		my $atoms = $row->{atoms};
		my $delim = "@";
		my $outliers = 'N';
        
        	#split on ; to make an array of atom groups 
        	my (@value) = split(/[;]/, $atoms);

        	#loop thru array
        	foreach $eventID_and_desc (sort @value)
		{
          		#split again to get $key (eventID) and $value (eventDESC)
          		($eventID, $eventDesc) = split(/[=]/, $eventID_and_desc);
         
            		#5 core event IDs
            		if($eventID eq 'X0600')
			{
              			($f1, $f2, $f3, $f4) = split(/:/, $eventDesc);
            		}elsif($eventID eq 'X0603'){
              			($f5) = split(/:/, $eventDesc);
            		}elsif($eventID eq 'X0700'){
              			($f6) = split(/:/, $eventDesc);
            		}elsif($eventID eq 'X0701'){
              			($f7,$f8,$f9) = split(/:/, $eventDesc);
            		}elsif($eventID eq 'X0702'){
              			($f10) = split(/:/, $eventDesc);
            		}else{ #indicate if events other than these occur (ie. X0001, X000E, etc)
              			$outliers = 'Y';
            		}
        	}
                #return_next( {atom_group_id => $atom_group_id, atoms => $atoms} );
		return_next ({"uuid" => $atom_group_id.$delim, "x0600_1"=>$f1.$delim, "x0600_2"=>$f2.$delim, "x0600_3"=>$f3.$delim, "x0600_4"=>$f4.$delim, "x0603_1"=>$f5.$delim, "x0700_1"=>$f6.  $delim, "x0701_1"=>$f7.$delim, "x0701_2"=>$f8.$delim, "x0701_3"=>$f9.$delim, "x0702_1"=>$f10, "outliers"=>$outliers});
        }
        return undef;
$$ LANGUAGE plperlu;

select * from mpp7581_perl_test() ;

drop type mpp7581_type_test cascade;
drop table mpp7581;

-- start_ignore
drop language plperlu;
-- end_ignore
