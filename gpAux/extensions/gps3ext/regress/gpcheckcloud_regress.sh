#!/bin/bash

EXIT_CODE=0
GPCHECKCLOUD=../bin/gpcheckcloud/gpcheckcloud
RANDOM_PREFIX=gpcheckcloud-$(date +%Y%m%d)-$(cat /dev/urandom | env LC_CTYPE=C tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

echo "Preparing data to upload..."
dd if=/dev/urandom of=/tmp/gpcheckcloud.small bs=6 count=1 &> /dev/null
dd if=/dev/urandom of=/tmp/gpcheckcloud.large bs=78MB count=1 &> /dev/null

MD5SUM_SMALL=`cat /tmp/gpcheckcloud.small |openssl md5 |cut -d ' ' -f 2`
MD5SUM_LARGE=`cat /tmp/gpcheckcloud.large |openssl md5 |cut -d ' ' -f 2`

echo "Uploading data..."
$GPCHECKCLOUD -u "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/small/ config=/home/gpadmin/s3.conf" -f /tmp/gpcheckcloud.small
$GPCHECKCLOUD -u "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/large/ config=/home/gpadmin/s3.conf" -f /tmp/gpcheckcloud.large

echo "Downloading and checking hashsum..."
CHECK_CASES=(
# gpcheckcloud
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/small/ $MD5SUM_SMALL"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/large/ $MD5SUM_LARGE"
# normal files
"http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/data0014 68c4a63b721e7af0ae945ce109ca87ad"
"http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/data0016 0fd502a303eb8f138f5916ec357721b1"
"https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/data0014 68c4a63b721e7af0ae945ce109ca87ad"
"https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/data0016 0fd502a303eb8f138f5916ec357721b1"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gzipped/data0001.gz b958fb80b98605a6095e6ebc4b9b4786"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/threebytes/ fe7d81814e02eb1296757e75bb3c6be9"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/ 138fc555074671912125ba692c678246"
"s3://s3-us-east-1.amazonaws.com/us-east-1.s3test.pivotal.io/regress/small17/ 138fc555074671912125ba692c678246"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gzipped/ 7b2260e9a3a3f26e84aa28dc2124f68f"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gzipped_normal1/ eacb7b210d3f7703ee06d16f520b103e"
# huge files
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/hugefile/airlinedata1.csv f5811ad92c994f1d6913d5338575fe38"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gzipped_normal2/ a930794bc885bccf6eed45bd40367a7d"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/2001files/ 38c18f5ddf9ef68fef9ef9842c2aa180"
)

for ((i=0; i<${#CHECK_CASES[@]}; i++))
do
	PREFIX=`echo ${CHECK_CASES[$i]} |cut -d' ' -f 1`
	DIGEST=`echo ${CHECK_CASES[$i]} |cut -d' ' -f 2`
	[ `$GPCHECKCLOUD -d "$PREFIX config=/home/gpadmin/s3.conf" 2>/dev/null |openssl md5 |cut -d ' ' -f 2` = "$DIGEST" ] \
		&& echo test $PREFIX ... ok || { EXIT_CODE=1; echo test $PREFIX ... failed; }
done

exit $EXIT_CODE
