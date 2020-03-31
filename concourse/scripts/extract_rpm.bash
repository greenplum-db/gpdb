#!/usr/bin/env bash

extract_rpm_to_tar () {
    rpm2cpio gpdb_rpm/greenplum-db-*.rpm | cpio -idvm

    local tarball="${PWD}/gpdb_artifacts/bin_gpdb.tar.gz"
    pushd usr/local/greenplum-db-*
        tar czf "${tarball}" ./*
    popd
}

extract_rpm_to_tar "${@}"