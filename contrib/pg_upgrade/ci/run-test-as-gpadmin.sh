#!/usr/bin/env bash


install_gpdb5() {
	local gpdb5_installation_path=$1
	local gpdb5_source_path=$2

	pushd "$gpdb5_source_path"

	./configure --disable-orca --prefix="$gpdb5_installation_path" --with-python &&
		make -j 4 -l 4 &&
		make install

	source "$gpdb5_source_path/gpAux/greenplum-installation/greenplum_path.sh"

	popd
}

install_gpdb6() {
	local gpdb6_installation_path=$1
	local gpdb6_source_path=$2

	pushd "$gpdb6_source_path"

	./configure --disable-orca --prefix="$gpdb6_installation_path" --without-zstd --with-python &&
		make -j 4 -l 4 &&
		make install

	source "$gpdb6_source_path/gpAux/greenplum-installation/greenplum_path.sh"

	popd
}

main() {
	local gpdb5_installation_path=$PWD/gpdb5/gpAux/greenplum-installation
	local gpdb5_source_path=$PWD/gpdb5/

	local gpdb6_installation_path=$PWD/gpdb6/gpAux/greenplum-installation
	local gpdb6_source_path=$PWD/gpdb6/

	ssh-keygen -f ~/.ssh/id_rsa -N ''
	cp ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys
	ssh-keyscan -H localhost >>~/.ssh/known_hosts

	install_gpdb5 "$gpdb5_installation_path" "$gpdb5_source_path"
	install_gpdb6 "$gpdb6_installation_path" "$gpdb6_source_path"

	pushd "$gpdb6_source_path/contrib/pg_upgrade/test/integration"

	./scripts/test.bash "$gpdb5_installation_path" \
		"$gpdb5_source_path" \
		"$gpdb6_installation_path" \
		"$gpdb6_source_path"
}

main "$@"
