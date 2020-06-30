#! /bin/bash
#
# Given a bucket containing coverage.py data, this script retrieves and combines
# that coverage data, generates an HTML report, and pushes the results back to
# the bucket. A textual report is also printed for convenience when looking at
# the CI.
#
# This script assumes that the provided bucket has a folder inside it with a
# name corresponding to the current gpdb_src commit SHA.
#
set -ex

if [ $# -ne 2 ]; then
    echo "Usage: $0 COVERAGE_BUCKET_URI COVERAGE_BUCKET_PATH"
    exit 1
fi

# Trim any trailing slash from the bucket uri.
BUCKET_URI="${1%/}"
BUCKET_PATH=$2
CWD=$(pwd)
read -r COMMIT_SHA < gpdb_src/.git/HEAD

# Coverage.py needs to be able to find the source files that were used during
# the coverage run. The easiest way to do that for the majority of those files
# is to install GPDB in the same place it was installed on the clusters.
source ./gpdb_src/concourse/scripts/common.bash
time install_gpdb

# Set PIP Download cache directory
export PIP_CACHE_DIR=${PWD}/pip-cache-dir

pip --retries 10 install -r ./gpdb_src/gpMgmt/requirements-dev.txt

# Save the JSON_KEY to a file, for later use by gsutil.
keyfile=secret-key.json
saved_umask=$(umask)
umask 077
cat - <<< "$JSON_KEY" > "$keyfile"
umask "${saved_umask}"

# Generate a Boto configuration file for gsutil.
cat - > boto.cfg <<EOF
[Boto]
https_validate_certificates = True
[Credentials]
gs_service_key_file = $(pwd)/$keyfile
EOF
export BOTO_CONFIG=$(pwd)/boto.cfg

# Pull down the coverage data for our current commit.
mkdir ./coverage
gsutil -m rsync -r "$BUCKET_URI/$COMMIT_SHA" ./coverage

cd ./coverage/

# Extract the raw coverage files
find . -name '*.tar' -exec tar -xf {} \;

# Installing GPDB gets most of the source we need, but Python sources that were
# inside the Git repo when they executed will be in a different location on this
# machine compared to the test clusters. Here, we use [paths] to tell
# coverage.py that any source files under /home/*/gpdb_src (for CCP clusters) or
# /tmp/build/*/gpdb_src (for demo clusters) can be found in our local copy of
# gpdb_src.
cat > .coveragerc <<EOF
[paths]
install =
    /usr/local/greenplum-db-devel/bin
    /tmp/build/*/gpdb_src/gpMgmt/bin

install_lib =
    /usr/local/greenplum-db-devel/lib/python/gppylib
    /tmp/build/*/gpdb_src/gpMgmt/bin/gppylib

source =
    $CWD/gpdb_src
    /home/*/gpdb_src
    /tmp/build/*/gpdb_src

[report]
omit =
    */site-packages/*
    */bin/behave
    */python/psutil/*
    */python/pygresql/*
    */python/subprocess32.py
    */python/yaml/*
    */gppkg_migrate/*
    */bin/pythonSrc/ext/*
EOF

# Now combine the individual coverage data files for analysis. There can be
# thousands of coverage files across an entire CI run, so we use a find | xargs
# pipeline to avoid execution limits.
find . -name '*.coverage.*' -print0 | xargs -0 coverage combine --append

# Generate an HTML report and sync it back to the bucket, then print out a quick
# text report for developers perusing the CI directly. The artifacts we push are
# publicly readable, to make it easy to browse the HTML. They're also gzipped to
# save on storage (see the -Z option for `gsutil cp`).
coverage html -d ./html
gsutil -m cp -rZ -a public-read ./html/* "$BUCKET_URI/$COMMIT_SHA/html"
coverage report
echo "View the full coverage report: https://storage.googleapis.com/$BUCKET_PATH/$COMMIT_SHA/html/index.html"
