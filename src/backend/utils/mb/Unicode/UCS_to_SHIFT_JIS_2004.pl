#! /usr/bin/perl
#
# Copyright (c) 2007-2015, PostgreSQL Global Development Group
#
# src/backend/utils/mb/Unicode/UCS_to_SHIFT_JIS_2004.pl
#
# Generate UTF-8 <--> SHIFT_JIS_2004 code conversion tables from
# "sjis-0213-2004-std.txt" (http://x0213.org)

require "ucs2utf.pl";

# first generate UTF-8 --> SHIFT_JIS_2004 table

$in_file = "sjis-0213-2004-std.txt";

open(FILE, $in_file) || die("cannot open $in_file");

reset 'array';
reset 'array1';
reset 'comment';
reset 'comment1';

while ($line = <FILE>)
{
	if ($line =~ /^0x(.*)[ \t]*U\+(.*)\+(.*)[ \t]*#(.*)$/)
	{
		$c              = $1;
		$u1             = $2;
		$u2             = $3;
		$rest           = "U+" . $u1 . "+" . $u2 . $4;
		$code           = hex($c);
		$ucs            = hex($u1);
		$utf1           = &ucs2utf($ucs);
		$ucs            = hex($u2);
		$utf2           = &ucs2utf($ucs);
		$str            = sprintf "%08x%08x", $utf1, $utf2;
		$array1{$str}   = $code;
		$comment1{$str} = $rest;
		$count1++;
		next;
	}
	elsif ($line =~ /^0x(.*)[ \t]*U\+(.*)[ \t]*#(.*)$/)
	{
		$c    = $1;
		$u    = $2;
		$rest = "U+" . $u . $3;
	}
	else
	{
		next;
	}

	$ucs  = hex($u);
	$code = hex($c);
	$utf  = &ucs2utf($ucs);
	if ($array{$utf} ne "")
	{
		printf STDERR
		  "Warning: duplicate UTF8: %08x UCS: %04x Shift JIS: %04x\n", $utf,
		  $ucs, $code;
		next;
	}
	$count++;

	$array{$utf}    = $code;
	$comment{$code} = $rest;
}
close(FILE);

$file = "utf8_to_shift_jis_2004.map";
open(FILE, "> $file") || die("cannot open $file");
print FILE "/*\n";
print FILE " * This file was generated by UCS_to_SHIFT_JIS_2004.pl\n";
print FILE " */\n";
print FILE "static const pg_utf_to_local ULmapSHIFT_JIS_2004[] = {\n";

for $index (sort { $a <=> $b } keys(%array))
{
	$code = $array{$index};
	$count--;
	if ($count == 0)
	{
		printf FILE "  {0x%08x, 0x%06x}	/* %s */\n", $index, $code,
		  $comment{$code};
	}
	else
	{
		printf FILE "  {0x%08x, 0x%06x},	/* %s */\n", $index, $code,
		  $comment{$code};
	}
}

print FILE "};\n";
close(FILE);

$file = "utf8_to_shift_jis_2004_combined.map";
open(FILE, "> $file") || die("cannot open $file");
print FILE "/*\n";
print FILE " * This file was generated by UCS_to_SHIFT_JIS_2004.pl\n";
print FILE " */\n";
print FILE
"static const pg_utf_to_local_combined ULmapSHIFT_JIS_2004_combined[] = {\n";

for $index (sort { $a cmp $b } keys(%array1))
{
	$code = $array1{$index};
	$count1--;
	if ($count1 == 0)
	{
		printf FILE "  {0x%s, 0x%s, 0x%04x}	/* %s */\n", substr($index, 0, 8),
		  substr($index, 8, 8), $code, $comment1{$index};
	}
	else
	{
		printf FILE "  {0x%s, 0x%s, 0x%04x},	/* %s */\n",
		  substr($index, 0, 8), substr($index, 8, 8), $code,
		  $comment1{$index};
	}
}

print FILE "};\n";
close(FILE);

# then generate SHIFT_JIS_2004 --> UTF-8 table

$in_file = "sjis-0213-2004-std.txt";

open(FILE, $in_file) || die("cannot open $in_file");

reset 'array';
reset 'array1';
reset 'comment';
reset 'comment1';

while ($line = <FILE>)
{
	if ($line =~ /^0x(.*)[ \t]*U\+(.*)\+(.*)[ \t]*#(.*)$/)
	{
		$c               = $1;
		$u1              = $2;
		$u2              = $3;
		$rest            = "U+" . $u1 . "+" . $u2 . $4;
		$code            = hex($c);
		$ucs             = hex($u1);
		$utf1            = &ucs2utf($ucs);
		$ucs             = hex($u2);
		$utf2            = &ucs2utf($ucs);
		$str             = sprintf "%08x%08x", $utf1, $utf2;
		$array1{$code}   = $str;
		$comment1{$code} = $rest;
		$count1++;
		next;
	}
	elsif ($line =~ /^0x(.*)[ \t]*U\+(.*)[ \t]*#(.*)$/)
	{
		$c    = $1;
		$u    = $2;
		$rest = "U+" . $u . $3;
	}
	else
	{
		next;
	}

	$ucs  = hex($u);
	$code = hex($c);
	$utf  = &ucs2utf($ucs);
	if ($array{$code} ne "")
	{
		printf STDERR
		  "Warning: duplicate UTF-8: %08x UCS: %04x Shift JIS: %04x\n", $utf,
		  $ucs, $code;
		printf STDERR "Previous value: UTF-8: %08x\n", $array{$utf};
		next;
	}
	$count++;

	$array{$code}  = $utf;
	$comment{$utf} = $rest;
}
close(FILE);

$file = "shift_jis_2004_to_utf8.map";
open(FILE, "> $file") || die("cannot open $file");
print FILE "/*\n";
print FILE " * This file was generated by UCS_to_SHIFTJIS_2004.pl\n";
print FILE " */\n";
print FILE "static const pg_local_to_utf LUmapSHIFT_JIS_2004[] = {\n";

for $index (sort { $a <=> $b } keys(%array))
{
	$code = $array{$index};
	$count--;
	if ($count == 0)
	{
		printf FILE "  {0x%04x, 0x%08x}	/* %s */\n", $index, $code,
		  $comment{$code};
	}
	else
	{
		printf FILE "  {0x%04x, 0x%08x},	/* %s */\n", $index, $code,
		  $comment{$code};
	}
}

print FILE "};\n";
close(FILE);

$file = "shift_jis_2004_to_utf8_combined.map";
open(FILE, "> $file") || die("cannot open $file");
print FILE "/*\n";
print FILE " * This file was generated by UCS_to_SHIFT_JIS_2004.pl\n";
print FILE " */\n";
print FILE
"static const pg_local_to_utf_combined LUmapSHIFT_JIS_2004_combined[] = {\n";

for $index (sort { $a <=> $b } keys(%array1))
{
	$code = $array1{$index};
	$count1--;
	if ($count1 == 0)
	{
		printf FILE "  {0x%04x, 0x%s, 0x%s}	/* %s */\n", $index,
		  substr($code, 0, 8), substr($code, 8, 8), $comment1{$index};
	}
	else
	{
		printf FILE "  {0x%04x, 0x%s, 0x%s},	/* %s */\n", $index,
		  substr($code, 0, 8), substr($code, 8, 8), $comment1{$index};
	}
}

print FILE "};\n";
close(FILE);
