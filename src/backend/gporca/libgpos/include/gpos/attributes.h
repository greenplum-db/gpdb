#ifndef GPOS_attributes_H
#define GPOS_attributes_H

#ifdef __GNUC__
#define GPOS_UNUSED __attribute__((unused))
#else
#define GPOS_UNUSED
#endif

#ifndef GPOS_DEBUG
#define GPOS_ASSERTS_ONLY GPOS_UNUSED
#else
#define GPOS_ASSERTS_ONLY
#endif

#endif	// !GPOS_attributes_H
