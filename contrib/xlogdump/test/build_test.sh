#!/bin/sh

VERSIONS="92 91 90 84 83"

cd ..
for v in $VERSIONS
  do rm -rf xlogdump-${v};

     if [ -d /usr/local/pgsql${v} ]; then
	 export USE_PGXS=1
	 export PATH=/usr/local/pgsql${v}/bin:$PATH
	 make clean;
	 make;

	 if [ -f xlogdump ]; then
	     success="$success $v"
	 else
	     failed="$failed $v"
	 fi;

	 mv xlogdump xlogdump-${v};
     fi;
done;

echo "-------------------------------------------"
echo "Success:$success";
echo "Failed:$failed";
