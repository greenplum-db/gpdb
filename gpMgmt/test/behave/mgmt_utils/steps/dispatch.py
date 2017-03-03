from gppylib.db import dbconn

@when('the user runs a twelve slice query on "{dbname}"')
def impl(context, dbname):

    SLICE_QUERY = """
select count(*) from (select * from
(select * from 
(select * from (select a from t2 group by a) SUBA, (select a from t1 group by a) SUBB) PARTA,
(select * from (select b from t2 group by b) SUBC, (select b from t1 group by b) SUBD) PARTB) SOUPA,
(select * from 
(select * from (select a from t2 group by a) SUBA, (select a from t1 group by a) SUBB) PARTA,
(select * from (select b from t2 group by b) SUBC, (select b from t1 group by b) SUBD) PARTB) SOUPB) FINALA;
    """

    try:
        with dbconn.connect(dbconn.DbURL(dbname=dbname)) as conn:
            context.slice_query_result = dbconn.execSQLForSingleton(conn, SLICE_QUERY)
    except Exception, e:
        context.dispatch_exception = e
    else:
        context.dispatch_exception = None

@then('there should not be a dispatch exception')
def impl(context):
    if context.dispatch_exception:
        raise context.dispatch_exception

@then('the dispatch exception should contain the string "{errstring}"')
def impl(context, errstring):
    if errstring not in context.dispatch_exception.__str__():
        raise Exception("Expected error string '%s' and got '%s'" % (errstring, context.dispatch_exception.__str__()))
 
