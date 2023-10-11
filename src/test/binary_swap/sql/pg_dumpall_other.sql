-- database binswap_connect will be re-created after swapping, ignore it
\! pg_dumpall -f dump_other.sql --exclude-database=binswap_connect
\! echo $?
