#!/usr/bin/env bash

setup_source_directory() {
	rm -rf "$SOURCE_DIRECTORY"

	mkdir "$SOURCE_DIRECTORY"

	git clone --single-branch --branch 5X_STABLE https://github.com/greenplum-db/gpdb.git "$SOURCE_DIRECTORY"

	cd "$SOURCE_DIRECTORY"

	git submodule update --init
}

configure() {
	cd "$SOURCE_DIRECTORY"

	./configure --prefix=$INSTALLATION_DIRECTORY --disable-orca
}

install() {
	make -j 8 && make install
}

main() {
	SOURCE_DIRECTORY="$PWD/gpdb5-source"
	INSTALLATION_DIRECTORY="$SOURCE_DIRECTORY/gpdb5-installation"

	setup_source_directory
	configure
	install
}

main "$@"
