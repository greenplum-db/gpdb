#ifndef GPOS_attributes_H
#define GPOS_attributes_H

#ifdef USE_CMAKE
#define GPOS_FORMAT_ARCHETYPE printf
#else
#include "pg_config.h"
#define GPOS_FORMAT_ARCHETYPE PG_PRINTF_ATTRIBUTE
#endif

#define GPOS_ATTRIBUTE_PRINTF(f, a) __attribute__((format(GPOS_FORMAT_ARCHETYPE, f, a)))

#endif // !GPOS_attributes_H
