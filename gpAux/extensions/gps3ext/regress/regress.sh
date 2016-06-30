#!/bin/bash

#> ~/regress.log && for i in `ls regress/regress_*.sql`; do echo XXXXXXXX START TESTING $i XXXXXXXX|tee -a ~/regress.log;psql -f $i 2>&1 |tee -a ~/regress.log; done

GPCHECKCLOUD=./bin/gpcheckcloud/gpcheckcloud

[ `$GPCHECKCLOUD -d "http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0014 config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "68c4a63b721e7af0ae945ce109ca87ad" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "http://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0016 config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "0fd502a303eb8f138f5916ec357721b1" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0014 config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "68c4a63b721e7af0ae945ce109ca87ad" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/data0016 config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "0fd502a303eb8f138f5916ec357721b1" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/gzipped/data0001.gz config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "b958fb80b98605a6095e6ebc4b9b4786" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/threebytes/ config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "fe7d81814e02eb1296757e75bb3c6be9" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/small17/ config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "138fc555074671912125ba692c678246" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "s3://s3-us-east-1.amazonaws.com/useast1.s3test.pivotal.io/small17/ config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "138fc555074671912125ba692c678246" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/gzipped/ config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "7b2260e9a3a3f26e84aa28dc2124f68f" ] && echo OK. || echo Failed.

[ `$GPCHECKCLOUD -d "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset1/gzipped_normal1/ config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "eacb7b210d3f7703ee06d16f520b103e" ] && echo OK. || echo Failed.

# HUGE
[ `$GPCHECKCLOUD -d "https://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/hugefile/airlinedata1.csv config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "f5811ad92c994f1d6913d5338575fe38" ] && echo OK. || echo Failed.

# HUGE
[ `$GPCHECKCLOUD -d "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/hugefile/ config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "75baaa39f2b1544ed8af437c2cad86b7" ] && echo OK. || echo Failed.

# HUGE
[ `$GPCHECKCLOUD -d "s3://s3-us-west-2.amazonaws.com/s3test.pivotal.io/dataset2/gzipped_normal2/ config=/home/gpadmin/s3.conf" 2>/dev/null |md5sum |cut -d ' ' -f 1` = "a930794bc885bccf6eed45bd40367a7d" ] && echo OK. || echo Failed.
