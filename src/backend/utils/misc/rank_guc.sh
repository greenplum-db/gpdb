#!/bin/bash
TOPDIR="$(pwd)/../../../.."
SYNC_META_HEADER_PATH="$(pwd)/sync_guc_name.conf"
UNSYNC_META_HEADER_PATH="$(pwd)/unsync_guc_name.conf"
SYNC_HEADER_PATH="$TOPDIR/src/include/utils/sync_guc_name.h"
UNSYNC_HEADER_PATH="$TOPDIR/src/include/utils/unsync_guc_name.h"

rm $SYNC_HEADER_PATH
rm $UNSYNC_HEADER_PATH

if [ "$1" == "clean" ]; then
	exit 0
fi

touch $SYNC_HEADER_PATH
touch $UNSYNC_HEADER_PATH

echo "/*" >> $SYNC_HEADER_PATH
echo " * This is auto generate header file, new guc name should populate in sync_guc_name.conf file" >> $SYNC_HEADER_PATH
echo " */" >> $SYNC_HEADER_PATH

echo "/* " >> $UNSYNC_HEADER_PATH
echo " * This is auto generate header file, new guc name should populate in unsync_guc_name.conf file" >> $UNSYNC_HEADER_PATH
echo " */" >> $UNSYNC_HEADER_PATH

export LC_ALL=C
awk '{ print tolower($1) }' $SYNC_META_HEADER_PATH | sed 's/#.*$//g' | sed '/^$/d' | sort >> $SYNC_HEADER_PATH
awk '{ print tolower($1) }' $UNSYNC_META_HEADER_PATH | sed 's/#.*$//g' | sed '/^$/d' | sort >> $UNSYNC_HEADER_PATH
