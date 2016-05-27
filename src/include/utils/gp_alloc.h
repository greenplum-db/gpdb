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
	/* The size of the allocation, without the header/footer overhead */
	size_t size;
} VmemHeader;

extern void *gp_malloc(int64 sz);
extern void *gp_realloc(void *ptr, int64 newsz);
extern void gp_free(void *ptr);

/* Gets the actual usable payload address of a vmem pointer */
#define VmemPtrToUserPtr(ptr)	\
					((void*)(((char *)(ptr)) + sizeof(VmemHeader)))

/*
 * Converts a user pointer to a VMEM pointer by going backward
 * to get the header address
 */
#define UserPtrToVmemPtr(usable_pointer)	\
					((void*)(((char *)(usable_pointer)) - sizeof(VmemHeader)))

/* Gets the pointer to size field in the VmemHeader */
#define VmemPtrToSizePtr(ptr) \
		((size_t *)(((char *)ptr) + offsetof(VmemHeader, size)))

/* Extracts the size of a Vmem pointer */
#define VmemPtr_GetUserPtrSize(ptr)	\
					(*VmemPtrToSizePtr(ptr))

/*
 * Stores the size of a Vmem pointer in the header for
 * later use by others such as gp_free
 */
#define VmemPtr_SetUserPtrSize(ptr, size)	\
					(*VmemPtrToSizePtr(ptr) = size)

#ifdef GP_ALLOC_DEBUG
/* Extracts the header checksum pointer */
#define VmemPtrToHeaderChecksumPtr(ptr) \
		((HeaderChecksumType *) (ptr + offsetof(VmemHeader, checksum)))

/* Stores a checksum in the header for debugging purpose */
#define VmemPtr_SetHeaderChecksum(ptr)	\
		(*VmemPtrToHeaderChecksumPtr(ptr) = VMEM_HEADER_CHECKSUM)

/* Extracts the footer checksum pointer */
#define VmemPtrToFooterChecksumPtr(ptr) \
		((FooterChecksumType *)(((char *)VmemPtr_GetEndAddress(ptr)) - sizeof(FooterChecksumType)))

/* Stores a checksum in the footer for debugging purpose */
#define VmemPtr_SetFooterChecksum(ptr) \
		(*VmemPtrToFooterChecksumPtr(ptr) = VMEM_FOOTER_CHECKSUM)

/* Checks if the header checksum of a Vmem pointer matches */
#define VmemPtr_VerifyHeaderChecksum(ptr) \
		(Assert(*VmemPtrToHeaderChecksumPtr(ptr) == VMEM_HEADER_CHECKSUM))

/* Checks if the footer checksum of a Vmem pointer matches */
#define VmemPtr_VerifyFooterChecksum(ptr) \
		(Assert(*VmemPtrToFooterChecksumPtr(ptr) == VMEM_FOOTER_CHECKSUM))

/* Checks if the header checksum of a user pointer matches */
#define UserPtr_VerifyHeaderChecksum(ptr) \
		(Assert(*VmemPtrToHeaderChecksumPtr(UserPtrToVmemPtr(ptr)) == VMEM_HEADER_CHECKSUM))

/* Checks if the footer checksum of a user pointer matches */
#define UserPtr_VerifyFooterChecksum(ptr) \
		(Assert(*VmemPtrToFooterChecksumPtr(UserPtrToVmemPtr(ptr)) == VMEM_FOOTER_CHECKSUM))

/* Checks if the header footer checksum of a user pointer matche */
#define UserPtr_VerifyChecksum(ptr) \
		UserPtr_VerifyHeaderChecksum(usable_pointer); \
		UserPtr_VerifyFooterChecksum(usable_pointer);
#else
	/* No-op */
#define VmemPtr_SetHeaderChecksum(ptr)
#define VmemPtr_SetFooterChecksum(ptr)
#endif

/* Converts a user pointer size to Vmem pointer size by adding header and footer overhead */
#define UserPtrSizeToVmemPtrSize(payload_size) \
					(sizeof(VmemHeader) + payload_size + FooterChecksumSize)

/* Gets the end address of a Vmem pointer */
#define VmemPtr_GetEndAddress(ptr) \
		(((char *)ptr) + UserPtrSizeToVmemPtrSize(VmemPtr_GetUserPtrSize(ptr)))

/* Extracts the size from an user pointer */
#define UserPtr_GetUserPtrSize(ptr) \
		(VmemPtr_GetUserPtrSize(UserPtrToVmemPtr(ptr)))

/* Extracts the Vmem size from an user pointer */
#define UserPtr_GetVmemPtrSize(ptr) \
		(UserPtrSizeToVmemPtrSize(VmemPtr_GetUserPtrSize(UserPtrToVmemPtr(ptr))))

/* The end address of a user pointer */
#define UserPtr_GetEndAddress(ptr) \
		(((char *)ptr) + UserPtr_GetUserPtrSize(ptr))

/* Initialize header/footer of a Vmem pointer */
#define VmemPtr_Initialize(ptr, size) \
		VmemPtr_SetUserPtrSize(ptr, size); \
		VmemPtr_SetHeaderChecksum(ptr); \
		VmemPtr_SetFooterChecksum(ptr);

#endif   /* GP_ALLOC_H */
