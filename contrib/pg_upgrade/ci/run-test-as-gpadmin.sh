#!/usr/bin/env bash

main() {
	ssh-keygen -f ~/.ssh/id_rsa -N ''
	cp ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys
	ssh-keyscan -H localhost >>~/.ssh/known_hosts

	pushd gpdb

	./configure --disable-orca --prefix=$PWD/gpAux/greenplum-installation --without-zstd &&
		make -j 4 -l 4 &&
		make install

	source gpAux/greenplum-installation/greenplum_path.sh

	popd

	pushd gpdb/contrib/pg_upgrade/test/integration

	./scripts/test.bash
}

main "$@"
