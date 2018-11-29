#ifndef _PXFUTILS_H_
#define _PXFUTILS_H_

#include "postgres.h"

/* convert input string to upper case and prepend "X-GP-OPTIONS-" prefix */
char	   *normalize_key_name(const char *key);

/* get the name of the type, given the OID */
char	   *TypeOidGetTypename(Oid typid);

/* Concatenate multiple literal strings using stringinfo */
char	   *concat(int num_args,...);

/* Get authority (host:port) for the PXF server URL */
char	   *get_authority(void);

#define PXF_PROFILE       "PROFILE"
#define FRAGMENTER        "FRAGMENTER"
#define ACCESSOR          "ACCESSOR"
#define RESOLVER          "RESOLVER"
#define ANALYZER          "ANALYZER"
#define PxfDefaultHost    "localhost"
#define PxfDefaultPort    5888

#endif							/* _PXFUTILS_H_ */
