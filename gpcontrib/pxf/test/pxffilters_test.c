/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include "cmockery.h"

#include "postgres.h"
#include "utils/memutils.h"

/* Define UNIT_TESTING so that the extension can skip declaring PG_MODULE_MAGIC */
#define UNIT_TESTING

/* include unit under test */
#include "../src/pxffilters.c"


void run__scalar_const_to_str(Const* input, StringInfo result, char* expected);
void run__scalar_const_to_str__negative(Const* input, StringInfo result, char* value);
void run__list_const_to_str(Const* input, StringInfo result, char* expected);
void run__list_const_to_str__negative(Const* input, StringInfo result, int len, Datum *dats);

/* date_eq(date, date) => bool */
#define DateEqualFuncId 1086

static void
test__supported_filter_type(void **state)
{
	Oid oids[] =
	{
		INT2OID,
		INT4OID,
		INT8OID,
		FLOAT4OID,
		FLOAT8OID,
		NUMERICOID,
		BOOLOID,
		TEXTOID,
		VARCHAROID,
		BPCHAROID,
		CHAROID,
		DATEOID,
		TIMESTAMPOID,
		CIRCLEOID /* unsupported type */
	};

	int array_size = sizeof(oids) / sizeof(Oid);
	bool result = false;
	int i;

	/* supported types */
	for (i = 0; i < array_size-1; ++i)
	{
		result = supported_filter_type(oids[i]);
		assert_true(result);
	}
	/* unsupported type */
	result = supported_filter_type(oids[i]);
	assert_false(result);

	/* go over pxf_supported_types array */
	int nargs = sizeof(pxf_supported_types) / sizeof(Oid);
	assert_int_equal(nargs, 17);
	for (i = 0; i < nargs; ++i)
	{
		assert_true(supported_filter_type(pxf_supported_types[i]));
	}

}

static void
test__supported_operator_type_op_expr(void **state)
{
	Oid operator_oids[13][2] = {
			{ Int2EqualOperator, PXFOP_EQ },
			{ 95, PXFOP_LT },
			{ 520, PXFOP_GT },
			{ 522, PXFOP_LE },
			{ 524, PXFOP_GE },
			{ 519, PXFOP_NE },
			{ Int4EqualOperator, PXFOP_EQ },
			{ 97, PXFOP_LT },
			{ 521, PXFOP_GT },
			{ 523, PXFOP_LE },
			{ 525, PXFOP_GE },
			{ 518, PXFOP_NE },
			{ InvalidOid, InvalidOid }
	};

	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));

	int array_size = sizeof(operator_oids) / sizeof(dbop_pxfop_map);
	bool result = false;
	int i;

	/* supported types */
	for (i = 0; i < array_size-1; ++i)
	{
		result = supported_operator_type_op_expr(operator_oids[i][0], filter);
		assert_true(result);
		assert_true(operator_oids[i][1] == filter->op);
	}

	/* unsupported type */
	result = supported_operator_type_op_expr(operator_oids[i][0], filter);
	assert_false(result);

	/* go over pxf_supported_opr_op_expr array */
	int nargs = sizeof(pxf_supported_opr_op_expr) / sizeof(dbop_pxfop_map);
	assert_int_equal(nargs, 97);
	for (i = 0; i < nargs; ++i)
	{
		assert_true(supported_operator_type_op_expr(pxf_supported_opr_op_expr[i].dbop, filter));
		assert_true(pxf_supported_opr_op_expr[i].pxfop == filter->op);
	}
}

/*
 * const_value must be palloc'ed, it will be freed by scalar_const_to_str
 */
static void
mock__scalar_const_to_str(Oid const_type, char* const_value)
{
	expect_value(getTypeOutputInfo, type, const_type);
	expect_any(getTypeOutputInfo, typOutput);
	expect_any(getTypeOutputInfo, typIsVarlena);
	will_return(getTypeOutputInfo, NULL);

	expect_any(OidOutputFunctionCall, functionId);
	expect_any(OidOutputFunctionCall, val);
	will_return(OidOutputFunctionCall, const_value);
}

/*
 * const_value must be palloc'ed, it will be freed by list_const_to_str
 */
static void
mock__list_const_to_str(Oid const_type, int len, Datum *dats) {

	expect_any(pg_detoast_datum, datum);
	will_return(pg_detoast_datum, NULL);

	expect_any(deconstruct_array, array);
	expect_any(deconstruct_array, elmtype);
	expect_any(deconstruct_array, elmlen);
	expect_any(deconstruct_array, elmbyval);
	expect_any(deconstruct_array, elmalign);
	expect_any(deconstruct_array, elemsp);
	expect_any(deconstruct_array, nullsp);
	expect_any(deconstruct_array, nelemsp);
	will_return(deconstruct_array, NULL);
	will_assign_value(deconstruct_array, nelemsp, len);
	will_assign_value(deconstruct_array, elemsp, dats);

	if (const_type == TEXTARRAYOID)
	{
		for (int i = 0; i < len; i++)
		{
			expect_any(DirectFunctionCall1, func);
			expect_any(DirectFunctionCall1, arg1);
			will_return(DirectFunctionCall1, dats[i]);
		}
	}
}

static void
verify__scalar_const_to_str(bool is_null, char* const_value, Oid const_type, char* expected)
{
	StringInfo result = makeStringInfo();
	char* value = NULL;
	Const* input = (Const*) palloc0(sizeof(Const));
	input->constisnull = is_null;
	input->consttype = const_type;

	/* need to prepare inner functions */
	if (!is_null)
	{
		value = strdup(const_value); /* will be free'd by scalar_const_to_str */

		mock__scalar_const_to_str(const_type, value);
	}

	/* no expected value means it's a negative test */
	if (expected)
	{
		run__scalar_const_to_str(input, result, expected);
	}
	else
	{
		run__scalar_const_to_str__negative(input, result, value);
	}

	pfree(result->data);
	pfree(result);
	pfree(input);
}

static void
verify__list_const_to_str(Oid const_type, char* expected, int len, Datum *dats)
{
	StringInfo result = makeStringInfo();
	Const* input = (Const*) palloc0(sizeof(Const));
	input->constisnull = false;
	input->consttype = const_type;

	/* need to prepare inner functions */
	mock__list_const_to_str(const_type, len, dats);

	/* no expected value means it's a negative test */
	if (expected)
	{
		run__list_const_to_str(input, result, expected);
	}
	else
	{
		run__list_const_to_str__negative(input, result, len, dats);
	}

	pfree(result->data);
	pfree(result);
	pfree(input);
}

static void
test__list_const_to_str__int(void **state) {

	Datum dats8[3] = {Int8GetDatum(1), Int8GetDatum(2), Int8GetDatum(3)};

	verify__list_const_to_str(INT2ARRAYOID, "s1d1s1d2s1d3", 3, dats8);

	Datum dats16[1] = {Int16GetDatum(42)};
	verify__list_const_to_str(INT4ARRAYOID, "s2d42", 1, dats16);

	Datum dats32[2] = {Int32GetDatum(11), Int32GetDatum(22)};
	verify__list_const_to_str(INT4ARRAYOID, "s2d11s2d22", 2, dats32);
}

void run__scalar_const_to_str(Const* input, StringInfo result, char* expected)
{
	scalar_const_to_str(input, result);
	assert_string_equal(result->data, expected);
}

void run__scalar_const_to_str__negative(Const* input, StringInfo result, char* value)
{

	StringInfo err_msg = makeStringInfo();
	appendStringInfo(err_msg,
			"internal error in pxffilters.c:scalar_const_to_str. "
			"Using unsupported data type (%d) (value %s)", input->consttype, value);

	/* Setting the test -- code omitted -- */
	MemoryContext old_context = CurrentMemoryContext;
	PG_TRY();
	{
		/* This will throw a ereport(ERROR).*/
		scalar_const_to_str(input, result);
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData *edata = CopyErrorData();

		/* Validate the type of expected error */
		assert_true(edata->sqlerrcode == ERRCODE_INTERNAL_ERROR);
		assert_true(edata->elevel == ERROR);
		assert_string_equal(edata->message, err_msg->data);

		pfree(err_msg->data);
		pfree(err_msg);

		return;
	}
	PG_END_TRY();

	assert_true(false);
}

void run__list_const_to_str(Const* input, StringInfo result, char* expected)
{
	list_const_to_str(input, result);
	assert_string_equal(result->data, expected);
}

void run__list_const_to_str__negative(Const* input, StringInfo result, int len, Datum *dats)
{

	StringInfo err_msg = makeStringInfo();
	appendStringInfo(err_msg,
			"internal error in pxffilters.c:list_const_to_str. "
			"Using unsupported data type (%d) (len %d)", input->consttype, len);

	/* Setting the test -- code omitted -- */
	MemoryContext old_context = CurrentMemoryContext;
	PG_TRY();
	{
		/* This will throw a ereport(ERROR).*/
		list_const_to_str(input, result);
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(old_context);
		ErrorData *edata = CopyErrorData();

		/* Validate the type of expected error */
		assert_true(edata->sqlerrcode == ERRCODE_INTERNAL_ERROR);
		assert_true(edata->elevel == ERROR);
		assert_string_equal(edata->message, err_msg->data);

		pfree(err_msg->data);
		pfree(err_msg);

		return;
	}
	PG_END_TRY();

	assert_true(false);
}

static void
test__scalar_const_to_str__null(void **state)
{
	verify__scalar_const_to_str(true, NULL, 1, NullConstValue);
}

static void
test__scalar_const_to_str__int(void **state)
{
	verify__scalar_const_to_str(false, "1234", INT2OID, "1234");
	verify__scalar_const_to_str(false, "1234", INT4OID, "1234");
	verify__scalar_const_to_str(false, "1234", INT8OID, "1234");
	verify__scalar_const_to_str(false, "1.234", FLOAT4OID, "1.234");
	verify__scalar_const_to_str(false, "1.234", FLOAT8OID, "1.234");
	verify__scalar_const_to_str(false, "1234", NUMERICOID, "1234");
}

static void
test__scalar_const_to_str__text(void **state)
{
	verify__scalar_const_to_str(false, "that", TEXTOID, "that");
	verify__scalar_const_to_str(false, "joke", VARCHAROID, "joke");
	verify__scalar_const_to_str(false, "isn't", BPCHAROID, "isn't");
	verify__scalar_const_to_str(false, "funny", CHAROID, "funny");
	verify__scalar_const_to_str(false, "anymore", BYTEAOID, "anymore");
	verify__scalar_const_to_str(false, "iamdate", DATEOID, "iamdate");
}

static void
test__scalar_const_to_str__NegativeCircle(void **state)
{
	verify__scalar_const_to_str(false, "<3,3,9>", CIRCLEOID, NULL);
}

static void
test__opexpr_to_pxffilter__null(void **state)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	OpExpr *expr = (OpExpr*) palloc0(sizeof(OpExpr));

	assert_false(opexpr_to_pxffilter(NULL, NULL));
	assert_false(opexpr_to_pxffilter(NULL, filter));
	assert_false(opexpr_to_pxffilter(expr, NULL));

	expr->args = NIL;
	assert_false(opexpr_to_pxffilter(expr, filter));

	pfree(filter);
	pfree(expr);
}

static void
test__opexpr_to_pxffilter__unary_expr(void **state)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	OpExpr *expr = (OpExpr*) palloc0(sizeof(OpExpr));
	Var *arg = (Var*) palloc0(sizeof(Var));
	arg->xpr.type = T_Var;

	assert_false(opexpr_to_pxffilter(NULL, NULL));
	assert_false(opexpr_to_pxffilter(NULL, filter));
	assert_false(opexpr_to_pxffilter(expr, NULL));

	expr->args = NIL;
	expr->args = lappend(expr->args, arg);
	assert_false(opexpr_to_pxffilter(expr, filter));

	pfree(arg);
	pfree(filter);
	pfree(expr);
}

static void
compare_filters(PxfFilterDesc* result, PxfFilterDesc* expected)
{
	assert_int_equal(result->l.opcode, expected->l.opcode);
	assert_int_equal(result->l.attnum, expected->l.attnum);
	if (expected->l.conststr)
		assert_string_equal(result->l.conststr->data, expected->l.conststr->data);
	else
		assert_true(result->l.conststr == NULL);

	assert_true(result->r.opcode == expected->r.opcode);
	assert_int_equal(result->r.attnum, expected->r.attnum);
	if (expected->r.conststr)
		assert_string_equal(result->r.conststr->data, expected->r.conststr->data);
	else
		assert_true(result->r.conststr == NULL);

	assert_int_equal(result->op, expected->op);
}

static PxfFilterDesc *
build_filter(char lopcode, int lattnum, char* lconststr,
							 char ropcode, int rattnum, char* rconststr,
							 PxfOperatorCode op)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));

	filter->l.opcode = lopcode;
	filter->l.attnum = lattnum;
	if (lconststr)
	{
		filter->l.conststr = makeStringInfo();
		appendStringInfoString(filter->l.conststr, lconststr);
	}

	filter->r.opcode = ropcode;
	filter->r.attnum = rattnum;
	if (rconststr)
	{
		filter->r.conststr = makeStringInfo();
		appendStringInfoString(filter->r.conststr, rconststr);
	}

	filter->op = op;

	return filter;
}

static Var *
build_var(Oid oid, int attno)
{
	Var *arg_var = (Var*) palloc0(sizeof(Var));
	arg_var->xpr.type = T_Var;
	arg_var->vartype = oid;
	arg_var->varattno = attno;
	return arg_var;
}

static Const *
build_const(Oid oid, char* value, bool expectToBeRead)
{
	Const* arg_const = (Const*) palloc0(sizeof(Const));
	arg_const->xpr.type = T_Const;
	arg_const->constisnull = (value == NULL);
	arg_const->consttype = oid;
	if (value != NULL && expectToBeRead)
	{
		mock__scalar_const_to_str(oid, value);
	}

	return arg_const;
}

static OpExpr *
build_op_expr(void* left, void* right, int op)
{
	OpExpr *expr = (OpExpr*) palloc0(sizeof(OpExpr));
	expr->args = NIL;
	expr->args = lappend(expr->args, left);
	expr->args = lappend(expr->args, right);

	expr->opno = op;
	expr->xpr.type = T_OpExpr;
	return expr;
}

static NullTest *
build_null_expr(Expr* arg, NullTestType nullType)
{
	NullTest *expr = (NullTest*) palloc0(sizeof(NullTest));
	expr->nulltesttype = nullType;
	expr->arg = arg;

	expr->xpr.type = T_NullTest;
	return expr;
}

static ExpressionItem *
build_null_expression_item(int attnum, Oid attrtype, NullTestType nullType)
{
	ExpressionItem *expressionItem = (ExpressionItem*) palloc0(sizeof(ExpressionItem));
	Var *vararg = build_var(attrtype, attnum);
	NullTest *operationExpression = build_null_expr((Expr *) vararg, nullType);

	expressionItem->node = (Node *) operationExpression;
	expressionItem->processed = false;
	expressionItem->parent = NULL;

	return expressionItem;
}

static ExpressionItem *
build_expression_item(int lattnum, Oid lattrtype, char *rconststr, Oid rattrtype, int op)
{
	ExpressionItem *expressionItem = (ExpressionItem*) palloc0(sizeof(ExpressionItem));
	Var *leftop = build_var(lattrtype, lattnum);
	Const *rightop = build_const(rattrtype, strdup(rconststr), true);
	OpExpr *operationExpression = build_op_expr(leftop, rightop, op);

	expressionItem->node = (Node *) operationExpression;
	expressionItem->processed = false;
	expressionItem->parent = NULL;

	return expressionItem;
}

static OpExpr *
build_qualifier(int lattnum, Oid lattrtype, char* rconststr, Oid rattrtype, int op)
{
	Var *leftop = build_var(lattrtype, lattnum);
	Const *rightop = build_const(rattrtype, strdup(rconststr), true);
	return build_op_expr(leftop, rightop, op);
}

static FuncExpr *
build_func_expr_operand(List *args, CoercionForm funcformat)
{
	FuncExpr* operand = palloc0(sizeof(FuncExpr));
	((Node*) operand)->type = T_FuncExpr;
	operand->args = args;
	operand->funcformat = funcformat;
	return operand;
}

/**
 * builds FuncExpr for a following expression: function(column1, column2,...,columnk)
 * columnOids - oids of column1, column2, ... types
 *
 * Typical representation of a functions we support:
 * COERCE_EXPLICIT_CAST->COERCE_EXPLICIT_CALL->COERCE_IMPLICIT_CAST->T_Var,
 * where Var holds actual arguments - column1, column2,...,columnk
 */
static FuncExpr *
build_nested_func_expr_operand(List *columnsOids, List *attrsIndices)
{
	assert_int_equal(columnsOids->length, attrsIndices->length);
	ListCell *columnOidLc = NULL, *attrIndexLc = NULL;
	Var *var = NULL;
	List* args = NIL;
	forboth(columnOidLc, columnsOids, attrIndexLc, attrsIndices) {
		var = build_var(lfirst_oid(columnOidLc), lfirst_int(attrIndexLc));
		args = lappend(args, var);
	}
	FuncExpr* operandImplicitCast = build_func_expr_operand(args, COERCE_IMPLICIT_CAST);
	FuncExpr* operandExplicitCall = build_func_expr_operand(list_make1(operandImplicitCast), COERCE_EXPLICIT_CALL);
	FuncExpr* operandExplicitCast = build_func_expr_operand(list_make1(operandExplicitCall), COERCE_EXPLICIT_CALL);

	return operandExplicitCast;
}

static void
run__opexpr_to_pxffilter__positive(Oid dbop, PxfOperatorCode expectedPxfOp)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	Var *arg_var = build_var(INT2OID, 1);
	char* const_value = strdup("1984"); /* will be free'd by const_to_str */
	Const* arg_const = build_const(INT2OID, const_value, true);

	OpExpr *expr = build_op_expr(arg_var, arg_const, dbop);
	PxfFilterDesc* expected = build_filter(
			PXF_ATTR_CODE, 1, NULL,
			PXF_SCALAR_CONST_CODE, 0, "1984",
			expectedPxfOp);

	/* run test */
	assert_true(opexpr_to_pxffilter(expr, filter));

	compare_filters(filter, expected);

	pxf_free_filter(expected);
	pxf_free_filter(filter);

	list_free_deep(expr->args); /* free all args */
	pfree(expr);
}

static void
test__opexpr_to_pxffilter__intGT(void **state)
{
	run__opexpr_to_pxffilter__positive(520 /* int2gt */, PXFOP_GT);
}

static void
test__opexpr_to_pxffilter__allSupportedTypes(void **state)
{
	int nargs = sizeof(pxf_supported_opr_op_expr) / sizeof(dbop_pxfop_map);
	PxfOperatorCode pxfop = 0;
	Oid dbop = InvalidOid;

	for (int i = 0; i < nargs; ++i)
	{
		dbop = pxf_supported_opr_op_expr[i].dbop;
		pxfop = pxf_supported_opr_op_expr[i].pxfop;
		run__opexpr_to_pxffilter__positive(dbop, pxfop);
	}
}

/* NOTE: this test is not  a use case - when the query includes
 * 'is null' or 'is not null' the qualifier code is T_NullTest and not T_OpExpr */
static void
test__opexpr_to_pxffilter__attributeEqualsNull(void **state)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	Var *arg_var = build_var(INT2OID, 1);
	Const* arg_const = build_const(INT2OID, NULL, true);
	OpExpr *expr = build_op_expr(arg_var, arg_const, 94 /* int2eq */);

	PxfFilterDesc* expected = build_filter(
				PXF_ATTR_CODE, 1, NULL,
				PXF_SCALAR_CONST_CODE, 0, NullConstValue,
				PXFOP_EQ);

	/* run test */
	assert_true(opexpr_to_pxffilter(expr, filter));
	compare_filters(filter, expected);

	pxf_free_filter(filter);
	pxf_free_filter(expected);

	list_free_deep(expr->args); /* free all args */
	pfree(expr);
}

static void
test__opexpr_to_pxffilter__attributeIsNull(void **state)
{
	Var *arg_var = build_var(INT2OID, 1);
	NullTest *expr = build_null_expr((Expr *) arg_var, IS_NULL);

	pfree(expr->arg);
	pfree(expr);
}

/*
 * Test for a query with different types.
 * Types pairing are not checked, it is covered by the
 * supported operations which are type specific.
 */
static void
test__opexpr_to_pxffilter__differentTypes(void **state)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	Var *arg_var = build_var(INT2OID, 3);
	char* const_value = strdup("13"); /* will be free'd by const_to_str */
	Const *arg_const = build_const(INT8OID, const_value, true);
	OpExpr *expr = build_op_expr(arg_const, arg_var, 1864 /* int28lt */);


	/* run test */
	assert_true(opexpr_to_pxffilter(expr, filter));
	PxfFilterDesc *expected = build_filter(
			PXF_SCALAR_CONST_CODE, 0, "13",
			PXF_ATTR_CODE, 3, NULL,
			PXFOP_LT);
	compare_filters(filter, expected);

	pxf_free_filter(filter);
	list_free_deep(expr->args); /* free all args */
	pfree(expr);
}

static void
test__opexpr_to_pxffilter__unsupportedTypeCircle(void **state)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	Var *arg_var = build_var(CIRCLEOID, 8);
	Const *arg_const = build_const(CIRCLEOID, NULL, true);
	OpExpr *expr = build_op_expr(arg_const, arg_var, 0 /* whatever */);

	/* run test */
	assert_false(opexpr_to_pxffilter(expr, filter));

	pxf_free_filter(filter);

	list_free_deep(expr->args); /* free all args */
	pfree(expr);
}

static void
test__opexpr_to_pxffilter__twoVars(void **state)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	Var *arg_var_left = build_var(INT4OID, 8);
	Var *arg_var_right = build_var(INT4OID, 9);
	OpExpr *expr = build_op_expr(arg_var_left, arg_var_right, 0 /* whatever */);

	/* run test */
	assert_false(opexpr_to_pxffilter(expr, filter));

	pxf_free_filter(filter);

	list_free_deep(expr->args); /* free all args */
	pfree(expr);
}

static void
test__opexpr_to_pxffilter__unsupportedOpNot(void **state)
{
	PxfFilterDesc *filter = (PxfFilterDesc*) palloc0(sizeof(PxfFilterDesc));
	Var *arg_var = build_var(INT2OID, 3);
	Const *arg_const = build_const(INT2OID, NULL, true);
	OpExpr *expr = build_op_expr(arg_const, arg_var, 1877 /* int2not */);

	/* run test */
	assert_false(opexpr_to_pxffilter(expr, filter));

	pxf_free_filter(filter);

	list_free_deep(expr->args); /* free all args */
	pfree(expr);
}

static void
test__pxf_serialize_filter_list__oneFilter(void **state)
{
    List* expressionItems = NIL;

    ExpressionItem* filterExpressionItem = build_expression_item(1, TEXTOID, "1984", TEXTOID, TextEqualOperator);

    expressionItems = lappend(expressionItems, filterExpressionItem);

    char* result = pxf_serialize_filter_list(expressionItems);
    assert_string_equal(result, "a0c25s4d1984o5");

    pxf_free_expression_items_list(expressionItems);
    expressionItems = NIL;
    pfree(result);

}

static void
test__pxf_serialize_fillter_list__nullFilter(void **state)
{

    List* expressionItems = NIL;

    ExpressionItem* filterExpressionItem = build_null_expression_item(1, TEXTOID, IS_NULL);

    expressionItems = lappend(expressionItems, filterExpressionItem);

    char* result = pxf_serialize_filter_list(expressionItems);
    assert_string_equal(result, "a0o8");

    pxf_free_expression_items_list(expressionItems);
    expressionItems = NIL;
    pfree(result);

}

static void
test__pxf_serialize_filter_list__manyFilters(void **state)
{
    char* result = NULL;
    List* expressionItems = NIL;

    ExpressionItem* expressionItem1 = build_expression_item(1, TEXTOID, "1984", TEXTOID, TextEqualOperator);
    ExpressionItem* expressionItem2 = build_expression_item(2, TEXTOID, "George Orwell", TEXTOID, TextEqualOperator);
    ExpressionItem* expressionItem3 = build_expression_item(3, TEXTOID, "Winston", TEXTOID, TextEqualOperator);
    ExpressionItem* expressionItem4 = build_expression_item(4, TEXTOID, "Eric-%", TEXTOID, 1209);
    ExpressionItem* expressionItem5 = build_expression_item(5, TEXTOID, "\"Ugly\" string with quotes", TEXTOID, TextEqualOperator);
    ExpressionItem* expressionItem6 = build_expression_item(6, TEXTOID, "", TEXTOID, TextEqualOperator);
    ExpressionItem* expressionItem7 = build_null_expression_item(7, TEXTOID, IS_NOT_NULL);

    expressionItems = lappend(expressionItems, expressionItem1);
    expressionItems = lappend(expressionItems, expressionItem2);
    expressionItems = lappend(expressionItems, expressionItem3);
    expressionItems = lappend(expressionItems, expressionItem4);
    expressionItems = lappend(expressionItems, expressionItem5);

    expressionItems = lappend(expressionItems, expressionItem6);
    expressionItems = lappend(expressionItems, expressionItem7);

    result = pxf_serialize_filter_list(expressionItems);
    assert_string_equal(result, "a0c25s4d1984o5a1c25s13dGeorge Orwello5a2c25s7dWinstono5a3c25s6dEric-%o7a4c25s25d\"Ugly\" string with quoteso5a5c25s0do5a6o9");
	pfree(result);

	int trivialExpressionItems = expressionItems->length;
	add_extra_and_expression_items(expressionItems, trivialExpressionItems - 1);

	assert_int_equal(expressionItems->length, 2 * trivialExpressionItems - 1);

	pxf_free_expression_items_list(expressionItems);
	expressionItems = NIL;
}

static void
test__serializePxfFilterQuals__emptyFilter(void **state)
{
	char* result = NULL;
	List* qualifierItems = NIL;

	result = serializePxfFilterQuals(qualifierItems);
	assert_true(result == NULL);
}

static void
test__serializePxfFilterQuals__oneFilter(void **state)
{
	char* result = NULL;

	List* qualifierItems = NIL;

	OpExpr* qualifierItem1 = build_qualifier(1, TEXTOID, "1984", TEXTOID, TextEqualOperator);
	qualifierItems = lappend(qualifierItems, qualifierItem1);

	result = serializePxfFilterQuals(qualifierItems);
	assert_string_equal(result, "a0c25s4d1984o5");
	pfree(result);
}

static void
test__serializePxfFilterQuals__nullFilter(void **state)
{
	char* result = NULL;

	List* qualifierItems = NIL;

	OpExpr* qualifierItem1 = (OpExpr*) build_null_expression_item(1, TEXTOID, IS_NULL)->node;
	qualifierItems = lappend(qualifierItems, qualifierItem1);

	result = serializePxfFilterQuals(qualifierItems);
	assert_string_equal(result, "a0o8");
	pfree(result);
}

static void
test__serializePxfFilterQuals__manyFilters(void **state)
{
    char* result = NULL;
    List* qualifierItems = NIL;

    OpExpr* qualifierItem1 = build_qualifier(1, TEXTOID, "1984", TEXTOID, TextEqualOperator);
    OpExpr* qualifierItem2 = build_qualifier(2, TEXTOID, "George Orwell", TEXTOID, TextEqualOperator);
    OpExpr* qualifierItem3 = build_qualifier(3, TEXTOID, "Winston", TEXTOID, TextEqualOperator);
    OpExpr* qualifierItem4 = build_qualifier(4, TEXTOID, "Eric-%", TEXTOID, 1209);
    OpExpr* qualifierItem5 = build_qualifier(5, TEXTOID, "\"Ugly\" string with quotes", TEXTOID, TextEqualOperator);
    OpExpr* qualifierItem6 = build_qualifier(6, TEXTOID, "", TEXTOID, TextEqualOperator);

    qualifierItems = lappend(qualifierItems, qualifierItem1);
    qualifierItems = lappend(qualifierItems, qualifierItem2);
    qualifierItems = lappend(qualifierItems, qualifierItem3);
    qualifierItems = lappend(qualifierItems, qualifierItem4);
    qualifierItems = lappend(qualifierItems, qualifierItem5);
    qualifierItems = lappend(qualifierItems, qualifierItem6);

    result = serializePxfFilterQuals(qualifierItems);
    // make sure the result has 5 "l0" at the end that correspond to implicit AND operation implied by planner's tree
    assert_string_equal(result, "a0c25s4d1984o5a1c25s13dGeorge Orwello5a2c25s7dWinstono5a3c25s6dEric-%o7a4c25s25d\"Ugly\" string with quoteso5a5c25s0do5l0l0l0l0l0");
    pfree(result);
}

static void
test__extractPxfAttributes_empty_quals(void **state)
{
	bool qualsAreSupported;
	qualsAreSupported = true;
	List* quals = NIL;
	extractPxfAttributes(quals, &qualsAreSupported);
	assert_true(qualsAreSupported);
}

/**
 * covers queries like:
 * SELECT a,b,c
 * FROM tab1
 * WHERE d = function(e) = const
 *
 * d is a column number 5 of int4 datatype
 * e is a column number 7 of date datatype
 * const is a int4 datatype
 *
 */
static void
test__extractPxfAttributes_supported_function_one_arg(void **state) {

	int argumentColumnIndex = 6; // index starts from 0
	bool qualsAreSupported;
	qualsAreSupported = true;
	List* columnsOids = list_make1_oid(DATEOID);
	List* attrsIndices = list_make1_int(argumentColumnIndex);

	// Create operands FuncExpr, Const
	FuncExpr* leftop = build_nested_func_expr_operand(columnsOids, attrsIndices);

	// extractPxfAttributes just extracts columns, doesn't read values of constants
	Node* rightop = (Node *) build_const(INT4OID, strdup("42"), false);
	OpExpr* opExpr = build_op_expr(leftop, rightop, Int4EqualOperator);
	List* quals = list_make1(opExpr);

	// Extract columns(attributes) from WHERE clause
	List* attrs = extractPxfAttributes(quals, &qualsAreSupported);

	// Make sure we extracted one attribute, column e
	assert_int_equal(1, attrs->length);

	// Make sure it's supported
	assert_true(qualsAreSupported);

	ListCell* lc = NULL;
	int attnum;
	foreach(lc, attrs)
	{
		attnum = lfirst_int(lc);
		// Index of attnum starts from 0
		assert_int_equal(attnum + 1, argumentColumnIndex);
	}
}

int
main(int argc, char* argv[])
{
	cmockery_parse_arguments(argc, argv);

	const UnitTest tests[] = {
			unit_test(test__supported_filter_type),
			unit_test(test__supported_operator_type_op_expr),
			unit_test(test__scalar_const_to_str__null),
			unit_test(test__scalar_const_to_str__int),
			unit_test(test__scalar_const_to_str__text),
			unit_test(test__scalar_const_to_str__NegativeCircle),
			unit_test(test__list_const_to_str__int),
			unit_test(test__opexpr_to_pxffilter__null),
			unit_test(test__opexpr_to_pxffilter__unary_expr),
			unit_test(test__opexpr_to_pxffilter__intGT),
			unit_test(test__opexpr_to_pxffilter__allSupportedTypes),
			unit_test(test__opexpr_to_pxffilter__attributeEqualsNull),
			unit_test(test__opexpr_to_pxffilter__differentTypes),
			unit_test(test__opexpr_to_pxffilter__unsupportedTypeCircle),
			unit_test(test__opexpr_to_pxffilter__twoVars),
			unit_test(test__opexpr_to_pxffilter__unsupportedOpNot),
			unit_test(test__opexpr_to_pxffilter__attributeIsNull),
			unit_test(test__pxf_serialize_filter_list__oneFilter),
			unit_test(test__pxf_serialize_fillter_list__nullFilter),
			unit_test(test__pxf_serialize_filter_list__manyFilters),
			unit_test(test__serializePxfFilterQuals__emptyFilter),
			unit_test(test__serializePxfFilterQuals__oneFilter),
			unit_test(test__serializePxfFilterQuals__nullFilter),
			unit_test(test__serializePxfFilterQuals__manyFilters),
			unit_test(test__extractPxfAttributes_empty_quals),
			unit_test(test__extractPxfAttributes_supported_function_one_arg)
	};

	MemoryContextInit();

	return run_tests(tests);
}
