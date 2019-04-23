#! /bin/bash

set -o pipefail
set -o errexit
set -o nounset
if [ $# -lt 1 ]; then
    echo "Usage: $0 binary [binary ...]"
    exit 1
fi

OLD_DIR=$(pwd)/compiled_bits_ubuntu16_tag
NEW_DIR=$(pwd)/compiled_bits_ubuntu16_head

pushd ${OLD_DIR}
    tar xzf *.tar.gz
    old=`expr "$(source ./bin/lib/gp_bash_version.sh; print_version)" : ".*\([0-9]\+\.[0-9]\+\.[0-9]\+\)"`
popd

pushd ${NEW_DIR}
    tar xzf *.tar.gz
    new=HEAD
popd

# Install our ABI compliance checker. TODO: move to Concourse inputs.
apt-get update && apt-get install -y libelf-dev elfutils html2text
git clone https://github.com/lvc/vtable-dumper
make -C vtable-dumper && make -C vtable-dumper install prefix=/usr/local

git clone https://github.com/lvc/abi-dumper
make -C abi-dumper install prefix=/usr/local

git clone https://github.com/lvc/abi-compliance-checker
make -C abi-compliance-checker install prefix=/usr/local

# Check compliance.
pushd abi
    echo "Comparing ABI between $old and $new..."

    failed_binaries=()
    for binary in "$@"; do
        echo "Checking $binary ABI..."
        binary_name=$(basename "$binary")

        abi-dumper -lver "$old" -o "$binary_name.$old.dump" "${OLD_DIR}/$binary"
        abi-dumper -lver "$new" -o "$binary_name.$new.dump" "${NEW_DIR}/$binary"
        if ! abi-compliance-checker -l "$binary_name" -old "$binary_name.$old.dump" -new "$binary_name.$new.dump"; then
            failed_binaries+=("$binary")
        fi
    done

    # Convert each output file to raw text
    find . -name compat_report.html -exec sh -c 'html2text {} > `dirname {}`/compat_report.txt' \;
popd

if [ -n "$failed_binaries" ]; then
    echo
    echo "The following binaries have ABI differences from $old:"
    echo
    echo "    ${failed_binaries[*]}"
    echo
    echo "A report has been generated, and is displayed below:"
    for failed_binary in ${failed_binaries[@]}; do
        # directory structure is /[path to reports]/[binary name]/[old version]_to_[new version]
        report_path="./abi/compat_reports/$(basename $failed_binary)/${old}_to_${new}/compat_report.txt"
        cat $report_path
    done
fi
