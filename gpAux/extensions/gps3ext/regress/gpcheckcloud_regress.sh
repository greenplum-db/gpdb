#!/bin/bash

EXIT_CODE=0
GPCHECKCLOUD=./bin/gpcheckcloud/gpcheckcloud

PREFIX[0]="http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0014"
PREFIX[1]="http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0016"
PREFIX[2]="https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0014"
PREFIX[3]="https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0016"
PREFIX[4]="https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/gzipped/data0001.gz"
PREFIX[5]="s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/threebytes/"
PREFIX[6]="s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/"
PREFIX[7]="s3://s3-us-east-1.amazonaws.com/useast1.s3test.pivotal.io/small17/"
PREFIX[8]="s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/gzipped/"
PREFIX[9]="s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/gzipped_normal1/"

HASHSUM[0]="68c4a63b721e7af0ae945ce109ca87ad"
HASHSUM[1]="0fd502a303eb8f138f5916ec357721b1"
HASHSUM[2]="68c4a63b721e7af0ae945ce109ca87ad"
HASHSUM[3]="0fd502a303eb8f138f5916ec357721b1"
HASHSUM[4]="b958fb80b98605a6095e6ebc4b9b4786"
HASHSUM[5]="fe7d81814e02eb1296757e75bb3c6be9"
HASHSUM[6]="138fc555074671912125ba692c678246"
HASHSUM[7]="138fc555074671912125ba692c678246"
HASHSUM[8]="7b2260e9a3a3f26e84aa28dc2124f68f"
HASHSUM[9]="eacb7b210d3f7703ee06d16f520b103e"

#HUGE FILES
PREFIX[10]="https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/hugefile/airlinedata1.csv"
PREFIX[11]="s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/hugefile/"
PREFIX[12]="s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/gzipped_normal2/"

HASHSUM[10]="f5811ad92c994f1d6913d5338575fe38"
HASHSUM[11]="75baaa39f2b1544ed8af437c2cad86b7"
HASHSUM[12]="a930794bc885bccf6eed45bd40367a7d"

for ((i=0; i<${#PREFIX[@]}; i++))
do
	[ `$GPCHECKCLOUD -d "${PREFIX[$i]} config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "${HASHSUM[$i]}" ] \
		&& echo test ${PREFIX[$i]} ... ok \
		|| { EXIT_CODE=1; echo test ${PREFIX[$i]} ... failed; }
done

exit $EXIT_CODE
