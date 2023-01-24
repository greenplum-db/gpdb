#!/bin/bash

# Grep all diff line numbers by searching for patterns like
# "+33586874c33586874" OR # "+33587876,33587879c33587876,33587879",
# then search backward from this line to find out what object's ddl
# this line belongs to, and append corresponding DROP statement for
# this object to the output sql file.

if [[ ( $@ == "--help") ||  $@ == "-h" ]]
then
        echo "Usage: $0 INPUT_DIFF_FILE OUTPUT_SQL_FILE"
        echo "Example: $0 my_dump.diff my_drop.sql"
        echo "To generate a INPUT_DIFF_FILE, run:"
        echo "diff dump_other.sql dump_current.sql > my_dump.diff"
        exit 0
fi

INPUT_DIFF_FILE=$1
OUTPUT_SQL_FILE=$2
DUMP_CURRENT_FILE=dump_current.sql

if [ ! -f ${DUMP_CURRENT_FILE} ]; then
	echo "${DUMP_CURRENT_FILE} file not exists. Bailing out."
	echo "HINT: most likely you need to run this script in the concourse container that failed the binary swap test."
	exit 1
fi

echo "-- Drop objects that introduce dump diff" > ${OUTPUT_SQL_FILE}

previous_db=""
previous_obj=""
while read linenumber ; do
        echo "processing diff at linenumber:$linenumber";

        # Search backwards from this linenumber and find the first "CREATE ..."
        obj=$(head -n${linenumber} ${DUMP_CURRENT_FILE} | tac | grep -m1 CREATE)
        if [[ "${obj}" != "${previous_obj}" ]]
        then
                db=$(head -n${linenumber} ${DUMP_CURRENT_FILE} | tac | grep -m1 \connect)
                echo "database: ${db}"
                echo "object: ${obj}"

                if [[ "${db}" != "${previous_db}" ]]
                then
                        echo "${db}" >> ${OUTPUT_SQL_FILE}
                fi

                # Append DROP statement to output sql file
                # e.g. modify
                # "CREATE TABLE foo (" or
                # "CREATE TABLE foo AS" to
                # "DROP TABLE IF EXISTS foo;"
                drop_stmt=$(echo "${obj}" | sed -r 's/CREATE (.*) ([^[:space:]]+) (\(|AS)/DROP \1 IF EXISTS \2;/')
                echo "${drop_stmt}" >> ${OUTPUT_SQL_FILE}
        fi

        previous_db="${db}"
        previous_obj="${obj}"
done < <(grep -o '^[1-9][0-9]\+[,|c]' ${INPUT_DIFF_FILE} | sed -r 's/[,c]//')

