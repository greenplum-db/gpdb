/*-------------------------------------------------------------------------
 *
 * extprotocol.h
 *	  Declarations for External Table Protocol functions
 *
 * Portions Copyright (c) 2010, EMC corporation
 * Portions Copyright (c) 2012-Present Pivotal Software, Inc.
 *
 *
 * IDENTIFICATION
 *	    src/include/access/extprotocol.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef EXTPROTOCOL_H
#define EXTPROTOCOL_H

#include "nodes/nodes.h"
#include "nodes/pg_list.h"
#include "nodes/value.h"
#include "utils/rel.h"

/* ------------------------- I/O function API -----------------------------*/

/*
 * ExtProtocolData is the node type that is passed as fmgr "context" info
 * when a function is called by the External Table protocol manager.
 */
typedef struct ExtProtocolData
{
	NodeTag			type;                 /* see T_ExtProtocolData */
	Relation    	prot_relation;
	char*			prot_url;
	char*			prot_databuf;
	int				prot_maxbytes;
	void*			prot_user_ctx;
	bool			prot_last_call;
		
} ExtProtocolData;

typedef ExtProtocolData *ExtProtocol;

#define CALLED_AS_EXTPROTOCOL(fcinfo) \
	((fcinfo->context != NULL && IsA((fcinfo)->context, ExtProtocolData)))

#define EXTPROTOCOL_GET_URL(fcinfo)    	   (((ExtProtocolData*) fcinfo->context)->prot_url)
#define EXTPROTOCOL_GET_RELATION(fcinfo)   (((ExtProtocolData*) fcinfo->context)->prot_relation)
#define EXTPROTOCOL_GET_DATABUF(fcinfo)    (((ExtProtocolData*) fcinfo->context)->prot_databuf)
#define EXTPROTOCOL_GET_DATALEN(fcinfo)    (((ExtProtocolData*) fcinfo->context)->prot_maxbytes)
#define EXTPROTOCOL_GET_USER_CTX(fcinfo)   (((ExtProtocolData*) fcinfo->context)->prot_user_ctx)
#define EXTPROTOCOL_IS_LAST_CALL(fcinfo)   (((ExtProtocolData*) fcinfo->context)->prot_last_call)

#define EXTPROTOCOL_SET_LAST_CALL(fcinfo)  (((ExtProtocolData*) fcinfo->context)->prot_last_call = true)
#define EXTPROTOCOL_SET_USER_CTX(fcinfo, p) \
	(((ExtProtocolData*) fcinfo->context)->prot_user_ctx = p)


/* ------------------------- Validator function API -----------------------------*/

typedef enum ValidatorDirection
{
	EXT_VALIDATE_READ,
	EXT_VALIDATE_WRITE
} ValidatorDirection;

/*
 * ExtProtocolValidatorData is the node type that is passed as fmgr "context" info
 * when a function is called by the External Table protocol manager.
 */
typedef struct ExtProtocolValidatorData
{
	NodeTag				 type;            /* see T_ExtProtocolValidatorData */
	List 		  		*url_list;
	ValidatorDirection 	 direction;  /* validating read or write? */
	char				*errmsg;		  /* the validation error upon return, if any */
	
} ExtProtocolValidatorData;

typedef ExtProtocolValidatorData *ExtProtocolValidator;

#define CALLED_AS_EXTPROTOCOL_VALIDATOR(fcinfo) \
	((fcinfo->context != NULL && IsA((fcinfo)->context, ExtProtocolValidatorData)))

#define EXTPROTOCOL_VALIDATOR_GET_URL_LIST(fcinfo)	(((ExtProtocolValidatorData*) fcinfo->context)->url_list)
#define EXTPROTOCOL_VALIDATOR_GET_NUM_URLS(fcinfo)	(list_length(((ExtProtocolValidatorData*) fcinfo->context)->url_list))

#define EXTPROTOCOL_VALIDATOR_GET_NTH_URL(fcinfo, n) (((Value *)(list_nth(EXTPROTOCOL_VALIDATOR_GET_URL_LIST(fcinfo),(n - 1))))->val.str)
#define EXTPROTOCOL_VALIDATOR_GET_DIRECTION(fcinfo)	(((ExtProtocolValidatorData*) fcinfo->context)->direction)


#endif /* EXTPROTOCOL_H */
