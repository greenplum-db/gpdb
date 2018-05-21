/*
 * Mock implementation for pxffilters.h
 *
 * Used only in pxffilters_test.c
 */


void
getTypeOutputInfo(Oid type, Oid *typOutput, bool *typIsVarlena)
{
    check_expected(type);
    check_expected(typOutput);
    check_expected(typIsVarlena);
    mock();
}


char *
OidOutputFunctionCall(Oid functionId, Datum val)
{
    check_expected(functionId);
    check_expected(val);
    return ((char *)mock());
}


struct varlena *
pg_detoast_datum(struct varlena * datum)
{
    check_expected(datum);
    return ((struct varlena *) mock());
}


void
deconstruct_array(ArrayType *array,
				  Oid elmtype,
				  int elmlen, bool elmbyval, char elmalign,
				  Datum **elemsp, bool **nullsp, int *nelemsp)
{
    check_expected(array);
    check_expected(elmtype);
    check_expected(elmlen);
    check_expected(elmbyval);
    check_expected(elmalign);
    check_expected(elemsp);
    check_expected(nullsp);
    check_expected(nelemsp);
    mock();
}


Datum
DirectFunctionCall1(PGFunction func, Datum arg1)
{
    check_expected(func);
    check_expected(arg1);
    return ((Datum) mock());
}

