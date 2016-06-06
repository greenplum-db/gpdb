/*-------------------------------------------------------------------------
 *
 * gp_alloc.h
 *	  This file contains declarations for an allocator that works with vmem quota.
 *
 * Copyright (c) 2016, Pivotal Inc.
 */
#ifndef GP_ALLOC_H
#define GP_ALLOC_H

#include "nodes/nodes.h"

#ifdef USE_ASSERT_CHECKING
#define GP_ALLOC_DEBUG
#endif

#ifdef GP_ALLOC_DEBUG
#define VMEM_HEADER_CHECKSUM 0x7f7f7f7f
#define VMEM_FOOTER_CHECKSUM 0x5f5f5f5f

typedef int64 HeaderChecksumType;
typedef int64 FooterChecksumType;

#define FooterChecksumSize (sizeof(FooterChecksumType))

#else

#define FooterChecksumSize 0

#endif

/* The VmemHeader prepends user pointer in all Vmem allocations */
typedef struct VmemHeader
{
#ifdef GP_ALLOC_DEBUG
	/*
	 * Checksum to verify that we are not trying to free a memory not allocated
	 * using gp_malloc
	 */
	HeaderChecksumType checksum;
#endif
	/* The size of the usable allocation, i.e., without the header/footer overhead */
	size_t size;
	/* Payload - must be last */
	char data[1];
} VmemHeader;

#define VMEM_HEADER_SIZE offsetof(VmemHeader, data)


extern void *gp_malloc(int64 sz);
extern void *gp_realloc(void *ptr, int64 newsz);
extern void gp_free(void *ptr);

/* Gets the actual usable payload address of a vmem pointer */
#define VmemPtrToUserPtr(ptr)	\
					((void*)(&(ptr)->data))

/*
 * Converts a user pointer to a VMEM pointer by going backward
 * to get the header address
 */
#define UserPtr_GetVmemPtr(usable_pointer)	\
					((VmemHeader*)(((char *)(usable_pointer)) - VMEM_HEADER_SIZE))

/* Extracts the size of the user pointer from a Vmem pointer */
#define VmemPtr_GetUserPtrSize(ptr)	\
					((ptr)->size)

/*
 * Stores the size of the user pointer in the header for
 * later use by others such as gp_free
 */
#define VmemPtr_SetUserPtrSize(ptr, user_size)	\
					((ptr)->size = user_size)

#ifdef GP_ALLOC_DEBUG
/* Stores a checksum in the header for debugging purpose */
#define VmemPtr_SetHeaderChecksum(ptr)	\
		((ptr)->checksum = VMEM_HEADER_CHECKSUM)

/* Checks if the header checksum of a Vmem pointer matches */
#define VmemPtr_VerifyHeaderChecksum(ptr) \
		(Assert((ptr)->checksum == VMEM_HEADER_CHECKSUM))

/* Extracts the footer checksum pointer */
#define VmemPtr_GetPointerToFooterChecksum(ptr) \
		((FooterChecksumType *)(((char *)VmemPtr_GetEndAddress(ptr)) - sizeof(FooterChecksumType)))

/* Stores a checksum in the footer for debugging purpose */
#define VmemPtr_SetFooterChecksum(ptr) \
		(*VmemPtr_GetPointerToFooterChecksum(ptr) = VMEM_FOOTER_CHECKSUM)

/* Checks if the footer checksum of a Vmem pointer matches */
#define VmemPtr_VerifyFooterChecksum(ptr) \
		(Assert(*VmemPtr_GetPointerToFooterChecksum(ptr) == VMEM_FOOTER_CHECKSUM))

#else
	/* No-op */
#define VmemPtr_SetHeaderChecksum(ptr)
#define VmemPtr_SetFooterChecksum(ptr)
#endif

/* Converts a user pointer size to Vmem pointer size by adding header and footer overhead */
#define UserPtrSizeToVmemPtrSize(payload_size) \
					(VMEM_HEADER_SIZE + payload_size + FooterChecksumSize)

/* Gets the end address of a Vmem pointer */
#define VmemPtr_GetEndAddress(ptr) \
		(((char *)ptr) + UserPtrSizeToVmemPtrSize(VmemPtr_GetUserPtrSize(ptr)))

/* Extracts the size from an user pointer */
#define UserPtr_GetUserPtrSize(ptr) \
		(VmemPtr_GetUserPtrSize(UserPtr_GetVmemPtr(ptr)))

/* Extracts the Vmem size from an user pointer */
#define UserPtr_GetVmemPtrSize(ptr) \
		(UserPtrSizeToVmemPtrSize(VmemPtr_GetUserPtrSize(UserPtr_GetVmemPtr(ptr))))

/* The end address of a user pointer */
#define UserPtr_GetEndAddress(ptr) \
		(((char *)ptr) + UserPtr_GetUserPtrSize(ptr))

/* Initialize header/footer of a Vmem pointer */
#define VmemPtr_Initialize(ptr, size) \
		VmemPtr_SetUserPtrSize(ptr, size); \
		VmemPtr_SetHeaderChecksum(ptr); \
		VmemPtr_SetFooterChecksum(ptr);

#endif   /* GP_ALLOC_H */
