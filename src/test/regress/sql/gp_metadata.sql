select version() ~ '^PostgreSQL ([0-9]+\.){2}([0-9]+)? \(Greenplum Database ([0-9]+\.){2}[0-9]+.+' as version;
select gp_opt_version() ~ '^GPOPT version: ([0-9]+\.){2}[0-9]+, Xerces version: ([0-9]+\.){2}[0-9]+$' as version;
