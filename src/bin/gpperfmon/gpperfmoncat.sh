#!/bin/bash

iconv_encodings=(
    ""  # SQL_ASCII not supported as server encoding.
    "EUC-JP"
    "EUC-CN"
    "EUC-KR"
    "EUC-TW"
    "EUC-JISX0213"
    "UTF8"
    ""  # MULE_INTERNAL not supported in iconv.
    "LATIN1"
    "LATIN2"
    "LATIN3"
    "LATIN4"
    "LATIN5"
    "LATIN6"
    "LATIN7"
    "LATIN8"
    "LATIN9"
    "LATIN10"
    "WINDOWS-1256"
    "WINDOWS-1258"
    ""  # WIN866 not supported in iconv.
    "WINDOWS-874"
    "KOI8-R"
    "WINDOWS-1251"
    "WINDOWS-1252"
    "ISO_8859-5"
    "ISO_8859-6"
    "ISO_8859-7"
    "ISO_8859-8"
    "WINDOWS-1250"
    "WINDOWS-1253"
    "WINDOWS-1254"
    "WINDOWS-1255"
    "WINDOWS-1257"
    "KOI8-U"
    "SJIS"
    ""  # BIG5 not supported in server encoding.
    ""  # GBK not supported in server encoding.
    ""  # UHC not supported in server encoding.
    ""  # GB18030 not supported in server encoding.
    "JOHAB"
    "" # SJIS not supported in server encoding.
)
server_encoding=`(psql -p $GP_MASTER_PORT -d postgres -c "select encoding from
pg_catalog.pg_database d where d.datname = 'gpperfmon'" | tail -n3 | head -n1) 2> /dev/null || true`
iconv_encoding=${iconv_encodings[${server_encoding}]}
iconv -f $iconv_encoding -t $iconv_encoding -c $*
