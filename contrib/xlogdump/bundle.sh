#!/bin/sh

VERSION=$1

rm -rf xlogdump-${VERSION}

cp -r xlogdump xlogdump-${VERSION}

rm -rf xlogdump-${VERSION}/*
cd xlogdump-${VERSION}; git checkout .; cd ..

rm -rf xlogdump-${VERSION}/.git
rm -rf xlogdump-${VERSION}/bundle.sh
rm -rf xlogdump-${VERSION}/*~

tar zcvf xlogdump-${VERSION}.tar.gz xlogdump-${VERSION} \
  --exclude xlogdump-${VERSION}/test

tar zcvf xlogdump-tests-${VERSION}.tar.gz xlogdump-${VERSION}/test
