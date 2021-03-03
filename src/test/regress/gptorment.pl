#!/usr/bin/env perl
#
# Portions Copyright (c) 2008-2010 GreenPlum.  All rights reserved.
# Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
#
# Author: Jeffrey I Cohen
#
use Pod::Usage;
use Getopt::Long;
use Data::Dumper;
use Config;
use strict;
use warnings;

=head1 NAME

B<gptorment.pl> - subject database to infernal torment

=head1 SYNOPSIS

B<gptorment.pl> [options] 

Options:

    -help            brief help message
    -man             full documentation
    -connect         psql connect parameters
    -sqlfile         the test file
    -id              id of parent (DO NOT SET)
    -parallel        degree of parallelism/concurrency
    -iter            number of testing iterations
    -timelimit       time duration for time-limited testing
    -debug           debug flag (show test plan, do not execute)
    -tmpdir          temporary directory (DO NOT SET)
    -grace_period    time to wait before killing child test processes

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-connect>
    
    psql connect string, e.g:

    -connect '-p 11000 -d template1'
    
=item B<-sqlfile>

    A sql file to execute.

=item B<-id>

    The ID code for the parent of the current test process.  DO NOT SET.
    
=item B<-parallel>
    
    The degree of parallelism of testing.  Spawn this many concurrent
    child test processes.  Degree=1 if not set.

=item B<-iter>

    The number of test iterations (1 by default). Note that if only
    iter is set and not timelimit, then gptorment has no way to detect
    test failures like infinite loops or hangs.

=item B<-timelimit>

    For timed testing, the amount of time spent repeating the test.
    If iter is set, then run the test for (iter*timelimit) time.
    If no units are specified, the timelimit is in seconds.  Timelimit
    accepts mixed time specifications like -tl="1 min 14 seconds" or
    even -tl="2+2 min - 14 seconds"

    Legal time periods are seconds, minutes, hours, days, weeks, years
    (but not months!).  gptorment simply uses the first letter of the
    unit to determine the period.
    
=item B<-debug>

    If specified, gptorment prints the basic test plan, but does not
    execute it.
    
=item B<-tmpdir>

    The temporary directory containing all test output (DO NOT SET).
    
=item B<-grace_period>

    If a timelimit is specified, the grace period is the amount of
    time gptorment waits after the timelimit is exceeded before trying
    to B<kill -9> the child processes.  A grace period can be negative,
    which will cause gptorment to kill the children prematurely.  Note
    that if you use a negative number, you must quote the expression
    e.g. -grace='-10 sec' to prevent gptorment from confusing the
    leading minus with a command argument.

=back

=head1 DESCRIPTION

gptorment.pl is a stress test framework.
Substitute the correct "connect string" for your postgres database.

gptorment.pl repeated executes a test plan against the database for a
number of iterations or for a specified amount of time, using a
specified degree of concurrency.  It is designed for batch or
interactive use.  Because this type of testing can result in serious
failure, deadlocks, or other system resource issues, gptorment has a
simple but robust strategy to manage child processes.  Rather than
wait for child processes, gptorment detaches them, but it marks a
unique id in the process argument list.  gptorment uses "ps" to
monitor the state of the child processes, and it can clean up "stuck"
processes with kill -9.

When gptorment is invoked, it creates a uniquely-named temporary
directory to hold test output.  All the child processes write their
output to files in this directory.  

gptorment does not create or startup a database.

-head1 TODO

invoke perl scripts
init, cleanup


=head1 AUTHORS

Jeffrey I Cohen

Portions Copyright (c) 2008-2010 GreenPlum.  All rights reserved.
Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.

Address bug reports and comments to: bugs@greenplum.org


=cut

my $glob_id = "";

my %GH; # GLOBAL HASH of all arguments

sub gettimelimit
{
    my $tlim = shift;

    return undef
        unless (defined($tlim));

    # time limit in seconds or other units (but not "1hr10min")
    #              time [space] [timeunit]{lookahead}
    unless ($tlim =~ m/^\s*(\+|\-|\.|\()?\d+/)
    {
        return undef;
    }

    $tlim =~ s/\s+//gm; # no spaces

# XXX XXX: month

    $tlim =~ s/([a-zA-Z])([a-zA-Z])*/$1/gm;

#    print "tlim: $tlim\n";

    $tlim = lc($tlim);

    my @foo = split(/([a-z])/, $tlim);

    $tlim = 0;

    my $tempnum;

    for my $vv (@foo)
    {
        next 
            unless (length($vv));

        if ($vv !~ /^[a-z]$/)
        {
            my $estr = '$tempnum = 0 + (' . $vv . ');';

 #           print "e:",$estr, "\n";

            eval $estr;
            if ($@)
            {
                warn "invalid numeric format \'$vv\' in time specification\n";
                return undef;
            }
        }
        else
        {
            my $min = 60;
            my $hr = $min * 60;
            my $day = $hr * 24;
            my $wk = $day * 7;
            my $mon = $day * 30;
            my $yr = $day * 365;

            if ($vv =~ m/s/)
            {
                $tlim += ($tempnum) 
            }
            elsif ($vv =~ m/m/)
            {
                $tlim += ($tempnum*$min) 
            }
            elsif ($vv =~ m/h/)
            {
                $tlim += ($tempnum*$hr) 
            }
            elsif ($vv =~ m/d/)
            {
                $tlim += ($tempnum*$day) 
            }
            elsif ($vv =~ m/w/)
            {
                $tlim += ($tempnum*$wk) 
            }
# XXX XXX: month
            elsif ($vv =~ m/y/)
            {
                $tlim += ($tempnum*$yr) 
            }
            else
            {
                warn "unknown time period spec \'$vv\'";
                return undef;
            }
            $tempnum = 0;

        }

    }

    $tlim += $tempnum;

#    print join(":", @foo), "\n";
#    print "time limit: $tlim \n";

    return $tlim;

} # end gettimelimit

sub I_am_interactive 
{
    return -t STDIN ;
}

sub gettmpfile
{

} # end gettmpfile

sub gettmpdir
{
    use File::Temp qw/ tempfile tempdir /;

    my $id = shift;

    # try to remove the leading date/time string from the id (to save
    # some space), since it will be implicit in the directory creation
    # time
    my @foo = split(/\./, $id, 2);

    if (2 == scalar(@foo))
    {
        shift @foo;
        $id = pop(@foo);
    }

    # replace dots with underscores
    $id =~ s/\./\_/gm;

    my $template = "gptorment_" . $id . "_XXXXXX";

    my $tempdir = tempdir ( $template, TMPDIR => 1, SUFFIX => '.dir' );

    return $tempdir;
} # end gettmpdir

BEGIN {
    my $man  = 0;
    my $help = 0;
    my $table;
    my $conn;
    my $paral;
    my $uid;
    my $sqlfile;
    my $iter;
    my $tlim;
    my $dbg;
    my $tmpdir;
    my $grace;

    GetOptions(
               'help|?' => \$help, man => \$man, 
               "connect=s" => \$conn,
               "id:s" => \$uid,
               "parallel:i" => \$paral,
               "sqlfile:s" => \$sqlfile,
               "iter:i" => \$iter,
               "tlim|timelimit:s" => \$tlim,
               "dbg|debug:s" => \$dbg,
               "tmpdir|tempdir:s" => \$tmpdir,
               "grace_period|grace_time|graceperiod|gracetime:s" => \$grace,
               )
        or pod2usage(2);

    
    pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
    pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

    $GH{connect} = $conn;

    $GH{connect} = '-p 11000 -d template1'
        unless (defined($GH{connect}));

    {  # check for bad connection
        my $badconnect = 0;
        my $psql_str = "psql ";

        $psql_str .= $GH{connect}
            if (defined($GH{connect}));

        $psql_str .= " -c \"select version()\" ";

        my $tabdef = `( $psql_str ) 2>&1`;

        # should say something like:
        #
        #    version
        # --------------
        # PostgreSQL 8.2.5 (Greenplum Database...
        #
        my @l3 = split(/\n/, $tabdef, 3);

        if (3 != scalar(@l3))
        {
            $badconnect = 1;
            goto l_badconnect;
        }

        my $hd1 = shift @l3;

        if ($hd1 !~ m/^\s*version\s*$/)
        {
            $badconnect = 1;
            goto l_badconnect;
        }
        my $hd2 = shift @l3;

        if ($hd2 !~ m/^\s*(\-)+\s*$/)
        {
            $badconnect = 1;
            goto l_badconnect;
        }

      l_badconnect:

        if ($badconnect)
        {
            $glob_id = "connect failed using \'" . $GH{connect} . "\'";

            pod2usage(-msg => $glob_id, -exitstatus => 1);
        }
    }

    $paral = 1
        unless (defined($paral));

    unless (defined($uid))
    {

        my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) =
            localtime(time);

        $year += 1900;
        $mon++;

        $uid = sprintf("%04d%02d%02d%02d%02d%02d",
                       $year,$mon, $mday, $hour, $min ,$sec);
    }

    $uid .= "." . $$;

    $iter = 1
        unless (defined($iter));

    unless (defined($sqlfile))
    {
        $glob_id = "no sql file";

        pod2usage(-msg => $glob_id, -exitstatus => 1);
    }

    unless (-e $sqlfile)
    {
        $glob_id = "sql file \'" . $sqlfile . "\' does not exist";

        pod2usage(-msg => $glob_id, -exitstatus => 1);
    }

    if (defined($tlim))
    {
        my $orig_tlim = $tlim;

        $tlim = gettimelimit($tlim);

        unless (defined($tlim))        
        {
            $glob_id = "invalid timelimit specification: $orig_tlim";
        
            pod2usage(-msg => $glob_id, -exitstatus => 1);
        }
        if ($tlim <= 0)
        {
            warn "invalid negative time $tlim seconds";
            pod2usage(-msg => $glob_id, -exitstatus => 1);
        }
    }

    unless (defined($tmpdir))
    {
        $tmpdir = gettmpdir($uid);
    }

    if (defined($grace))
    {
        my $orig_tlim = $grace;

        $grace = gettimelimit($grace);

        unless (defined($grace))        
        {
            $glob_id = "invalid grace_limit specification: $orig_tlim";
        
            pod2usage(-msg => $glob_id, -exitstatus => 1);
        }
        # NOTE:  allow negative grace period
    }
    else
    {
        $grace = 10;
    }


    $GH{uid} = $uid;
    $GH{parallel} = $paral;
    $GH{iter} = $iter;
    $GH{sqlfile} = $sqlfile;
    $GH{tlim} = $tlim;
    $GH{debug} = $dbg;
    $GH{tmpdir} = $tmpdir;
    $GH{grace} = $grace;

#    print "loading...\n" ;
}

# convert a postgresql psql formatted table into an array of hashes
sub tablelizer
{
    my $ini = shift;

    # first, split into separate lines, the find all the column headings

    my @lines = split(/\n/, $ini);

    return undef
        unless (scalar(@lines));

    my $line1 = shift @lines;

    # look for <space>|<space>
    my @colheads = split(/\s+\|\s+/, $line1);

    # fixup first, last column head (remove leading,trailing spaces)

    $colheads[0] =~ s/^\s+//;
    $colheads[0] =~ s/\s+$//;
    $colheads[-1] =~ s/^\s+//;
    $colheads[-1] =~ s/\s+$//;

    return undef
        unless (scalar(@lines));
    
    shift @lines; # skip dashed separator

    my @rows;

    for my $lin (@lines)
    {
        my @cols = split(/\|/, $lin, scalar(@colheads));
        last 
            unless (scalar(@cols) == scalar(@colheads));

        my $rowh = {};

        for my $colhd (@colheads)
        {
            my $rawcol = shift @cols;

            $rawcol =~ s/^\s+//;
            $rawcol =~ s/\s+$//;

            $rowh->{$colhd} = $rawcol;
        }
        push @rows, $rowh;
    }

    return \@rows;
}

sub do_ps_pids
{
    my $uid = shift;

    my $pidlist = [];
    my $pidh = {};

    my $cmd = 'ps auxww | grep -v grep | grep gptorment | grep ' . $uid;

    my $psout = `$cmd`;

    my @lines = split(/\n/, $psout);

    for my $lin (@lines)
    {
        my @foo = split(/\s+/, $lin, 3);

        next
            unless (3 == scalar(@foo));

        shift @foo;

        my $pid = shift @foo;

        push @{$pidlist}, $pid;

        $pidh->{$pid} = 1;

    }
    return ($pidh, $pidlist);
} # end do_ps_pids


sub init_monitor
{
    my ($uid, $iter, $tlim, $paral) = @_;

    my $mon_stat = {};

    $mon_stat->{uid}   = $uid;
    $mon_stat->{iter}  = $iter;
    $mon_stat->{tlim}  = $tlim;
    $mon_stat->{paral} = $paral;

    my ($pidh, $pidlist) = do_ps_pids($uid);
 
    $mon_stat->{orig_pidlist} = $pidlist;
    $mon_stat->{curr_pidlist} = [];
    push @{$mon_stat->{curr_pidlist}}, @{$pidlist};
    $mon_stat->{pidh} = $pidh;
    
    if ($paral != scalar(@{$pidlist}))
    {
        $mon_stat->{msg} = "WARNING: found " . scalar(@{$pidlist}) . 
            " processes, expected " . $paral . ".\n";
    }
    else
    {
        $mon_stat->{msg} = "Found " . $paral . " processes.\n"
    }

#    print Data::Dumper->Dump([$mon_stat]);

    return $mon_stat;
    
}

sub do_monitor
{
    my ($mon_stat, $tim, $test_fh) = @_;

#    print $$, ": ", Data::Dumper->Dump([$mon_stat]), "\nGH: ",
#    Data::Dumper->Dump([%GH]), "\n"
#        unless (exists($mon_stat->{uid}) 
#                && exists($mon_stat->{curr_pidlist}));

    my ($pidh, $pidlist) = do_ps_pids($mon_stat->{uid});

    if (exists($mon_stat->{msg}))
    {
        print $mon_stat->{msg};

        print $test_fh $mon_stat->{msg} if (defined($test_fh));

        delete $mon_stat->{msg};
    }

    my $pcount = scalar(@{$pidlist});
    my $old_pcount = scalar(@{$mon_stat->{curr_pidlist}});

    my $status_change = 1;
    my $mmm;
    if ($pcount < $old_pcount)
    {
        $mmm = time() . ": " . ($old_pcount - $pcount) . 
            " processes died, $pcount remaining\n";
    }
    elsif ($pcount > $old_pcount)
    {
        $mmm = time() . ": " . ($pcount - $old_pcount) . 
            " processes BORN, $pcount now remaining\n";

    }
    else
    {
        $mmm = time() . ": " . $pcount . " processes still remaining\n";
        $status_change = 0;
    }

    print $mmm;

    print $test_fh $mmm if (defined($test_fh) && $status_change);

    $mon_stat->{curr_pidlist} = $pidlist;
    $mon_stat->{pidh} = $pidh;

    sleep ($tim)
        if ($pcount);

    return $pcount;
}


sub do_kill_kill
{
    my ($uid, $iter, $tlim, $paral, $test_fh, $mon_stat, $maxtime) = @_;

    # kill if necessary

    do_monitor($mon_stat, 0, $test_fh);

    my $pcount = scalar(@{$mon_stat->{curr_pidlist}});

    my $mmm = time() . ": $pcount processes remain at test end\n";

    print $mmm;
    print $test_fh $mmm if (defined($test_fh));

    for my $pid (@{$mon_stat->{curr_pidlist}})
    {
        print "killing $pid\n";
        system ("kill -9 $pid");
    }

    if (!defined($maxtime))
    {
        $mmm = "test ended prematurely (no time limit specified)";

    }
    else
    {
        my $time_diff = $maxtime - time();

        if ($GH{grace} > 0)
        {
            if ($time_diff <= $GH{grace})
            {
                $mmm = "test ended on time (grace period $GH{grace})\n";
            }
            else
            {
                $mmm = "test ended $time_diff seconds prematurely\n";
            }
        }
        else
        {
            my $grt = (-1 * $GH{grace});

            if ($time_diff <= $grt)
            {
                $mmm = 
                    "test ended on time (negative grace period $GH{grace})\n";
            }
            else
            {
                $mmm = "test ended " . ($time_diff - $grt) .
                    " seconds prematurely\n";
            }
        }
    }

    print $mmm;
    print $test_fh $mmm if (defined($test_fh));

} # end do_kill_kill

sub do_kill
{
    my ($uid, $iter, $tlim, $paral, $test_fh) = @_;

    my $mon_stat = init_monitor($uid, $iter, $tlim, $paral);
    $GH{mon_stat} = $mon_stat;

    my $maxtime = time() + ($GH{iter} * $GH{tlim}) + $GH{grace};
    $GH{maxtime} = $maxtime;

    while (do_monitor($mon_stat, 10, $test_fh) && (time() < $maxtime))
    {
        $GH{mon_stat} = $mon_stat;
    }

    do_kill_kill($uid, $iter, $tlim, $paral, $test_fh, $mon_stat, $maxtime);
} # end do_kill

# cntrl c handler
sub do_handle_interrupt
{
    my $test_fh = $GH{test_fh};

    my $mmm = "\n" . time() . ": Test interrupted!\n";

    print $mmm;
    print $test_fh $mmm
        if (defined($test_fh));

#    print Data::Dumper->Dump([%GH]), "\n";

    if (defined($GH{mon_stat}) && defined($GH{parallel})
        && $GH{parallel} > 1)
    {
        $mmm = "\n" . time() . ": Attempting to clean up child processes\n";
        print $mmm;
        print $test_fh $mmm
            if (defined($test_fh));
        
        do_kill_kill($GH{uid}, $GH{iter}, $GH{tlim}, $GH{parallel}, 
                     $GH{test_fh}, $GH{mon_stat}, $GH{maxtime});
    }
    else
    {
        $mmm = "cleaning up...";
        print $mmm;
        print $test_fh $mmm
            if (defined($test_fh));

    }

    close $test_fh
        if (defined($test_fh));

    exit;
}

sub do_sql
{
    my $psql_str = "psql ";

    $psql_str .= $GH{connect}
        if (defined($GH{connect}));

#    $psql_str .= " -c \'\\d $glob_tab \'";
#    $psql_str .= " -c \'select * from gp_configuration \'";

    $psql_str .= " -a -e  -f $GH{sqlfile}";

#    print $psql_str, "\n";

    my $tstout = "gpt_" . $GH{uid};

    $tstout =~ s/\./\_/gm;

    $tstout .= ".out";

    $tstout = File::Spec->catfile($GH{tmpdir}, $tstout);

    my $tstfh;

    open ($tstfh, ">$tstout" ) or die "can't open $tstout: $!";

    $GH{test_fh} = $tstfh;

    if (($GH{parallel} == 1) && (!defined($GH{debug})))
    {
        my $start_time = time();
        my $end_time;
        $end_time = $start_time + $GH{tlim}
            if (defined($GH{tlim}));

#        print "sql tlim: $GH{tlim}\n"
#            if (defined($GH{tlim}));

        my $realSTDOUT;

        open $realSTDOUT, ">&STDOUT"  or die "Can't dup STDOUT: $!";

        for my $ii (1..$GH{iter})
        {
            my $filnam;

            $filnam = "sql_" . $GH{uid} . "_" . $ii;
            
            $filnam =~ s/\./\_/gm;

            $filnam .= ".out";

            my $tmpnam = File::Spec->catfile($GH{tmpdir}, $filnam);

            close STDOUT;
            open (STDOUT, ">$tmpnam" ) or die "can't open STDOUT: $!";

            select STDOUT; $| = 1;      # make unbuffered
            
            my $tl_iter = 0;

          l_timeloop:

            my $tabdef = `( $psql_str ) 2>&1`;

            print $tabdef;
            print $tstfh $tabdef;

            if (defined($end_time))
            {
                if (time() < $end_time)
                {
                    $tl_iter++;
                    print $tstfh "-- GP_IGNORE: timeloop $tl_iter, ", 
                    time(), "\n";
                    goto l_timeloop;
                }

#            undef $end_time;
                $end_time = time() + $GH{tlim};
            }
        }

        if (defined($realSTDOUT))
        {
            close STDOUT;
            open STDOUT, ">&", $realSTDOUT or die "Can't dup \$realSTDOUT: $!";
            
        }

    }
    else
    {
        my $do_para = $GH{parallel};

        if (defined($GH{debug}))
        {
            my $timetxt = localtime();
            print "# start at $timetxt\n";
            print "# repeat $do_para times...\n";
            $do_para = 1;
        }

        for my $ii (1..$do_para)
        {
            my $cmd;

            $cmd = $0 . " ";

            $cmd .= "--id=" . $GH{uid} . " ";
            $cmd .= "--conn=\'" . $GH{connect} . "\' "
                if (defined($GH{connect}));
            $cmd .= "--iter=" . $GH{iter} . " ";
            $cmd .= "--sqlfile=" . $GH{sqlfile} . " ";

            $cmd .= "--timelimit=\'" . $GH{tlim} . " seconds\' "
                if (defined($GH{tlim}));

            $cmd .= "--tempdir=\'" . $GH{tmpdir} . "\' "
                if (defined($GH{tmpdir}));

            $cmd .= "--grace_period=\'" . $GH{grace} . " seconds\' ";

            $cmd .= " & ";

            print $cmd, "\n";

            unless (defined($GH{debug}))
            {
                print $tstfh $cmd, "\n";
                system($cmd);
            }

        } # end for ii

        unless (defined($GH{debug}))
        {
            if (defined($GH{tlim}))
            {
                do_kill($GH{uid}, $GH{iter}, 
                        $GH{tlim}, $GH{parallel}, $tstfh);
            }
            else
            {
                my $mon_stat = 
                    init_monitor($GH{uid}, $GH{iter}, 
                                 $GH{tlim}, $GH{parallel});

                $GH{mon_stat} = $mon_stat;

                while (do_monitor($mon_stat, 10, $tstfh))
                {
                    $GH{mon_stat} = $mon_stat;
                }
            }
        }


    } # end else

    close $tstfh;
    undef $GH{test_fh};

} # end do_sql

foreach my $signal (qw (INT TERM HUP)) {
    $SIG{$signal} = \&do_handle_interrupt; # handle cntrl-c
}

if (1)
{
    if (defined($GH{sqlfile})
        && length($GH{sqlfile}))
    {
        do_sql();
    }

}

exit();

