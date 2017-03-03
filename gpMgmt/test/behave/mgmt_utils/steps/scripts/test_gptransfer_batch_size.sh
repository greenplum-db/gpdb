#!/bin/bash

OLDROWS=`psql -c 'select * from pg_stat_activity' | egrep "\([0-9]+ rows?\)" | egrep -o "[0-9]+"`
OUTPUT=$1

echo "$OLDROWS" > "$OUTPUT"

while :
do
    ROWS=`psql -c 'select * from pg_stat_activity' | egrep "\([0-9]+ rows?\)" | egrep -o "[0-9]+"`
    if [[ $ROWS > $OLDROWS ]]
    then
        echo "$ROWS" > "$OUTPUT"
    fi
    OLDROWS=$ROWS
    sleep 1
done
