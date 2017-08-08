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

#include "pxfutils.h"
#include "utils/formatting.h"
#include "utils/syscache.h"

/*
 * Checks if two strings are equal
 */
bool
are_ips_equal(char *ip1, char *ip2)
{
    if ((ip1 == NULL) || (ip2 == NULL))
        return false;
    return (strcmp(ip1, ip2) == 0);
}

/*
 * Override port str with given new port int
 */
void
port_to_str(char **port, int new_port)
{
    char tmp[10];

    if (!port)
        elog(ERROR, "unexpected internal error in pxfutils.c");
    if (*port)
        pfree(*port);

    Assert((new_port <= 65535) && (new_port >= 1)); /* legal port range */
    pg_ltoa(new_port, tmp);
    *port = pstrdup(tmp);
}

/*
 * Full name of the HEADER KEY expected by the PXF service
 * Converts input string to upper case and prepends "X-GP-" string
 *
 */
char*
normalize_key_name(const char* key)
{
    if (!key || strlen(key) == 0)
    {
        ereport(ERROR,
                (errcode(ERRCODE_INTERNAL_ERROR),
                 errmsg("internal error in pxfutils.c:normalize_key_name. Parameter key is null or empty.")));
    }

    StringInfoData formatter;
    initStringInfo(&formatter);
    char* upperCasedKey = str_toupper(pstrdup(key), strlen(key));
    appendStringInfo(&formatter, "X-GP-%s", upperCasedKey);
    pfree(upperCasedKey);

    return formatter.data;
}

/*
 * TypeOidGetTypename
 * Get the name of the type, given the OID
 */
char*
TypeOidGetTypename(Oid typid)
{

    Assert(OidIsValid(typid));

    HeapTuple typtup;
    Form_pg_type typform;

    typtup = SearchSysCache(TYPEOID,
                            ObjectIdGetDatum(typid),
                            0, 0, 0);
    if (!HeapTupleIsValid(typtup))
        elog(ERROR, "cache lookup failed for type %u", typid);

    typform = (Form_pg_type) GETSTRUCT(typtup);

    char *typname = NameStr(typform->typname);

    StringInfoData tname;
    initStringInfo(&tname);
    appendStringInfo(&tname, "%s", typname);

    ReleaseSysCache(typtup);

    return tname.data;
}
