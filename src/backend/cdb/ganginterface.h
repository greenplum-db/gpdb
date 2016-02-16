/*
 * Gang interface
 */

#include "cdb/cdbgang.h"

typedef struct GangMethods
{
	Gang * createGang(GangType type, int gang_id, int size, int content, char *portal_name);
} GangMethods;
