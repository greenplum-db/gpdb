#!/bin/bash

set -euo pipefail

BASE_DIR="$(pwd)"

GPDB_RELEASE_COMMIT_SHA=$(git --git-dir gpdb_src/.git rev-list --tags --max-count=1)
GPDB_RELEASE_TAG="$(git --git-dir gpdb_src/.git describe --tags ${GPDB_RELEASE_COMMIT_SHA})"

function build_gpdb_binaries_tarball(){
    pushd "${BASE_DIR}/gpdb_src"
        git --no-pager show --summary refs/tags/"${GPDB_RELEASE_TAG}"

        # TODO: adding the gpdb_bin_${PLATFORM} tar.gz here
        # The tar.gz and .zip files are the github release attachements.
        # here, I just want to test the uploading attachments function for github release concourse resource.
        # later, we can upload the binaries of gpdb instead of them.
        # eg. bin_gpdb_centos6.tar.gz, bin_gpdb_unbuntu18.04.tar.gz ...

        git archive -o "${BASE_DIR}/release_artifacts/${GPDB_RELEASE_TAG}.tar.gz" --prefix="gpdb-${GPDB_RELEASE_TAG}/"  --format=tar.gz  refs/tags/"${GPDB_RELEASE_TAG}"
        git archive -o "${BASE_DIR}/release_artifacts/${GPDB_RELEASE_TAG}.zip" --prefix="gpdb-${GPDB_RELEASE_TAG}/" --format=zip  -9 refs/tags/"${GPDB_RELEASE_TAG}"
    popd

    # Prepare for the gpdb github release
    echo "${GPDB_RELEASE_TAG}" > "release_artifacts/name"
    echo "${GPDB_RELEASE_TAG}" > "release_artifacts/tag"
    echo "Greenplum-db version: ${GPDB_RELEASE_TAG}" > "release_artifacts/body"


}

function _main(){
    echo "Current Released Tag: ${GPDB_RELEASE_TAG}"

    build_gpdb_binaries_tarball
    echo "Created the release binaries successfully! [tar.gz, zip]"
}

_main
