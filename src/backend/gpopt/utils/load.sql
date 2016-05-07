drop schema if exists gpoptutils cascade;

create schema gpoptutils;

create function gpoptutils.DumpPlan(text) returns bytea as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpPlan'
language c strict;

create function gpoptutils.RestorePlan(bytea) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'RestorePlan'
language c strict;

create function gpoptutils.DumpPlanToFile(text, text) returns int as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpPlanToFile'
language c strict;

create function gpoptutils.RestorePlanFromFile(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'RestorePlanFromFile'
language c strict;

create function gpoptutils.RestorePlanDXL(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'RestorePlanDXL' language c strict;

create function gpoptutils.RestorePlanFromDXLFile(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'RestorePlanFromDXLFile' language c strict;

create function gpoptutils.DumpQuery(text) returns bytea as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpQuery'
language c strict;

create function gpoptutils.DumpQueryToFile(text, text) returns int as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpQueryToFile'
language c strict;

create function gpoptutils.RestoreQueryFromFile(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'RestoreQueryFromFile'
language c strict;

create function gpoptutils.DumpQueryDXL(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpQueryDXL'
language c strict;

create function gpoptutils.DumpQueryToDXLFile(text, text) returns int as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpQueryToDXLFile'
language c strict;

create function gpoptutils.RestoreQueryDXL(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'RestoreQueryDXL' language c strict;

create function gpoptutils.RestoreQueryFromDXLFile(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'RestoreQueryFromDXLFile' language c strict;

create function gpoptutils.Optimize(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'Optimize'
language c strict;

create function gpoptutils.DumpMDObjDXL(Oid) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpMDObjDXL'
language c strict;

create function gpoptutils.DumpCatalogDXL(text) returns int as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpCatalogDXL'
language c strict;

create function gpoptutils.DumpRelStatsDXL(Oid) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DumpRelStatsDXL'
language c strict;


CREATE OR REPLACE FUNCTION gp_partition_propagation(int, oid)
RETURNS void
AS '$libdir/gp_partition_functions.so', 'gp_partition_propagation_wrapper' LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION gp_partition_selection(oid, anyelement)
RETURNS oid
AS '$libdir/gp_partition_functions.so', 'gp_partition_selection_wrapper' LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION gp_partition_expansion(oid)
RETURNS setof oid
AS '$libdir/gp_partition_functions.so', 'gp_partition_expansion_wrapper' LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION gp_partition_inverse(oid)
RETURNS setof record
AS '$libdir/gp_partition_functions.so', 'gp_partition_inverse_wrapper' LANGUAGE C STRICT;

create or replace function gpoptutils.DisableXform(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'DisableXform'
language c strict;

create or replace function gpoptutils.EnableXform(text) returns text as '/Users/solimm1/greenplum-db-devel/lib/libgpoptudf.dylib', 'EnableXform'
language c strict;


