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
static inline
void* VmemPtrToUserPtr(VmemHeader *ptr)
{
	return ((void*)(&(ptr)->data));
}

/*
 * Converts a user pointer to a VMEM pointer by going backward
 * to get the header address
 */
static inline VmemHeader*
UserPtr_GetVmemPtr(void *usable_pointer)
{
	return ((VmemHeader*)(((char *)(usable_pointer)) - VMEM_HEADER_SIZE));
}

/* Extracts the size of the user pointer from a Vmem pointer */
static inline size_t
VmemPtr_GetUserPtrSize(VmemHeader *ptr)
{
	return ptr->size;
}

/*
 * Stores the size of the user pointer in the header for
 * later use by others such as gp_free
 */
static inline void
VmemPtr_SetUserPtrSize(VmemHeader *ptr, size_t user_size)
{
	ptr->size = user_size;
}

/* Converts a user pointer size to Vmem pointer size by adding header and footer overhead */
static inline size_t
UserPtrSizeToVmemPtrSize(size_t payload_size)
{
	return (VMEM_HEADER_SIZE + payload_size + FooterChecksumSize);
}

/* Gets the end address of a Vmem pointer */
static void* VmemPtr_GetEndAddress(VmemHeader *ptr)
{
	return (((char *)ptr) + UserPtrSizeToVmemPtrSize(VmemPtr_GetUserPtrSize(ptr)));
}

#ifdef GP_ALLOC_DEBUG
/* Stores a checksum in the header for debugging purpose */
static inline void
VmemPtr_SetHeaderChecksum(VmemHeader *ptr)
{
	ptr->checksum = VMEM_HEADER_CHECKSUM;
}

/* Checks if the header checksum of a Vmem pointer matches */
static inline void
VmemPtr_VerifyHeaderChecksum(VmemHeader *ptr)
{
	Assert(ptr->checksum == VMEM_HEADER_CHECKSUM);
}

/* Extracts the footer checksum pointer */
static inline FooterChecksumType*
VmemPtr_GetPointerToFooterChecksum(VmemHeader *ptr)
{
	return ((FooterChecksumType *)(((char *)VmemPtr_GetEndAddress(ptr)) - sizeof(FooterChecksumType)));
}

/* Stores a checksum in the footer for debugging purpose */
static void
VmemPtr_SetFooterChecksum(VmemHeader *ptr)
{
	*VmemPtr_GetPointerToFooterChecksum(ptr) = VMEM_FOOTER_CHECKSUM;
}

/* Checks if the footer checksum of a Vmem pointer matches */
static void
VmemPtr_VerifyFooterChecksum(VmemHeader *ptr)
{
	Assert(*VmemPtr_GetPointerToFooterChecksum(ptr) == VMEM_FOOTER_CHECKSUM);
}

#endif

/* Extracts the size from an user pointer */
static size_t
UserPtr_GetUserPtrSize(void *ptr)
{
	return VmemPtr_GetUserPtrSize(UserPtr_GetVmemPtr(ptr));
}

/* Extracts the Vmem size from an user pointer */
static size_t
UserPtr_GetVmemPtrSize(void *ptr)
{
	return UserPtrSizeToVmemPtrSize(VmemPtr_GetUserPtrSize(UserPtr_GetVmemPtr(ptr)));
}

/* The end address of a user pointer */
static void*
UserPtr_GetEndAddress(void *ptr)
{
	return (((char *)ptr) + UserPtr_GetUserPtrSize(ptr));
}

/* Initialize header/footer of a Vmem pointer */
static inline void
VmemPtr_Initialize(VmemHeader *ptr, size_t size)
{
	VmemPtr_SetUserPtrSize(ptr, size);
	VmemPtr_SetHeaderChecksum(ptr);
	VmemPtr_SetFooterChecksum(ptr);
}

#endif   /* GP_ALLOC_H */
