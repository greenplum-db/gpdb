#!/usr/bin/env bash
# issue coverage command by iterate though all source directories, skipping some

pushd_quiet () {
    command pushd "$@" > /dev/null
}

popd_quiet () {
    command popd "$@" > /dev/null
}

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PARENT_DIR=${SCRIPT_DIR}/..
SKIP_DIRS_FILE=`mktemp`
CHILD_DIRS_FILE=`mktemp`
COVERAGE_DIRS_FILE=`mktemp`

#echo "skip file: $SKIP_DIRS_FILE"
#echo "child file: $CHILD_DIRS_FILE"

# some directories we wish to skip for coverage
SKIP_DIRS="integrations scripts testUtils"
SKIP_DIR_ARRAY=(${SKIP_DIRS})
IFS=$'\n' sorted=($(sort <<<"${SKIP_DIR_ARRAY[*]}"))
unset IFS

printf "%s\n" "${sorted[@]}" > ${SKIP_DIRS_FILE}

pushd_quiet ${PARENT_DIR}
child_dirs=`find * -type d  -mindepth 0 -maxdepth 0`
child_dirs_array=(${child_dirs})
printf "%s\n" "${child_dirs_array[@]}" > ${CHILD_DIRS_FILE}

comm -23 ${CHILD_DIRS_FILE} ${SKIP_DIRS_FILE} > ${COVERAGE_DIRS_FILE}

cat $COVERAGE_DIRS_FILE | while read child
do
    echo "./$child/"
    pushd_quiet "./$child/"
        go test -cover
    popd_quiet
done


#echo ""
#echo skip:
#cat $SKIP_DIRS_FILE
#
#echo ""
#echo child:
#cat $CHILD_DIRS_FILE

rm $SKIP_DIRS_FILE
rm $CHILD_DIRS_FILE
rm $COVERAGE_DIRS_FILE
