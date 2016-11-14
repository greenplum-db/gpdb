drop function gpoptutils.DumpPlanToFile(text, text);
drop function gpoptutils.RestorePlanFromFile(text);
--drop function gpoptutils.DumpPlanDXL(text);
--drop function gpoptutils.DumpPlanToDXLFile(text, text);
drop function gpoptutils.RestorePlanDXL(text);
drop function gpoptutils.RestorePlanFromDXLFile(text);
drop function gpoptutils.DumpQuery(text);
drop function gpoptutils.DumpQueryToFile(text, text);
drop function gpoptutils.RestoreQueryFromFile(text);
drop function gpoptutils.DumpQueryDXL(text);
drop function gpoptutils.DumpQueryToDXLFile(text, text);
drop function gpoptutils.DumpQueryFromFileToDXLFile(text, text);
--drop function gpoptutils.RestoreQueryDXL(text);
--drop function gpoptutils.RestoreQueryFromDXLFile(text);
drop function gpoptutils.Optimize(text);
drop function gpoptutils.DumpMDObjDXL(Oid);
drop function gpoptutils.DumpCatalogDXL(text);
drop function gpoptutils.DumpRelStatsDXL(Oid);
drop function gpoptutils.DumpMDCastDXL(Oid, Oid);
drop function gpoptutils.DumpMDScCmpDXL(Oid, Oid, text);

--drop function gpoptutils.EvalExprFromDXLFile(text) returns text as 'MODULE_PATHNAME', 'EvalExprFromDXLFile';
--drop function gpoptutils.OptimizeMinidumpFromFile(text) returns text as 'MODULE_PATHNAME', 'OptimizeMinidumpFromFile';
--drop function gpoptutils.ExecuteMinidumpFromFile(text) returns text as 'MODULE_PATHNAME', 'ExecuteMinidumpFromFile';

drop schema gpoptutils;

