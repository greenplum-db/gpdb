#!/usr/bin/env perl
#
# Portions Copyright (c) 2007-2010, Greenplum Inc
# Portions Copyright (c) 2012-Present Pivotal Software, Inc.
# Author: Jeffrey I Cohen
#
#
use Pod::Usage;
use Getopt::Long;
use Data::Dumper;
use strict;
use warnings;

=head1 NAME

B<dld.pl> - [D]ead[L]ock [D]etector

=head1 SYNOPSIS

B<dld> [options] 

Options:

    -help            brief help message
    -man             full documentation
    -connect         psql connect parameters

=head1 OPTIONS

=over 8

=item B<-help>

    Print a brief help message and exits.

=item B<-man>
    
    Prints the manual page and exits.

=item B<-connect>
    
    psql connect string, e.g:

    -connect '-p 11000 template1'

=back

=head1 DESCRIPTION

dld.pl finds (but does not fix) deadlocks

perl dld.pl -connect '-p 11000 template1'

Substitute the correct "connect string" for your postgres database.

=head1 AUTHORS

Jeffrey I Cohen

Portions Copyright (c) 2007-2010 GreenPlum.  All rights reserved.
Portions Copyright (c) 2012-Present Pivotal Software, Inc.

Address bug reports and comments to: bugs@greenplum.org


=cut

my $glob_id = "";

my $glob_tab;
my $glob_connect;
my $glob_verbose = 1;


BEGIN {
    my $man  = 0;
    my $help = 0;
    my $table;
    my $conn;

    GetOptions(
               'help|?' => \$help, man => \$man, 
               "table=s" => \$table,
               "connect=s" => \$conn
               )
        or pod2usage(2);

    
    pod2usage(-msg => $glob_id, -exitstatus => 1) if $help;
    pod2usage(-msg => $glob_id, -exitstatus => 0, -verbose => 2) if $man;

    $glob_tab = $table;
    $glob_connect = $conn;

    $glob_connect = '-p 11000 template1'
        unless (defined($glob_connect));

    $glob_verbose = 1;

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

sub walk_graph
{
    my ($wfg, $key, $visited) = @_;

    if ($glob_verbose && defined($key))
    {
        print "key: $key\n";
        print "visit: ", join("-> ", @{$visited}), "\n";
    }

    return 0
        unless (defined($wfg) && (ref($wfg) eq 'HASH'));

    my @wfg_keys = keys(%{$wfg});
    
    unless (defined($visited))
    {
        for my $k1 (@wfg_keys)
        {
            my $stat = walk_graph($wfg, $k1, [$k1]);
            return 0
                unless ($stat);
        }
        return 1;
    }

    my $k1 = $key;

    return 1
        unless (exists($wfg->{$k1}));

    {
        for my $holder (@{$wfg->{$k1}->{primary}})
        {
            for my $itm (@{$visited})
            {
                if ($itm eq $holder)
                {
                    print "cycle detected!\n";
                    my $first = $visited->[0];
                    print join(" -> ", @{$visited}, $first), "\n";
                    return 0;
                }
            }
            my $v2;
            $v2 = [];
            push @{$v2}, @{$visited}, $holder;
            return 0
                unless (walk_graph($wfg, $holder, $v2));
        }
    }


    return 1;
}


if (1)
{
    my $psql_str = "psql ";

    $psql_str .= $glob_connect
        if (defined($glob_connect));

    # need to deal with mirrors, etc
#    $psql_str .= " -c \'select * from gp_configuration where definedprimary is true or content > 0\'";
    $psql_str .= " -c \'select * from gp_segment_configuration where role=" .
		# use "dollar-quoting" to avoid dealing with nested single-quotes:
		# $q$p$q$ is equivalent to 'p'
		'$q$p$q$' . " \'";

#    print $psql_str, "\n";

    my $tabdef = `$psql_str`;

    print $tabdef 
        if ($glob_verbose);

    my $mpp_config_table = tablelizer($tabdef);

    # some locks are on txns, not relations, so no relnames
    my $sel_str = "\'select cl.relname as relname, lk.* from pg_locks as lk left outer join pg_class as cl on cl.relfilenode = lk.relation\'";

    my @combo_tab;
    my @rel_list;
    my @txn_list;

    for my $rowh (@{$mpp_config_table})
    {
#        print Data::Dumper->Dump([$vv]), "\n";
        

        my $psql_seg = "PGOPTIONS=\'-c gp_role=utility\' psql -h $rowh->{hostname} -p $rowh->{port} template1 -c $sel_str";

        print $psql_seg,"\n"
            if ($glob_verbose);

        my $lk1 = `$psql_seg`;

        print $lk1
            if ($glob_verbose);

        my $tabh = tablelizer($lk1);

#        print Data::Dumper->Dump([$tabh]), "\n";
        for my $rr (@{$tabh})
        {
            $rr->{segid} = $rowh->{content};

            # look for waiters - "granted = [f]alse"
            if ($rr->{granted} =~ m/\s*f\s*/ )
            { 
                if ($rr->{locktype} =~ m/transactionid/)
                {
                    push @txn_list, $rr->{transactionid};
                }
                else
                {
                    push @rel_list, $rr->{relation};
                }
            }

        }

        push @combo_tab, @{$tabh};

    }

    # find all txns with same tables as waiters

    my @wait_tab;

    my $rel_str = join(", ", @rel_list);
    my $txn_str = join(", ", @txn_list);

#    print "rel_str: $rel_str\ntxn_str: $txn_str\n";

    $sel_str = "\'select cl.relname as relname, lk.* from pg_locks as lk left outer join pg_class as cl on cl.relfilenode = lk.relation ";

    if (scalar(@rel_list) || scalar(@txn_list))
    {
        $sel_str .= " \'";

        for my $rowh (@{$mpp_config_table})
        {
#        print Data::Dumper->Dump([$vv]), "\n";
        

            my $psql_seg = "PGOPTIONS=\'-c gp_role=utility\' psql -h $rowh->{hostname}  -p $rowh->{port} template1 -c $sel_str";

            my $lk1 = `$psql_seg`;

            print $lk1
                if ($glob_verbose);

            my $tabh = tablelizer($lk1);

#        print Data::Dumper->Dump([$tabh]), "\n";
            for my $rr (@{$tabh})
            {
                $rr->{segid} = $rowh->{content};
#        map {$_->{segid} = $mpp_config_table->{content} },  @{$tabh};
            }

            push @wait_tab, @{$tabh};

        }
    }

    print "wait_tab:", Data::Dumper->Dump(\@wait_tab), "\n"
        if ($glob_verbose);
#    print Data::Dumper->Dump(\@combo_tab), "\n";

    my %holders;
    my %waiters;

    # find lock holders and lock waiters per relation

    for my $wrow (@wait_tab)
    {
        my $reltn = $wrow->{relation};

        next
            unless (defined($reltn));

        if ($wrow->{granted} eq 't')
        {
            unless (exists($holders{$reltn}))
            {
                $holders{$reltn} = [];
            }
            push @{$holders{$reltn}}, $wrow;
        }
        else
        {
            unless (exists($waiters{$reltn}))
            {
                $waiters{$reltn} = [];
            }
            push @{$waiters{$reltn}}, $wrow;
        }
    }

    my %wfg; # WAIT FOR GRAPH by segment, pid

    while ( my ($kk, $vv) = each(%waiters))
    {
        for my $waititm (@{$vv})
        {
            my $big_id = $waititm->{segid} . "/" . $waititm->{pid};

            unless (exists($wfg{$big_id}))
            {
                $wfg{$big_id} = {primary => []};
            }

            unless (exists($holders{$kk}))
            {
                print "no lock holder for relation $kk!!\n";
                next;
            }

            for my $holditm (@{$holders{$kk}})
            {
                my $h_id = $holditm->{segid} . "/" . $holditm->{pid};

                # don't wait on yourself
                next
                    if ($h_id eq $big_id);

                push @{$wfg{$big_id}->{primary}}, $h_id;
            }
        }

    }
        
    if ($glob_verbose)
    {
        print "waiters:\n",Data::Dumper->Dump([%waiters]), "\n";
        print "holders:\n",Data::Dumper->Dump([%holders]), "\n";
        print "wfg:\n",Data::Dumper->Dump([%wfg]), "\n";
    }

    walk_graph(\%wfg);

}

exit();

