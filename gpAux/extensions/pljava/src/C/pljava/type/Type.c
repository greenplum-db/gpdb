/*
 * Copyright (c) 2004, 2005, 2006 TADA AB - Taby Sweden
 * Distributed under the terms shown in the file COPYRIGHT
 * found in the root folder of this project or at
 * http://eng.tada.se/osprojects/COPYRIGHT.html
 *
 * @author Thomas Hallgren
 */
#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#include <parser/parse_coerce.h>
#include <utils/typcache.h>
#include <utils/lsyscache.h>

#include "pljava/type/String_priv.h"
#include "pljava/type/Array.h"
#include "pljava/type/Coerce.h"
#include "pljava/type/Composite.h"
#include "pljava/type/TupleDesc.h"
#include "pljava/type/Oid.h"
#include "pljava/type/UDT.h"
#include "pljava/Invocation.h"
#include "pljava/HashMap.h"
#include "pljava/SPI.h"

static HashMap s_typeByOid;
static HashMap s_obtainerByOid;
static HashMap s_obtainerByJavaName;

static jclass s_Map_class;
static jmethodID s_Map_get;

typedef struct CacheEntryData
{
	Type			type;
	TypeObtainer	obtainer;
	Oid				typeId;
} CacheEntryData;

typedef CacheEntryData* CacheEntry;

static jclass s_Iterator_class;
static jmethodID s_Iterator_hasNext;
static jmethodID s_Iterator_next;

/* Structure used in multi function calls (calls returning
 * SETOF <composite type>)
 */
typedef struct
{
	Type          elemType;
	jobject       rowProducer;
	jobject       rowCollector;
	jobject       invocation;
	MemoryContext rowContext;
	MemoryContext spiContext;
	bool          hasConnected;
	bool          trusted;
} CallContextData;

static void _closeIteration(CallContextData* ctxData)
{
	currentInvocation->hasConnected = ctxData->hasConnected;
	currentInvocation->invocation   = ctxData->invocation;

	Type_closeSRF(ctxData->elemType, ctxData->rowProducer);
	JNI_deleteGlobalRef(ctxData->rowProducer);
	if(ctxData->rowCollector != 0)
		JNI_deleteGlobalRef(ctxData->rowCollector);
	MemoryContextDelete(ctxData->rowContext);

	if(ctxData->hasConnected && ctxData->spiContext != 0)
	{
		/* Connect during SRF_IS_FIRSTCALL(). Switch context back to what
		 * it was at that time and disconnect.
		 */
		MemoryContext currCtx = MemoryContextSwitchTo(ctxData->spiContext);
		Invocation_assertDisconnect();
		MemoryContextSwitchTo(currCtx);
	}
}

static void _endOfSetCB(Datum arg)
{
	Invocation topCall;
	bool saveInExprCtxCB;
	CallContextData* ctxData = (CallContextData*)DatumGetPointer(arg);
	if(currentInvocation == 0)
		Invocation_pushInvocation(&topCall, ctxData->trusted);

	saveInExprCtxCB = currentInvocation->inExprContextCB;
	currentInvocation->inExprContextCB = true;
	_closeIteration(ctxData);
	currentInvocation->inExprContextCB = saveInExprCtxCB;
}

Type Type_getCoerceIn(Type self, Type other)
{
	Oid  funcId;
	Type coerce;
	CoercionPathType	pathtype = COERCION_PATH_NONE;
	Oid  fromOid = other->typeId;
	Oid  toOid = self->typeId;

	if(self->inCoercions != 0)
	{
		coerce = HashMap_getByOid(self->inCoercions, fromOid);
		if(coerce != 0)
			return coerce;
	}

	pathtype = find_coercion_pathway(toOid, fromOid, COERCION_EXPLICIT, &funcId);
	if (pathtype == COERCION_PATH_RELABELTYPE)
	{
		/*
		 * Binary compatible type. No need for a special coercer
		 */
		return self;
	}
	else if (!OidIsValid(funcId))
	{
		elog(ERROR, "no conversion function from %s to %s",
			 format_type_be(fromOid),
			 format_type_be(toOid));
	}

	if(self->inCoercions == 0)
		self->inCoercions = HashMap_create(7, GetMemoryChunkContext(self));

	coerce = Coerce_createIn(self, other, funcId);
	HashMap_putByOid(self->inCoercions, fromOid, coerce);
	return coerce;
}

Type Type_getCoerceOut(Type self, Type other)
{
	Oid  funcId;
	Type coercer;
	CoercionPathType	pathtype = COERCION_PATH_NONE;
	Oid  fromOid = self->typeId;
	Oid  toOid = other->typeId;

	if(self->outCoercions != 0)
	{
		coercer = HashMap_getByOid(self->outCoercions, toOid);
		if(coercer != 0)
			return coercer;
	}

	pathtype = find_coercion_pathway(toOid, fromOid, COERCION_EXPLICIT, &funcId);
	if (pathtype == COERCION_PATH_RELABELTYPE)
	{
		/*
		 * Binary compatible type. No need for a special coercer
		 */
		return self;
	}
	else if (!OidIsValid(funcId))
	{
		elog(ERROR, "no conversion function from %s to %s",
			 format_type_be(fromOid),
			 format_type_be(toOid));
	}

	if(self->outCoercions == 0)
		self->outCoercions = HashMap_create(7, GetMemoryChunkContext(self));

	coercer = Coerce_createOut(self, other, funcId);
	HashMap_putByOid(self->outCoercions, toOid, coercer);
	return coercer;
}

bool Type_canReplaceType(Type self, Type other)
{
	return self->typeClass->canReplaceType(self, other);
}

bool Type_isDynamic(Type self)
{
	return self->typeClass->dynamic;
}

bool Type_isOutParameter(Type self)
{
	return self->typeClass->outParameter;
}

jvalue Type_coerceDatum(Type self, Datum value)
{
	return self->typeClass->coerceDatum(self, value);
}

Datum Type_coerceObject(Type self, jobject object)
{
	return self->typeClass->coerceObject(self, object);
}

char Type_getAlign(Type self)
{
	return self->align;
}

TypeClass Type_getClass(Type self)
{
	return self->typeClass;
}

int16 Type_getLength(Type self)
{
	return self->length;
}

bool Type_isByValue(Type self)
{
	return self->byValue;
}

jclass Type_getJavaClass(Type self)
{
	TypeClass typeClass = self->typeClass;
	if(typeClass->javaClass == 0)
	{
		jclass cls;
		const char* cp = typeClass->JNISignature;
		if(cp == 0 || *cp == 0)
			ereport(ERROR, (
				errmsg("Type '%s' has no corresponding java class",
					PgObjectClass_getName((PgObjectClass)typeClass))));

		if(*cp == 'L')
		{
			/* L<object name>; should be just <object name>. Strange
			 * since the L and ; are retained if its an array.
			 */
			int len = strlen(cp) - 2;
			char* bp = palloc(len + 1);
			memcpy(bp, cp + 1, len);
			bp[len] = 0;
			cls = PgObject_getJavaClass(bp);
			pfree(bp);
		}
		else
			cls = PgObject_getJavaClass(cp);

		typeClass->javaClass = JNI_newGlobalRef(cls);
		JNI_deleteLocalRef(cls);
	}
	return typeClass->javaClass;
}

const char* Type_getJavaTypeName(Type self)
{
	return self->typeClass->javaTypeName;
}

const char* Type_getJNISignature(Type self)
{
	return self->typeClass->getJNISignature(self);
}

const char* Type_getJNIReturnSignature(Type self, bool forMultiCall, bool useAltRepr)
{
	return self->typeClass->getJNIReturnSignature(self, forMultiCall, useAltRepr);
}

Type Type_getArrayType(Type self, Oid arrayTypeId)
{
	Type arrayType = self->arrayType;
	if(arrayType != 0)
	{
		if(arrayType->typeId == arrayTypeId)
			return arrayType;

		if(arrayType->typeId == InvalidOid)
		{
			arrayType->typeId = arrayTypeId;
			return arrayType;
		}
	}
	arrayType = self->typeClass->createArrayType(self, arrayTypeId);
	self->arrayType = arrayType;
	return arrayType;
}

Type Type_getElementType(Type self)
{
	return self->elementType;
}

Type Type_getObjectType(Type self)
{
	return self->objectType;
}

Type Type_getRealType(Type self, Oid realTypeId, jobject typeMap)
{
	return self->typeClass->getRealType(self, realTypeId, typeMap);
}

Oid Type_getOid(Type self)
{
	return self->typeId;
}

TupleDesc Type_getTupleDesc(Type self, PG_FUNCTION_ARGS)
{
	return self->typeClass->getTupleDesc(self, fcinfo);
}

Datum Type_invoke(Type self, jclass cls, jmethodID method, jvalue* args, PG_FUNCTION_ARGS)
{
	return self->typeClass->invoke(self, cls, method, args, fcinfo);
}

Datum Type_invokeSRF(Type self, jclass cls, jmethodID method, jvalue* args, PG_FUNCTION_ARGS)
{
	bool hasRow;
	CallContextData* ctxData;
	FuncCallContext* context;
	MemoryContext currCtx;

	/* stuff done only on the first call of the function
	 */
	if(SRF_IS_FIRSTCALL())
	{
		jobject tmp;

		/* create a function context for cross-call persistence
		 */
		context = SRF_FIRSTCALL_INIT();
		currCtx = MemoryContextSwitchTo(context->multi_call_memory_ctx);

		/* Call the declared Java function. It returns an instance that can produce
		 * the rows.
		 */
		tmp = Type_getSRFProducer(self, cls, method, args);
		if(tmp == 0)
		{
			Invocation_assertDisconnect();
			MemoryContextSwitchTo(currCtx);
			fcinfo->isnull = true;
			SRF_RETURN_DONE(context);
		}

		ctxData = (CallContextData*)palloc(sizeof(CallContextData));
		context->user_fctx = ctxData;

		ctxData->elemType = self;
		ctxData->rowProducer = JNI_newGlobalRef(tmp);
		JNI_deleteLocalRef(tmp);

		/* Some row producers will need a writable result set in order
		 * to produce the row. If one is needed, it's created here.
		 */
		tmp = Type_getSRFCollector(self, fcinfo);
		if(tmp == 0)
			ctxData->rowCollector = 0;
		else
		{
			ctxData->rowCollector = JNI_newGlobalRef(tmp);
			JNI_deleteLocalRef(tmp);
		}		

		ctxData->trusted       = currentInvocation->trusted;
		ctxData->hasConnected  = currentInvocation->hasConnected;
		ctxData->invocation    = currentInvocation->invocation;
		if(ctxData->hasConnected)
			ctxData->spiContext = CurrentMemoryContext;
		else
			ctxData->spiContext = 0;

		ctxData->rowContext = AllocSetContextCreate(context->multi_call_memory_ctx,
								  "PL/Java row context",
								  ALLOCSET_DEFAULT_MINSIZE,
								  ALLOCSET_DEFAULT_INITSIZE,
								  ALLOCSET_DEFAULT_MAXSIZE);

		/* Register callback to be called when the function ends
		 */
		RegisterExprContextCallback(((ReturnSetInfo*)fcinfo->resultinfo)->econtext, _endOfSetCB, PointerGetDatum(ctxData));
		MemoryContextSwitchTo(currCtx);
	}

	context = SRF_PERCALL_SETUP();
	ctxData = (CallContextData*)context->user_fctx;
	MemoryContextReset(ctxData->rowContext);
	currCtx = MemoryContextSwitchTo(ctxData->rowContext);
	currentInvocation->hasConnected = ctxData->hasConnected;
	currentInvocation->invocation   = ctxData->invocation;

	hasRow = Type_hasNextSRF(self, ctxData->rowProducer, ctxData->rowCollector, (jint)context->call_cntr);

	ctxData->hasConnected = currentInvocation->hasConnected;
	ctxData->invocation   = currentInvocation->invocation;
	currentInvocation->hasConnected = false;
	currentInvocation->invocation   = 0;

	if(hasRow)
	{
		Datum result = Type_nextSRF(self, ctxData->rowProducer, ctxData->rowCollector);
		MemoryContextSwitchTo(currCtx);
		SRF_RETURN_NEXT(context, result);
	}

	MemoryContextSwitchTo(currCtx);

	/* Unregister this callback and call it manually. We do this because
	 * otherwise it will be called when the backend is in progress of
	 * cleaning up Portals. If we close cursors (i.e. drop portals) in
	 * the close, then that mechanism fails since attempts are made to
	 * delete portals more then once.
	 */
	UnregisterExprContextCallback(
		((ReturnSetInfo*)fcinfo->resultinfo)->econtext,
		_endOfSetCB,
		PointerGetDatum(ctxData));

	_closeIteration(ctxData);

	/* This is the end of the set.
	 */
	SRF_RETURN_DONE(context);
}

bool Type_isPrimitive(Type self)
{
	return self->objectType != 0;
}

Type Type_fromJavaType(Oid typeId, const char* javaTypeName)
{
	CacheEntry ce = (CacheEntry)HashMap_getByString(s_obtainerByJavaName, javaTypeName);
	if(ce == 0)
	{
		int jtlen = strlen(javaTypeName) - 2;
		if(jtlen > 0 && strcmp("[]", javaTypeName + jtlen) == 0)
		{
			Type type;
			char* elemName = palloc(jtlen+1);
			memcpy(elemName, javaTypeName, jtlen);
			elemName[jtlen] = 0;
			type = Type_getArrayType(Type_fromJavaType(InvalidOid, elemName), typeId);
			pfree(elemName);
			return type;
		}
		ereport(ERROR, (
			errcode(ERRCODE_CANNOT_COERCE),
			errmsg("No java type mapping installed for \"%s\"", javaTypeName)));
	}

	return ce->type == 0
		? ce->obtainer(typeId == InvalidOid ? ce->typeId : typeId)
		: ce->type;
}

void Type_cacheByOid(Oid typeId, Type type)
{
	HashMap_putByOid(s_typeByOid, typeId, type);
}

Type Type_fromOidCache(Oid typeId)
{
	return (Type)HashMap_getByOid(s_typeByOid, typeId);
}

Type Type_fromOid(Oid typeId, jobject typeMap)
{
	CacheEntry   ce;
	HeapTuple    typeTup;
	Form_pg_type typeStruct;
	Type         type = Type_fromOidCache(typeId);

	if(type != 0)
		return type;

	typeTup    = PgObject_getValidTuple(TYPEOID, typeId, "type");
	typeStruct = (Form_pg_type)GETSTRUCT(typeTup);

	if(typeStruct->typelem != 0 && typeStruct->typlen == -1)
	{
		type = Type_getArrayType(Type_fromOid(typeStruct->typelem, typeMap), typeId);
		goto finally;
	}

	/* For some reason, the anyarray is *not* an array with anyelement as the
	 * element type. We'd like to see it that way though.
	 */
	if(typeId == ANYARRAYOID)
	{
		type = Type_getArrayType(Type_fromOid(ANYELEMENTOID, typeMap), typeId);
		goto finally;
	}

	if(typeStruct->typbasetype != 0)
	{
		/* Domain type, recurse using the base type (which in turn may
		 * also be a domain)
		 */
		type = Type_fromOid(typeStruct->typbasetype, typeMap);
		goto finally;
	}

	if(typeMap != 0)
	{
		jobject joid      = Oid_create(typeId);
		jclass  typeClass = (jclass)JNI_callObjectMethod(typeMap, s_Map_get, joid);

		JNI_deleteLocalRef(joid);
		if(typeClass != 0)
		{
			TupleDesc tupleDesc = lookup_rowtype_tupdesc_noerror(typeId, -1, true);
			type = (Type)UDT_registerUDT(typeClass, typeId, typeStruct, tupleDesc, false);
			JNI_deleteLocalRef(typeClass);
			goto finally;
		}
	}

	/* Composite and record types will not have a TypeObtainer registered
	 */
	if(typeStruct->typtype == 'c' || (typeStruct->typtype == 'p' && typeId == RECORDOID))
	{
		type = Composite_obtain(typeId);
		goto finally;
	}

	ce = (CacheEntry)HashMap_getByOid(s_obtainerByOid, typeId);
	if(ce == 0)
		/*
		 * Default to String and standard textin/textout coersion.
		 */
		type = String_obtain(typeId);
	else
	{
		type = ce->type;
		if(type == 0)
			type = ce->obtainer(typeId);
	}

finally:
	ReleaseSysCache(typeTup);
	Type_cacheByOid(typeId, type);
	return type;
}

Type Type_objectTypeFromOid(Oid typeId, jobject typeMap)
{
	Type type = Type_fromOid(typeId, typeMap);
	Type objectType = type->objectType;
	return (objectType == 0) ? type : objectType;
}

bool _Type_canReplaceType(Type self, Type other)
{
	return self->typeClass == other->typeClass;
}

Datum _Type_invoke(Type self, jclass cls, jmethodID method, jvalue* args, PG_FUNCTION_ARGS)
{
	MemoryContext currCtx;
	Datum ret;
	jobject value = JNI_callStaticObjectMethodA(cls, method, args);
	if(value == 0)
	{
		fcinfo->isnull = true;
		return 0;
	}

	/* The return value cannot be created in the current context since it
	 * goes out of scope when SPI_finish is called.
	 */
	currCtx = Invocation_switchToUpperContext();
	ret = self->typeClass->coerceObject(self, value);
	MemoryContextSwitchTo(currCtx);
	JNI_deleteLocalRef(value);
	return ret;
}

static Type _Type_createArrayType(Type self, Oid arrayTypeId)
{
	return Array_fromOid(arrayTypeId, self);
}

static jobject _Type_getSRFProducer(Type self, jclass cls, jmethodID method, jvalue* args)
{
	return JNI_callStaticObjectMethodA(cls, method, args);
}

static jobject _Type_getSRFCollector(Type self, PG_FUNCTION_ARGS)
{
	return 0;
}

static bool _Type_hasNextSRF(Type self, jobject rowProducer, jobject rowCollector, jint callCounter)
{
	return (JNI_callBooleanMethod(rowProducer, s_Iterator_hasNext) == JNI_TRUE);
}

static Datum _Type_nextSRF(Type self, jobject rowProducer, jobject rowCollector)
{
	jobject tmp = JNI_callObjectMethod(rowProducer, s_Iterator_next);
	Datum result = Type_coerceObject(self, tmp);
	JNI_deleteLocalRef(tmp);
	return result;
}

static void _Type_closeSRF(Type self, jobject rowProducer)
{
}

jobject Type_getSRFProducer(Type self, jclass cls, jmethodID method, jvalue* args)
{
	return self->typeClass->getSRFProducer(self, cls, method, args);
}

jobject Type_getSRFCollector(Type self, PG_FUNCTION_ARGS)
{
	return self->typeClass->getSRFCollector(self, fcinfo);
}

bool Type_hasNextSRF(Type self, jobject rowProducer, jobject rowCollector, jint callCounter)
{
	return self->typeClass->hasNextSRF(self, rowProducer, rowCollector, callCounter);
}

Datum Type_nextSRF(Type self, jobject rowProducer, jobject rowCollector)
{
	return self->typeClass->nextSRF(self, rowProducer, rowCollector);
}

void Type_closeSRF(Type self, jobject rowProducer)
{
	self->typeClass->closeSRF(self, rowProducer);
}

static Type _Type_getRealType(Type self, Oid realId, jobject typeMap)
{
	return self;
}

static const char* _Type_getJNISignature(Type self)
{
	return self->typeClass->JNISignature;
}

static const char* _Type_getJNIReturnSignature(Type self, bool forMultiCall, bool useAltRepr)
{
	return forMultiCall ? "Ljava/util/Iterator;" : Type_getJNISignature(self);
}

TupleDesc _Type_getTupleDesc(Type self, PG_FUNCTION_ARGS)
{
	ereport(ERROR,
		(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
		 errmsg("Type is not associated with a record")));
	return 0;	/* Keep compiler happy */
}

/*
 * Shortcuts to initializers of known types
 */
extern void Any_initialize(void);
extern void Coerce_initialize(void);
extern void Void_initialize(void);
extern void Boolean_initialize(void);
extern void Byte_initialize(void);
extern void Short_initialize(void);
extern void Integer_initialize(void);
extern void Long_initialize(void);
extern void Float_initialize(void);
extern void Double_initialize(void);
extern void BigDecimal_initialize(void);

extern void Date_initialize(void);
extern void Time_initialize(void);
extern void Timestamp_initialize(void);

extern void Oid_initialize(void);
extern void AclId_initialize(void);
extern void ErrorData_initialize(void);
extern void LargeObject_initialize(void);

extern void String_initialize(void);
extern void byte_array_initialize(void);

extern void JavaWrapper_initialize(void);
extern void ExecutionPlan_initialize(void);
extern void Portal_initialize(void);
extern void Relation_initialize(void);
extern void TriggerData_initialize(void);
extern void Tuple_initialize(void);
extern void TupleDesc_initialize(void);
extern void TupleTable_initialize(void);

extern void Composite_initialize(void);

extern void Type_initialize(void);
void Type_initialize(void)
{
	s_typeByOid          = HashMap_create(59, TopMemoryContext);
	s_obtainerByOid      = HashMap_create(59, TopMemoryContext);
	s_obtainerByJavaName = HashMap_create(59, TopMemoryContext);

	String_initialize();

	Any_initialize();
	Coerce_initialize();
	Void_initialize();
	Boolean_initialize();
	Byte_initialize();
	Short_initialize();
	Integer_initialize();
	Long_initialize();
	Float_initialize();
	Double_initialize();

	BigDecimal_initialize();

	Date_initialize();
	Time_initialize();
	Timestamp_initialize();

	Oid_initialize();
	AclId_initialize();
	ErrorData_initialize();
	LargeObject_initialize();

	byte_array_initialize();

	JavaWrapper_initialize();
	ExecutionPlan_initialize();
	Portal_initialize();
	TriggerData_initialize();
	Relation_initialize();
	TupleDesc_initialize();
	Tuple_initialize();
	TupleTable_initialize();

	Composite_initialize();

	s_Map_class = JNI_newGlobalRef(PgObject_getJavaClass("java/util/Map"));
	s_Map_get = PgObject_getJavaMethod(s_Map_class, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");

	s_Iterator_class = JNI_newGlobalRef(PgObject_getJavaClass("java/util/Iterator"));
	s_Iterator_hasNext = PgObject_getJavaMethod(s_Iterator_class, "hasNext", "()Z");
	s_Iterator_next = PgObject_getJavaMethod(s_Iterator_class, "next", "()Ljava/lang/Object;");
}

/*
 * Abstract Type constructor
 */
TypeClass TypeClass_alloc(const char* typeName)
{
	return TypeClass_alloc2(typeName, sizeof(struct TypeClass_), sizeof(struct Type_));
}

TypeClass TypeClass_alloc2(const char* typeName, Size classSize, Size instanceSize)
{
	TypeClass self = (TypeClass)MemoryContextAlloc(TopMemoryContext, classSize);
	PgObjectClass_init((PgObjectClass)self, typeName, instanceSize, 0);
	self->JNISignature    = "";
	self->javaTypeName    = "";
	self->javaClass       = 0;
	self->canReplaceType  = _Type_canReplaceType;
	self->coerceDatum     = (DatumCoercer)_PgObject_pureVirtualCalled;
	self->coerceObject    = (ObjectCoercer)_PgObject_pureVirtualCalled;
	self->createArrayType = _Type_createArrayType;
	self->invoke          = _Type_invoke;
	self->getSRFProducer  = _Type_getSRFProducer;
	self->getSRFCollector = _Type_getSRFCollector;
	self->hasNextSRF      = _Type_hasNextSRF;
	self->nextSRF         = _Type_nextSRF;
	self->closeSRF        = _Type_closeSRF;
	self->getTupleDesc    = _Type_getTupleDesc;
	self->getJNISignature = _Type_getJNISignature;
	self->getJNIReturnSignature = _Type_getJNIReturnSignature;
	self->dynamic         = false;
	self->outParameter    = false;
	self->getRealType     = _Type_getRealType;
	return self;
}

/*
 * Types are always allocated in global context.
 */
Type TypeClass_allocInstance(TypeClass cls, Oid typeId)
{
	return TypeClass_allocInstance2(cls, typeId, 0);
}

/*
 * Types are always allocated in global context.
 */
Type TypeClass_allocInstance2(TypeClass cls, Oid typeId, Form_pg_type pgType)
{
	Type t = (Type)PgObjectClass_allocInstance((PgObjectClass)(cls), TopMemoryContext);
	t->typeId       = typeId;
	t->arrayType    = 0;
	t->elementType  = 0;
	t->objectType   = 0;
	t->inCoercions  = 0;
	t->outCoercions = 0;
	if(pgType != 0)
	{
		t->length  = pgType->typlen;
		t->byValue = pgType->typbyval;
		t->align   = pgType->typalign;
	}
	else if(typeId != InvalidOid)
	{
		get_typlenbyvalalign(typeId,
						 &t->length,
						 &t->byValue,
						 &t->align);
	}
	else
	{
		t->length = 0;
		t->byValue = true;
		t->align = 'i';
	}
	return t;
}

/*
 * Register this type.
 */
static void _registerType(Oid typeId, const char* javaTypeName, Type type, TypeObtainer obtainer)
{
	CacheEntry ce = (CacheEntry)MemoryContextAlloc(TopMemoryContext, sizeof(CacheEntryData));
	ce->typeId   = typeId;
	ce->type     = type;
	ce->obtainer = obtainer;

	if(javaTypeName != 0)
		HashMap_putByString(s_obtainerByJavaName, javaTypeName, ce);

	if(typeId != InvalidOid && HashMap_getByOid(s_obtainerByOid, typeId) == 0)
		HashMap_putByOid(s_obtainerByOid, typeId, ce);
}

void Type_registerType(const char* javaTypeName, Type type)
{
	_registerType(type->typeId, javaTypeName, type, (TypeObtainer)_PgObject_pureVirtualCalled);
}

void Type_registerType2(Oid typeId, const char* javaTypeName, TypeObtainer obtainer)
{
	_registerType(typeId, javaTypeName, 0, obtainer);
}
