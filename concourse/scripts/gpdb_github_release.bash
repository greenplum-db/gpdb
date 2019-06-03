#!/bin/bash

set -euo pipefail

BASE_DIR=$(pwd)

# Format: 6.0.0-beta.3#2019-05-08T15:35:55.252Z
GPDB_BUILD_TAG=$(cat pivotal_gpdb/version)

# Format: 6.0.0-beta.3
GPDB_RELEASE_TAG=${GPDB_BUILD_TAG%%#*}

git config --global user.email "gp-releng@pivotal.io"
git config --global user.name "gp-releng-bot"

mkdir -p ~/.ssh/
echo "${GPDB_SRC_DEPLOY_KEY}" > ~/.ssh/id_rsa
chmod 600 ~/.ssh/id_rsa
ssh-keyscan github.com >> ~/.ssh/known_hosts

function get_release_commit_sha() {
    local __release_tag
    __release_tag=$1

    git --git-dir "${BASE_DIR}/gpdb_staging/.git" log -1 --pretty=format:"%H" "${__release_tag}"
}

function tag_gpdb_src() {
    local __commit_sha
    local __gpdb_tag

    __gpdb_tag=$1
    __commit_sha=$2

    # github-release concourse resource type: only lightweight tags are supported
    # ref: https://github.com/concourse/github-release-resource/blob/0447360bed360a53c11b09c912decc3b2fa5abd2/in_command.go#L209
    git --git-dir "${BASE_DIR}/gpdb_src/.git" tag "${__gpdb_tag}" "${__commit_sha}"
}

function push_gpdb_src_tags_to_remote() {
    git --git-dir "${BASE_DIR}/gpdb_src/.git" push --tags
}

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
    local __gpdb_release_commit_sha

    echo "Current Released Tag: ${GPDB_RELEASE_TAG}"

    __gpdb_release_commit_sha=$(get_release_commit_sha "${GPDB_RELEASE_TAG}")
    echo "The commit SHA of current release: ${__gpdb_release_commit_sha}"

    tag_gpdb_src "${GPDB_RELEASE_TAG}" "${__gpdb_release_commit_sha}"
    echo "Created tag: ${GPDB_RELEASE_TAG} on gpdb_src successfully!"

    build_gpdb_binaries_tarball
    echo "Created the release binaries successfully! [tar.gz, zip]"

    push_gpdb_src_tags_to_remote
    echo "Pushed all tags of gpdb repo successfully!"
}

_main
