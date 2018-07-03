/*
 * pxffilters.h
 *
 * Header for handling push down of supported scan level filters to PXF.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.	See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
#ifndef _PXF_FILTERS_H_
#define _PXF_FILTERS_H_

#include "postgres.h"
#include "access/attnum.h"
#include "executor/executor.h"
#include "nodes/pg_list.h"


/*
 * each supported operator has a code that will describe the operator
 * type in the final serialized string that gets pushed down.
 *
 * NOTE: the codes could be forced into a single byte, but list will
 * grow larger in the future, so why bother.
 */
typedef enum PxfOperatorCode
{
	PXFOP_LT = 1,
	PXFOP_GT,
	PXFOP_LE,
	PXFOP_GE,
	PXFOP_EQ,
	PXFOP_NE,
	PXFOP_LIKE,
	PXFOP_IS_NULL,
	PXFOP_IS_NOTNULL,
	PXFOP_IN

}	PxfOperatorCode;

/*
 * each supported operand from both sides of the operator is represented
 * by a code that will describe the operator type in the final serialized
 * string that gets pushed down.
 */
#define PXF_ATTR_CODE				'a'
#define PXF_SCALAR_CONST_CODE		'c'
#define PXF_LIST_CONST_CODE			'm'
#define PXF_SIZE_BYTES				's'
#define PXF_CONST_DATA				'd'
#define PXF_OPERATOR_CODE			'o'
#define PXF_LOGICAL_OPERATOR_CODE	'l'

#define NullConstValue	 "NULL"
#define TrueConstValue	 "true"
#define FalseConstValue   "false"

/*
 * An Operand has any of the above codes, and the information specific to
 * its type. This could be compacted but filter structures are expected to
 * be very few and very small so it's ok as is.
 */
typedef struct PxfOperand
{
	char		opcode;			/* PXF_ATTR_CODE, PXF_SCALAR_CONST_CODE,
								 * PXF_LIST_CONST_CODE */
	AttrNumber	attnum;			/* used when opcode is PXF_ATTR_CODE */
	StringInfo	conststr;		/* used when opcode is PXF_SCALAR_CONST_CODE
								 * or PXF_LIST_CONST_CODE */
	Oid			consttype;		/* used when opcode is PXF_SCALAR_CONST_CODE
								 * or PXF_LIST_CONST_CODE */

}	PxfOperand;

/*
 * A PXF filter has a left and right operands, and an operator.
 */
typedef struct PxfFilterDesc
{
	PxfOperand	l;				/* left operand */
	PxfOperand	r;				/* right operand or InvalidAttrNumber if none */
	PxfOperatorCode op;			/* operator code */

}	PxfFilterDesc;

/*
 * A mapping of GPDB operator OID to PXF operator code.
 * Used for Node of type 'T_OpExpr'
 */
typedef struct dbop_pxfop_map
{
	Oid			dbop;
	PxfOperatorCode pxfop;

}	dbop_pxfop_map;

/*
 * A mapping of GPDB operator OID to PXF operator code ('PXFOP_IN').
 * Used for Node of type 'T_ScalarArrayOpExpr'
 */
typedef struct dbop_pxfop_array_map
{
	Oid			dbop;
	PxfOperatorCode pxfop;
	bool		useOr;

}	dbop_pxfop_array_map;

typedef struct ExpressionItem
{
	Node	   *node;
	Node	   *parent;
	bool		processed;
}	ExpressionItem;

static inline bool
pxfoperand_is_attr(PxfOperand x)
{
	return (x.opcode == PXF_ATTR_CODE);
}

static inline bool
pxfoperand_is_scalar_const(PxfOperand x)
{
	return (x.opcode == PXF_SCALAR_CONST_CODE);
}

static inline bool
pxfoperand_is_list_const(PxfOperand x)
{
	return (x.opcode == PXF_LIST_CONST_CODE);
}

char	   *serializePxfFilterQuals(List *quals);
List	   *extractPxfAttributes(List *quals, bool *qualsAreSupported);

#endif   /* // _PXF_FILTERS_H_ */
