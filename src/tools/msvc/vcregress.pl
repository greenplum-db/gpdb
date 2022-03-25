# -*-perl-*- hey - emacs - this is a perl file

# src/tools/msvc/vcregress.pl

use strict;

our $config;

use Cwd;
use File::Basename;
use File::Copy;
use File::Find ();
use File::Spec;
BEGIN  { use lib File::Spec->rel2abs(dirname(__FILE__)); }

use Install qw(Install);

my $startdir = getcwd();

chdir "../../.." if (-d "../../../src/tools/msvc");

my $topdir = getcwd();
my $tmp_installdir = "$topdir/tmp_install";

do './src/tools/msvc/config_default.pl';
do './src/tools/msvc/config.pl' if (-f 'src/tools/msvc/config.pl');

# buildenv.pl is for specifying the build environment settings
# it should contain lines like:
# $ENV{PATH} = "c:/path/to/bison/bin;$ENV{PATH}";

if (-e "src/tools/msvc/buildenv.pl")
{
	do "./src/tools/msvc/buildenv.pl";
}

my $what = shift || "";
if ($what =~
/^(check|installcheck|plcheck|contribcheck|ecpgcheck|isolationcheck|upgradecheck|bincheck|taptest)$/i
  )
{
	$what = uc $what;
}
else
{
	usage();
}

# use a capital C here because config.pl has $config
my $Config = -e "release/postgres/postgres.exe" ? "Release" : "Debug";

copy("$Config/refint/refint.dll",                 "src/test/regress");
copy("$Config/autoinc/autoinc.dll",               "src/test/regress");
copy("$Config/regress/regress.dll",               "src/test/regress");
copy("$Config/dummy_seclabel/dummy_seclabel.dll", "src/test/regress");

$ENV{PATH} = "../../../$Config/libpq;../../$Config/libpq;$ENV{PATH}";

if ($ENV{PERL5LIB})
{
	$ENV{PERL5LIB} = "$topdir/src/tools/msvc;$ENV{PERL5LIB}";
}
else
{
	$ENV{PERL5LIB} = "$topdir/src/tools/msvc";
}

my $maxconn = "";
$maxconn = "--max_connections=$ENV{MAX_CONNECTIONS}"
  if $ENV{MAX_CONNECTIONS};

my $temp_config = "";
$temp_config = "--temp-config=\"$ENV{TEMP_CONFIG}\""
  if $ENV{TEMP_CONFIG};

chdir "src/test/regress";

my %command = (
	CHECK          => \&check,
	PLCHECK        => \&plcheck,
	INSTALLCHECK   => \&installcheck,
	ECPGCHECK      => \&ecpgcheck,
	CONTRIBCHECK   => \&contribcheck,
	ISOLATIONCHECK => \&isolationcheck,
	BINCHECK       => \&bincheck,
	UPGRADECHECK   => \&upgradecheck,
	TAPTEST        => \&taptest,);

my $proc = $command{$what};

exit 3 unless $proc;

&$proc(@ARGV);

exit 0;

########################################################################

sub installcheck
{
	my $schedule = shift || 'serial';
	my @args = (
		"../../../$Config/pg_regress/pg_regress",
		"--dlpath=.",
		"--psqldir=../../../$Config/psql",
		"--schedule=${schedule}_schedule",
		"--encoding=SQL_ASCII",
		"--no-locale");
	push(@args, $maxconn) if $maxconn;
	system(@args);
	my $status = $? >> 8;
	exit $status if $status;
}

sub check
{
	my $schedule = shift || 'parallel';
	my @args = (
		"../../../$Config/pg_regress/pg_regress",
		"--dlpath=.",
		"--psqldir=../../../$Config/psql",
		"--schedule=${schedule}_schedule",
		"--encoding=SQL_ASCII",
		"--no-locale",
		"--temp-install=./tmp_check",
		"--top-builddir=\"$topdir\"");
	push(@args, $maxconn)     if $maxconn;
	push(@args, $temp_config) if $temp_config;
	system(@args);
	my $status = $? >> 8;
	exit $status if $status;
}

sub ecpgcheck
{
	chdir $startdir;
	system("msbuild ecpg_regression.proj /p:config=$Config");
	my $status = $? >> 8;
	exit $status if $status;
	chdir "$topdir/src/interfaces/ecpg/test";
	my $schedule = "ecpg";
	my @args     = (
		"../../../../$Config/pg_regress_ecpg/pg_regress_ecpg",
		"--psqldir=../../../$Config/psql",
		"--dbname=regress1,connectdb",
		"--create-role=connectuser,connectdb",
		"--schedule=${schedule}_schedule",
		"--encoding=SQL_ASCII",
		"--no-locale",
		"--temp-install=./tmp_chk",
		"--top-builddir=\"$topdir\"");
	push(@args, $maxconn) if $maxconn;
	system(@args);
	$status = $? >> 8;
	exit $status if $status;
}

sub isolationcheck
{
	chdir "../isolation";
	copy("../../../$Config/isolationtester/isolationtester.exe",
		"../../../$Config/pg_isolation_regress");
	my @args = (
		"../../../$Config/pg_isolation_regress/pg_isolation_regress",
		"--psqldir=../../../$Config/psql",
		"--inputdir=.",
		"--schedule=./isolation_schedule");
	push(@args, $maxconn) if $maxconn;
	system(@args);
	my $status = $? >> 8;
	exit $status if $status;
}

sub tap_check
{
	die "Tap tests not enabled in configuration"
	  unless $config->{tap_tests};

	my @flags;
	foreach my $arg (0 .. scalar(@_))
	{
		next unless $_[$arg] =~ /^PROVE_FLAGS=(.*)/;
		@flags = split(/\s+/, $1);
		splice(@_,$arg,1);
		last;
	}

	my $dir = shift;
	chdir $dir;

	my @args = ("prove", @flags, glob("t/*.pl"));

	# adjust the environment for just this test
	local %ENV = %ENV;
	$ENV{PERL5LIB} = "$topdir/src/test/perl;$ENV{PERL5LIB}";
	$ENV{PG_REGRESS} = "$topdir/$Config/pg_regress/pg_regress";
	$ENV{REGRESS_SHLIB} = "$topdir/src/test/regress/regress.dll";

	$ENV{TESTDIR} = "$dir";

	system(@args);
	my $status = $? >> 8;
	return $status;
}

sub bincheck
{
	InstallTemp();

	my $mstat = 0;

	# Find out all the existing TAP tests by looking for t/ directories
	# in the tree.
	my @bin_dirs = glob("$topdir/src/bin/*");

	# Process each test
	foreach my $dir (@bin_dirs)
	{
		next unless -d "$dir/t";
		my $status = tap_check($dir);
		$mstat ||= $status;
	}
	exit $mstat if $mstat;
}

sub taptest
{
	my $dir = shift;
	my @args;

	if ($dir =~ /^PROVE_FLAGS=/)
	{
		push(@args, $dir);
		$dir = shift;
	}

	die "no tests found!" unless -d "$topdir/$dir/t";

	push(@args,"$topdir/$dir");

	InstallTemp();
	my $status = tap_check(@args);
	exit $status if $status;
}

sub mangle_plpython3
{
	my $tests = shift;
	mkdir "results" unless -d "results";
	mkdir "sql/python3";
	mkdir "results/python3";
	mkdir "expected/python3";

	foreach my $test (@$tests)
	{
		local $/ = undef;
		foreach my $dir ('sql','expected')
		{
			my $extension = ($dir eq 'sql' ? 'sql' : 'out');

			my @files = glob("$dir/$test.$extension $dir/${test}_[0-9].$extension");
			foreach my $file (@files)
			{
				open(my $handle, '<', $file) || die "test file $file not found";
				my $contents = <$handle>;
				close($handle);
				do
				{
					s/except ([[:alpha:]][[:alpha:].]*), *([[:alpha:]][[:alpha:]]*):/except $1 as $2:/g;
					s/<type 'exceptions\.([[:alpha:]]*)'>/<class '$1'>/g;
					s/<type 'long'>/<class 'int'>/g;
					s/([0-9][0-9]*)L/$1/g;
					s/([ [{])u"/$1"/g;
					s/([ [{])u'/$1'/g;
					s/def next/def __next__/g;
					s/LANGUAGE plpython2?u/LANGUAGE plpython3u/g;
					s/EXTENSION ([^ ]*_)*plpython2?u/EXTENSION $1plpython3u/g;
					s/installing required extension "plpython2u"/installing required extension "plpython3u"/g;
				} for ($contents);
				my $base = basename $file;
				open($handle, '>', "$dir/python3/$base") ||
				  die "opening python 3 file for $file";
				print $handle $contents;
				close($handle);
			}
		}
	}
	do { s!^!python3/!; } foreach(@$tests);
	return @$tests;
}

sub plcheck
{
	chdir "../../pl";

	foreach my $pl (glob("*"))
	{
		next unless -d "$pl/sql" && -d "$pl/expected";
		my $lang = $pl eq 'tcl' ? 'pltcl' : $pl;
		if ($lang eq 'plpython')
		{
			next unless -d "$topdir/$Config/plpython2" ||
				-d "$topdir/$Config/plpython3";
			$lang = 'plpythonu';
		}
		else
		{
			next unless -d "../../$Config/$lang";
		}
		my @lang_args = ("--load-extension=$lang");
		chdir $pl;
		my @tests = fetchTests();
		@tests = mangle_plpython3(\@tests)
			if $lang eq 'plpythonu' && -d "$topdir/$Config/plpython3";
		if ($lang eq 'plperl')
		{

			# run both trusted and untrusted perl tests
			push(@lang_args, "--load-extension=plperlu");

			# assume we're using this perl to built postgres
			# test if we can run two interpreters in one backend, and if so
			# run the trusted/untrusted interaction tests
			use Config;
			if ($Config{usemultiplicity} eq 'define')
			{
				push(@tests, 'plperl_plperlu');
			}
		}
		elsif ($lang eq 'plpythonu' && -d "$topdir/$Config/plpython3")
		{
			@lang_args = ();
		}
		print
		  "============================================================\n";
		print "Checking $lang\n";
		my @args = (
			"../../../$Config/pg_regress/pg_regress",
			"--psqldir=../../../$Config/psql",
			"--dbname=pl_regression", @lang_args, @tests);
		system(@args);
		my $status = $? >> 8;
		exit $status if $status;
		chdir "..";
	}

	chdir "../../..";
}

sub contribcheck
{
	chdir "../../../contrib";
	my $mstat = 0;
	foreach my $module (glob("*"))
	{
		# these configuration-based exclusions must match Install.pm
		next if ($module eq "uuid-ossp" && !defined($config->{uuid}));
		next if ($module eq "sslinfo"   && !defined($config->{openssl}));
		next if ($module eq "xml2"      && !defined($config->{xml}));
		next if ($module eq "sepgsql");

		next
		  unless -d "$module/sql"
			  && -d "$module/expected"
			  && (-f "$module/GNUmakefile" || -f "$module/Makefile");
		chdir $module;
		print
		  "============================================================\n";
		print "Checking $module\n";
		my @tests = fetchTests();
		my @opts  = fetchRegressOpts();
		my @args  = (
			"../../$Config/pg_regress/pg_regress",
			"--psqldir=../../$Config/psql",
			"--dbname=contrib_regression", @opts, @tests);
		system(@args);
		my $status = $? >> 8;
		$mstat ||= $status;
		chdir "..";
	}
	exit $mstat if $mstat;
}

# Run "initdb", then reconfigure authentication.
sub standard_initdb
{
	return (
		system('initdb', '-N') == 0 and system(
			"$topdir/$Config/pg_regress/pg_regress", '--config-auth',
			$ENV{PGDATA}) == 0);
}

# This is similar to appendShellString().  Perl system(@args) bypasses
# cmd.exe, so omit the caret escape layer.
sub quote_system_arg
{
	my $arg = shift;

	# Change N >= 0 backslashes before a double quote to 2N+1 backslashes.
	$arg =~ s/(\\*)"/${\($1 . $1)}\\"/gs;

	# Change N >= 1 backslashes at end of argument to 2N backslashes.
	$arg =~ s/(\\+)$/${\($1 . $1)}/gs;

	# Wrap the whole thing in unescaped double quotes.
	return "\"$arg\"";
}

# Generate a database with a name made of a range of ASCII characters, useful
# for testing pg_upgrade.
sub generate_db
{
	my ($prefix, $from_char, $to_char, $suffix) = @_;

	my $dbname = $prefix;
	for my $i ($from_char .. $to_char)
	{
		next if $i == 7 || $i == 10 || $i == 13;    # skip BEL, LF, and CR
		$dbname = $dbname . sprintf('%c', $i);
	}
	$dbname .= $suffix;

	system('createdb', quote_system_arg($dbname));
	my $status = $? >> 8;
	exit $status if $status;
}

sub upgradecheck
{
	my $status;
	my $cwd = getcwd();

	# Much of this comes from the pg_upgrade test.sh script,
	# but it only covers the --install case, and not the case
	# where the old and new source or bin dirs are different.
	# i.e. only this version to this version check. That's
	# what pg_upgrade's "make check" does.

	$ENV{PGHOST} = 'localhost';
	$ENV{PGPORT} ||= 50432;
	my $tmp_root = "$topdir/contrib/pg_upgrade/tmp_check";
	(mkdir $tmp_root || die $!) unless -d $tmp_root;
	my $tmp_install = "$tmp_root/install";
	print "Setting up temp install\n\n";
	Install($tmp_install, "all", $config);

	# Install does a chdir, so change back after that
	chdir $cwd;
	my ($bindir, $libdir, $oldsrc, $newsrc) =
	  ("$tmp_install/bin", "$tmp_install/lib", $topdir, $topdir);
	$ENV{PATH} = "$bindir;$ENV{PATH}";
	my $data = "$tmp_root/data";
	$ENV{PGDATA} = "$data.old";
	my $logdir = "$topdir/contrib/pg_upgrade/log";
	(mkdir $logdir || die $!) unless -d $logdir;
	print "\nRunning initdb on old cluster\n\n";
	standard_initdb() or exit 1;
	print "\nStarting old cluster\n\n";
	my @args = ('pg_ctl', 'start', '-l', "$logdir/postmaster1.log", '-w');
	system(@args) == 0 or exit 1;

	print "\nCreating databases with names covering most ASCII bytes\n\n";
	generate_db("\\\"\\", 1,  45,  "\\\\\"\\\\\\");
	generate_db('',       46, 90,  '');
	generate_db('',       91, 127, '');

	print "\nSetting up data for upgrading\n\n";
	installcheck();

	# now we can chdir into the source dir
	chdir "$topdir/contrib/pg_upgrade";
	print "\nDumping old cluster\n\n";
	@args = ('pg_dumpall', '-f', "$tmp_root/dump1.sql");
	system(@args) == 0 or exit 1;
	print "\nStopping old cluster\n\n";
	system("pg_ctl -m fast stop") == 0 or exit 1;
	$ENV{PGDATA} = "$data";
	print "\nSetting up new cluster\n\n";
	standard_initdb() or exit 1;
	print "\nRunning pg_upgrade\n\n";
	@args = ('pg_upgrade', '-d', "$data.old", '-D', $data, '-b', $bindir,
			 '-B', $bindir);
	system(@args) == 0 or exit 1;
	print "\nStarting new cluster\n\n";
	@args = ('pg_ctl', '-l', "$logdir/postmaster2.log", '-w', 'start');
	system(@args) == 0 or exit 1;
	print "\nSetting up stats on new cluster\n\n";
	system(".\\analyze_new_cluster.bat") == 0 or exit 1;
	print "\nDumping new cluster\n\n";
	@args = ('pg_dumpall', '-f', "$tmp_root/dump2.sql");
	system(@args) == 0 or exit 1;
	print "\nStopping new cluster\n\n";
	system("pg_ctl -m fast stop") == 0 or exit 1;
	print "\nDeleting old cluster\n\n";
	system(".\\delete_old_cluster.bat") == 0 or exit 1;
	print "\nComparing old and new cluster dumps\n\n";

	@args = ('diff', '-q', "$tmp_root/dump1.sql", "$tmp_root/dump2.sql");
	system(@args);
	$status = $?;
	if (!$status)
	{
		print "PASSED\n";
	}
	else
	{
		print "dumps not identical!\n";
		exit(1);
	}
}

sub fetchRegressOpts
{
	my $handle;
	open($handle, "<GNUmakefile")
	  || open($handle, "<Makefile")
	  || die "Could not open Makefile";
	local ($/) = undef;
	my $m = <$handle>;
	close($handle);
	my @opts;

	$m =~ s{\\\r?\n}{}g;
	if ($m =~ /^\s*REGRESS_OPTS\s*=(.*)/m)
	{
		# Substitute known Makefile variables, then ignore options that retain
		# an unhandled variable reference.  Ignore anything that isn't an
		# option starting with "--".
		@opts = grep {
			s/\Q$(top_builddir)\E/\"$topdir\"/;
			$_ !~ /\$\(/ && $_ =~ /^--/
		} split(/\s+/, $1);
	}
	if ($m =~ /^\s*ENCODING\s*=\s*(\S+)/m)
	{
		push @opts, "--encoding=$1";
	}
	if ($m =~ /^\s*NO_LOCALE\s*=\s*\S+/m)
	{
		push @opts, "--no-locale";
	}
	return @opts;
}

sub fetchTests
{

	my $handle;
	open($handle, "<GNUmakefile")
	  || open($handle, "<Makefile")
	  || die "Could not open Makefile";
	local ($/) = undef;
	my $m = <$handle>;
	close($handle);
	my $t = "";

	$m =~ s/\\[\r\n]*//gs;
	if ($m =~ /^REGRESS\s*=\s*(.*)$/gm)
	{
		$t = $1;
		$t =~ s/\s+/ /g;

		if ($m =~ /contrib\/pgcrypto/)
		{

			# pgcrypto is special since the tests depend on the
			# configuration of the build

			my $cftests =
			  $config->{openssl}
			  ? GetTests("OSSL_TESTS", $m)
			  : GetTests("INT_TESTS",  $m);
			my $pgptests =
			  $config->{zlib}
			  ? GetTests("ZLIB_TST",     $m)
			  : GetTests("ZLIB_OFF_TST", $m);
			$t =~ s/\$\(CF_TESTS\)/$cftests/;
			$t =~ s/\$\(CF_PGP_TESTS\)/$pgptests/;
		}
	}

	return split(/\s+/, $t);
}

sub GetTests
{
	my $testname = shift;
	my $m        = shift;
	if ($m =~ /^$testname\s*=\s*(.*)$/gm)
	{
		return $1;
	}
	return "";
}

sub InstallTemp
{
	unless ($ENV{NO_TEMP_INSTALL})
	{
		print "Setting up temp install\n\n";
		Install("$tmp_installdir", "all", $config);
	}
	$ENV{PATH} = "$tmp_installdir/bin;$ENV{PATH}";
}

sub usage
{
	print STDERR
	  "Usage: vcregress.pl <mode> [ <arg>]\n\n",
	  "Options for <mode>:\n",
	  "  bincheck       run tests of utilities in src/bin/\n",
	  "  check          deploy instance and run regression tests on it\n",
	  "  contribcheck   run tests of modules in contrib/\n",
	  "  ecpgcheck      run regression tests of ECPG\n",
	  "  installcheck   run regression tests on existing instance\n",
	  "  isolationcheck run isolation tests\n",
	  "  plcheck        run tests of PL languages\n",
	  "  taptest        run an arbitrary TAP test set\n",
	  "  upgradecheck   run tests of pg_upgrade\n",
	  "\nOptions for <arg>: (used by check and installcheck)\n",
	  "  serial         serial mode\n",
	  "  parallel       parallel mode\n",
	  "\nOption for <arg>: for taptest\n",
	  "  TEST_DIR       (required) directory where tests reside\n";
	exit(1);
}
