#!/usr/bin/perl

## A driver script to implement PageRank via multiple iterations of a MapReduce
## job.
##
## Input specified in pagerank-init.yml, currently a pipe-delimited file.
## Output specified in pagerank-final.yml, currently a table called pagerank.
##
## Flags for this script:
##    -i ITERATIONS to control the number of iterations.
##    All other options passed to gpmapreduce *and* psql.

use Getopt::Std;


%args = ();
getopt("ihpUW", \%args);


$iterations = $args{i} || 10;
delete($args{i});
$params = " ";
while (($key, $value) = each(%args)) {
	$params .= "-" . $key . " " . $value;
}
$init_str = "gpmapreduce " . $params . " -f /data/tangp3/tincrepo/main/mapreduce/demo/PageRank/pagerank-init.yml";
$iter_str = "gpmapreduce " . $params . " -f /data/tangp3/tincrepo/main/mapreduce/demo/PageRank/pagerank-iter.yml";
$final_str = "gpmapreduce " . $params . " -f /data/tangp3/tincrepo/main/mapreduce/demo/PageRank/pagerank-final.yml";
$psql_str = "psql " . $params . ' -c "drop table pagerank_source; alter table pagerank_next rename to pagerank_source;"';

system ($init_str);
for (my $i = 0;  $i < $iterations; $i++) {
	system ($iter_str);
	system ($psql_str);
}
system ($final_str);
