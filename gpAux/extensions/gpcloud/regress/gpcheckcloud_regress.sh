#!/bin/bash

EXIT_CODE=0
CONFIG_FILE=/home/gpadmin/s3.conf
[ -n "`command -v gpcheckcloud`" ] && GPCHECKCLOUD=gpcheckcloud || GPCHECKCLOUD=../bin/gpcheckcloud/gpcheckcloud
RANDOM_PREFIX=gpcheckcloud-$(date +%Y%m%d)-$(cat /dev/urandom | env LC_CTYPE=C tr -dc 'a-zA-Z0-9' | fold -w 8 | head -n 1)

./generate_config_file.sh $CONFIG_FILE

echo "Preparing data to upload..."
dd if=/dev/urandom of=/tmp/gpcheckcloud.small bs=6 count=1 &> /dev/null
dd if=/dev/urandom of=/tmp/gpcheckcloud.large bs=78MB count=1 &> /dev/null
echo -ne "\n" >> /tmp/gpcheckcloud.small
echo -ne "\n" >> /tmp/gpcheckcloud.large

MD5SUM_SMALL=`cat /tmp/gpcheckcloud.small |openssl md5 |cut -d ' ' -f 2`
MD5SUM_LARGE=`cat /tmp/gpcheckcloud.large |openssl md5 |cut -d ' ' -f 2`

startTimer() {
	startTime=`date +%s`
}

printTimer() {
	currentTime=`date +%s`
	echo \($(($currentTime - $startTime))sec\)
	startTime=$currentTime
}

startTimer
echo "Uploading data..."
$GPCHECKCLOUD -u /tmp/gpcheckcloud.small "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/small/ config=$CONFIG_FILE" \
	&& echo upload small file ... ok `printTimer` || { EXIT_CODE=1; echo upload small file ... failed `printTimer`; }
$GPCHECKCLOUD -u /tmp/gpcheckcloud.large "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/large/ config=$CONFIG_FILE" \
	&& echo upload large file ... ok `printTimer` || { EXIT_CODE=1; echo upload large file ... failed `printTimer`; }

echo "Downloading and checking hashsum..."
CHECK_CASES=(
# gpcheckcloud
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/small/ $MD5SUM_SMALL"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/s3write/$RANDOM_PREFIX/large/ $MD5SUM_LARGE"
# normal files
"http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/data0014 68c4a63b721e7af0ae945ce109ca87ad"
"https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/data0014 68c4a63b721e7af0ae945ce109ca87ad"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gzipped/data0001.gz b958fb80b98605a6095e6ebc4b9b4786"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/threebytes/ fe7d81814e02eb1296757e75bb3c6be9"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/small17/ 138fc555074671912125ba692c678246"
"s3://s3-us-east-1.amazonaws.com/us-east-1.s3test.pivotal.io/regress/small17/ 138fc555074671912125ba692c678246"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gzipped/ 7b2260e9a3a3f26e84aa28dc2124f68f"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gzipped_normal1/ eacb7b210d3f7703ee06d16f520b103e"
"s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/regress/gpcheckcloud_newline/ d9331ecb3dc71b9bfca469654d26ec5f"
)

startTimer
for ((i=0; i<${#CHECK_CASES[@]}; i++))
do
	PREFIX=`echo ${CHECK_CASES[$i]} |cut -d' ' -f 1`
	DIGEST=`echo ${CHECK_CASES[$i]} |cut -d' ' -f 2`
	[ `$GPCHECKCLOUD -d "$PREFIX config=$CONFIG_FILE" 2>/dev/null |openssl md5 |cut -d ' ' -f 2` = "$DIGEST" ] \
		&& echo test $PREFIX ... ok `printTimer` || { EXIT_CODE=1; echo test $PREFIX ... failed `printTimer`; }
done

echo "Testing expected failures..."
FAILED_CASES=(
"https://s3-us-external-1.amazonaws.com/us-east-1.s3test.pivotal.io/"
)

startTimer
for ((i=0; i<${#FAILED_CASES[@]}; i++))
do
	$GPCHECKCLOUD -d "${FAILED_CASES[$i]} config=$CONFIG_FILE" 2>/dev/null
	[ "$?" -lt "128" ] && echo test ${FAILED_CASES[$i]} ... ok `printTimer` || { EXIT_CODE=1; echo test ${FAILED_CASES[$i]} ... failed `printTimer`; }
done

exit $EXIT_CODE
