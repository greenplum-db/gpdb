/*
 * tuplestorenew.c
 *		A better tuplestore
 *
 *		Design overview:
 *		Each tuple store has many pages. Each page has a page header
 *		(NTupleStorePageHeader), followed by a fixed area of binary raw data
 *		(char data[NTS_MAX_ENTRY_SIZE]), followed by a directory of tuple slots
 *		(NTupleStorePageSlotEntry). Each tuple slot simply saves a index where
 *		the slot's data starts in the byte array and a size to indicate how many
 *		bytes to read from the index.
 *
 *		All the pages are linked together using each page header's
 *		(NTupleStorePageHeader) prev_1 and next_1 pointers. This pointers are
 *		only valid for in memory pages.
 *
 *		Each page's slot table grows from end to beginning (using the same byte
 *		array) and the data grows from beginning to end. Therefore, all the slot
 *		indexes are negative.
 */

#include "postgres.h"
#include "access/heapam.h"
#include "executor/instrument.h"
#include "executor/execWorkfile.h"
#include "utils/tuplestorenew.h"
#include "utils/memutils.h"

#include "cdb/cdbvars.h"                /* currentSliceId */


typedef struct NTupleStorePageHeader
{
	long blockn;        /* block number */
	long page_flag;     /* flag */ 
	void *prev_1;    	/* in mem list */	
	void *next_1;		/* in mem list */
	int pin_cnt;		/* in mem pin count */
	int data_bcnt;		/* tail of payload data */
	int slot_cnt;		/* number of slot writen in the page */
	int first_slot;     /* first valid slot */
} NTupleStorePageHeader;

typedef struct NTupleStorePageSlotEntry
{
	short data_start; 	/* data starts */
	short size;			/* size of the entry */
} NTupleStorePageSlotEntry;

#define NTS_MAX_ENTRY_SIZE (BLCKSZ - sizeof(NTupleStorePageHeader) - sizeof(NTupleStorePageSlotEntry))

/* Page.  data grown incr, slot grow desc */
typedef struct NTupleStorePage
{
	NTupleStorePageHeader header;
	char data[NTS_MAX_ENTRY_SIZE]; 
	NTupleStorePageSlotEntry slot[1];
} NTupleStorePage;

typedef struct NTupleStoreLobRef
{
	int64 start;
	Size size;
} NTupleStoreLobRef;

/* some convinient macro/inline functions */
/* page flag bits */
#define NTS_PAGE_DIRTY 1
static inline bool nts_page_is_dirty(NTupleStorePage * page)
{
	return (page->header.page_flag & NTS_PAGE_DIRTY) != 0;
}
static inline void nts_page_set_dirty(NTupleStorePage *page, bool dirty) 
{
	if(dirty)
		(page)->header.page_flag |= NTS_PAGE_DIRTY;
	else
		(page)->header.page_flag &= (~NTS_PAGE_DIRTY);
}

#define NTS_ALLIGN8(n) (((n)+7) & (~7)) 

/* page stuff.  Note slot grow desc, so the minus array index stuff. */
static inline long nts_page_blockn(NTupleStorePage *page) { return page->header.blockn; }
static inline void nts_page_set_blockn(NTupleStorePage *page, long blockn) { page->header.blockn = blockn; }
static inline NTupleStorePage *nts_page_prev(NTupleStorePage *page) { return (NTupleStorePage *) page->header.prev_1; }
static inline void nts_page_set_prev(NTupleStorePage *page, NTupleStorePage* prev) { page->header.prev_1 = (void *) prev; }
static inline NTupleStorePage *nts_page_next(NTupleStorePage *page) { return (NTupleStorePage *) page->header.next_1; }
static inline void nts_page_set_next(NTupleStorePage *page, NTupleStorePage *next) { page->header.next_1 = (void *) next; }
static inline int nts_page_pin_cnt(NTupleStorePage *page) { return page->header.pin_cnt; }
static inline void nts_page_set_pin_cnt(NTupleStorePage *page, int pc) { page->header.pin_cnt = pc; }
static inline void nts_page_incr_pin_cnt(NTupleStorePage *page) { ++page->header.pin_cnt; }
static inline void nts_page_decr_pin_cnt(NTupleStorePage *page) { --page->header.pin_cnt; }
static inline int nts_page_slot_cnt(NTupleStorePage *page) { return page->header.slot_cnt; }
static inline void nts_page_set_slot_cnt(NTupleStorePage *page, int sc) { page->header.slot_cnt = sc; }
static inline void nts_page_incr_slot_cnt(NTupleStorePage *page) { ++page->header.slot_cnt; }
static inline int nts_page_data_bcnt(NTupleStorePage *page) { return page->header.data_bcnt; }
static inline void nts_page_set_data_bcnt(NTupleStorePage *page, int bc) { page->header.data_bcnt = bc; }
static inline int nts_page_first_valid_slotn(NTupleStorePage *page) { return page->header.first_slot; }
static inline void nts_page_set_first_valid_slotn(NTupleStorePage *page, int fs) { page->header.first_slot = fs; }
static inline int nts_page_valid_slot_cnt(NTupleStorePage *page)
{
	return nts_page_slot_cnt(page) - nts_page_first_valid_slotn(page);
}
static inline NTupleStorePageSlotEntry *nts_page_slot_entry(NTupleStorePage *page, int slotn)
{
	return &(page->slot[-slotn]);
}
static inline char *nts_page_slot_data_ptr(NTupleStorePage *page, int slotn)
{
	return page->data + nts_page_slot_entry(page, slotn)->data_start; 
}
static inline int nts_page_slot_data_len(NTupleStorePage *page, int slotn)
{
	return nts_page_slot_entry(page, slotn)->size;
}

static inline NTupleStorePageSlotEntry *nts_page_next_slot_entry(NTupleStorePage *page)
{
	return nts_page_slot_entry(page, nts_page_slot_cnt(page));
}
static inline char *nts_page_next_slot_data_ptr(NTupleStorePage *page)
{
	return page->data + NTS_ALLIGN8(nts_page_data_bcnt(page));
}
static inline int nts_page_avail_bytes(NTupleStorePage *page)
{
	int b = (char *) nts_page_slot_entry(page, nts_page_slot_cnt(page)) - 
		(char *) nts_page_next_slot_data_ptr(page);
	if(b < 0)
		return 0;
	return b;
}

static inline void update_page_slot_entry(NTupleStorePage *page, int len)
{
	short data_start = (short) NTS_ALLIGN8( nts_page_data_bcnt(page) );
	int w_len = len > 0 ? len : -len;

	NTupleStorePageSlotEntry *slot = nts_page_next_slot_entry(page);

	Assert(len < (int) NTS_MAX_ENTRY_SIZE);
	Assert(len > 0 || len == -(int)sizeof(NTupleStoreLobRef)); 

	slot->data_start = data_start;
	slot->size = (short) len;

	nts_page_incr_slot_cnt(page);
	nts_page_set_data_bcnt(page, data_start+w_len);
	nts_page_set_dirty(page, true);
}

/* tuple store type */
#define NTS_NOT_READERWRITER 1
#define NTS_IS_WRITER 2
#define NTS_IS_READER 3

struct NTupleStore
{
	int page_max;   /* max page the store can use */
	int page_effective_max; 
	int page_cnt; 	/* page count */
	int pin_cnt;    /* pin count */
	NTupleStorePage *first_page; 		/* first page */
	NTupleStorePage *last_page;     	/* last page */
	NTupleStorePage *first_free_page; 	/* free page, cached to save palloc */

	long first_ondisk_blockn;           /* first blockn that is written to disk */
	int rwflag;  /* if I am ordinary store, or a reader, or a writer of readerwriter (share input) */


	bool workfiles_created; /* set if the operator created workfiles */
	workfile_set *work_set; /* workfile set to use when using workfile manager */

	ExecWorkFile *pfile; 	/* underlying backed file */
	ExecWorkFile *plobfile;  /* underlying backed file for lobs (entries does not fit one page) */
	int64     lobbytes;  /* number of bytes written to lob file */

	List *accessors;    /* all current accessors of the store */
	bool fwacc; 		/* if I had already has a write acc */

	MemoryContext mcxt; /* memory context holding this tuplestore, and the page structs */

	/* instrumentation for explain analyze */
	Instrumentation *instrument;
};

bool ntuplestore_is_readerwriter_reader(NTupleStore *nts) { return nts->rwflag == NTS_IS_READER; }

/* Accessor to the tuplestore.
 * 
 * About the pos of an Accessor. 
 * An accessor has a pos, pos.blockn == -1 implies the position is not valid.
 * Else, pos.blockn should be the same as blockn of the current page.
 * If the pos.blockn is valid, then pos.slotn should also be a valid slotn in the 
 * page, or
 * 		1. it may equals (first_valid - 1), which indicates it is positioned right before
 *			first valid slot,
 *		2. it may equals page->header.slotn, which indicates it is positioned just after
 *			the last slot,
 * the two positions are handy when we advance the accessor.
 */
struct NTupleStoreAccessor
{
	NTupleStore *store;
	NTupleStorePage *page;
	bool isWriter;
	char *tmp_lob;
	int tmp_len;

	NTupleStorePos  pos;
};

static void ntuplestore_init_reader(NTupleStore *store, int maxBytes);
static void ntuplestore_create_spill_files(NTupleStore *nts);

void ntuplestore_setinstrument(NTupleStore *st, struct Instrumentation *instr)
{
	st->instrument = instr;
}

static inline void init_page(NTupleStorePage *page)
{
	nts_page_set_blockn(page, -1);
	nts_page_set_prev(page, NULL);
	nts_page_set_next(page, NULL);
	nts_page_set_dirty(page, true);
	nts_page_set_pin_cnt(page, 0);
	nts_page_set_data_bcnt(page, 0);
	nts_page_set_slot_cnt(page, 0);
	nts_page_set_first_valid_slotn(page, 0);
}

static inline void nts_pin_page(NTupleStore* nts, NTupleStorePage *page)
{
	Assert(nts && page);
	nts_page_incr_pin_cnt(page);
	++nts->pin_cnt;
}

static inline void nts_unpin_page(NTupleStore *nts, NTupleStorePage *page)
{
	Assert(nts && nts->pin_cnt > 0);
	Assert(page && nts_page_pin_cnt(page) > 0); 
	
	--nts->pin_cnt;
	nts_page_decr_pin_cnt(page);
}

/* Prepend to list head.  Pass in pointer of prev, and next so that the function
 * can be reused later if the one page can be on server lists 
 */
static NTupleStorePage *nts_prepend_to_dlist(
		NTupleStorePage *head, 
		NTupleStorePage *page, 
		void **prev, void **next)
{
	void **head_prev;

	*prev = NULL;

	if(!head)
	{
		(*next) = NULL;
		return page;
	}

	(*next) = head;
	head_prev = (void **) ((char *) head + ((char *) prev - (char *) page));
	*head_prev = page;

	return page;
}
#define NTS_PREPEND_1(head, page) (nts_prepend_to_dlist((head), (page), &((page)->header.prev_1), &((page)->header.next_1)))

/* Append page after curr */
static void nts_append_to_dlist(
		NTupleStorePage *curr,
		NTupleStorePage *page,
		void **prev, void **next)
{
	*prev = curr;

	if(!curr)
		*next = NULL;
	else
	{
		void **curr_next = (void **) ((char *) curr + ((char *) next - (char *) page));
		NTupleStorePage *nextpage = (NTupleStorePage *) (*curr_next);

		*curr_next = page;
		*next = nextpage;

		if(nextpage)
		{
			void **next_prev = (void **) ((char *) nextpage + ((char *) prev - (char *) page));
			Assert( *next_prev == curr );
			*next_prev = page;
		}
	}
}
#define NTS_APPEND_1(curr, page) (nts_append_to_dlist((curr), (page), &((page)->header.prev_1), &((page)->header.next_1)))

static void nts_remove_page_from_dlist(NTupleStorePage *page, void **prev, void **next)
{
	void ** prev_next = (void **) ((char *) (*prev) + ((char *) next - (char *) page));
	void ** next_prev = (void **) ((char *) (*next) + ((char *) prev - (char *) page)); 

	if(*prev)
		*prev_next = *next;
	if(*next)
		*next_prev = *prev;
}
#define NTS_REMOVE_1(page) (nts_remove_page_from_dlist((page), &((page)->header.prev_1), &((page)->header.next_1)))

/* read a page by blockn.  After read the page in not pined and not in any list */
static bool ntsReadBlock(NTupleStore *ts, int blockn, NTupleStorePage *page)
{
	long diskblockn = blockn - ts->first_ondisk_blockn;

	if(!ts->pfile)
		return false;
	
	Assert(ts->first_ondisk_blockn >= 0);
	Assert(ts && diskblockn >= 0 && page);
	if(ExecWorkFile_Seek(ts->pfile, diskblockn * BLCKSZ, SEEK_SET) != 0 ||
			ExecWorkFile_Read(ts->pfile, page, BLCKSZ) != BLCKSZ)
	{
		return false;
	}

	Assert(nts_page_blockn(page) == blockn); 
	Assert(!nts_page_is_dirty(page)); 

	nts_page_set_pin_cnt(page, 0);
	nts_page_set_prev(page, NULL);
	nts_page_set_next(page, NULL);

	return true;
}

/* write a page */
static bool ntsWriteBlock(NTupleStore *ts, NTupleStorePage *page)
{
	Assert(ts->rwflag != NTS_IS_READER);
	Assert(nts_page_blockn(page) >= 0); 

	if(ts->first_ondisk_blockn == -1)
	{
		AssertImply(ts->rwflag == NTS_IS_WRITER, nts_page_blockn(page) == 0); 
		ts->first_ondisk_blockn = nts_page_blockn(page);
	}

	Assert(nts_page_blockn(page) >= ts->first_ondisk_blockn);
	Assert(nts_page_is_dirty(page));

	nts_page_set_dirty(page, false);

	if(ExecWorkFile_Seek(ts->pfile, (nts_page_blockn(page) - ts->first_ondisk_blockn) * BLCKSZ, SEEK_SET) != 0 ||
			!ExecWorkFile_Write(ts->pfile, page, BLCKSZ))
	{
		return false;
	}

	
	return true;
}

/* Put a page onto the free list.  Do not increase nts->page_cnt */
static void nts_return_free_page(NTupleStore *nts, NTupleStorePage *page)
{
	nts->first_free_page = NTS_PREPEND_1(nts->first_free_page, page);
}

/* Get a free page.  Will shrink if necessary.
 * The page returned still belong to the store (accounted by nts->page_cnt), but it is
 * not on any list.  Caller is responsible to put it back onto a list.
 */
static NTupleStorePage *nts_get_free_page(NTupleStore *nts)
{
	NTupleStorePage *page = NULL;
	NTupleStorePage *page_next = NULL;
	int page_max = nts->page_max;

	/* if have free page, just use it */
	if(nts->first_free_page)
	{
		page = nts->first_free_page;
		nts->first_free_page = nts_page_next(page); 

		init_page(page);
		return page;
	}

	/* use 1/4 of max allowed if we are working with a disk file */
	if(nts->pfile)
		page_max >>= 2;

	if(nts->page_cnt >= page_max)
	{
		if(!nts->pfile)
		{
			if (nts->work_set != NULL)
			{
				/* We have a usable workfile_set. Use that to generate temp files */
				ntuplestore_create_spill_files(nts);
			}
			else
			{
				char tmpprefix[MAXPGPATH];
				snprintf(tmpprefix, MAXPGPATH, "%s/slice%d_ntuplestore", PG_TEMP_FILES_DIR, currentSliceId);
				nts->pfile = ExecWorkFile_CreateUnique(tmpprefix, BUFFILE, true /* delOnClose */, 0 /* compressType */ );
			}

			nts->workfiles_created = true;
			
			if (nts->instrument)
			{
				nts->instrument->workfileCreated = true;
			}
		}
		
		page = nts->first_page;

		while(page) 
		{
			page_next = nts_page_next(page); 

			if(page_next && nts_page_pin_cnt(page_next) == 0)
			{
				NTS_REMOVE_1(page_next);

				if(nts_page_is_dirty(page_next))
				{
					Assert(nts->rwflag != NTS_IS_READER);
					if(!ntsWriteBlock(nts, page_next))
					{
						workfile_mgr_report_error();
					}
				}

				if(nts->page_cnt >= page_max)
				{
					pfree(page_next);
					--nts->page_cnt;
				}
				else
					break;
			}
			else
				page = page_next;
		}

		if(page_next)
		{
			init_page(page_next);
			return page_next;
		}

		/* failed to shrink */
		if(nts->page_cnt >= page_max)
			return NULL;
	}

	Assert(page_next == NULL && nts->page_cnt < page_max);
	page = (NTupleStorePage *) MemoryContextAlloc(nts->mcxt, sizeof(NTupleStorePage));
	init_page(page);
	++nts->page_cnt;

	if(nts->instrument && nts->instrument->need_cdb)
	{
		nts->instrument->workmemused = Max(nts->instrument->workmemused, nts->page_cnt * BLCKSZ); 
		if(nts->last_page)
		{
			long pagewanted = nts_page_blockn(nts->last_page) - nts_page_blockn(nts->first_page) + 1;
			Assert(pagewanted >= 1);

			nts->instrument->workmemwanted = Max(nts->instrument->workmemwanted, pagewanted * BLCKSZ);
		}
	}
	return page;
}

/* Find a page in the store with blockn, return NULL if not found.
 * It will load the page from disk if necessary.  The page returned 
 * is not pin-ed.
 */
static NTupleStorePage *nts_load_page(NTupleStore* store, int blockn)
{
	NTupleStorePage *page = NULL;
	NTupleStorePage *page_next = NULL;
	bool readOK;

	/* fast track.  Try last page */
	if(store->last_page && nts_page_blockn(store->last_page) == blockn)
		return store->last_page;

	/* try the in mem list */
	page = store->first_page;
	while(page)
	{
		if(nts_page_blockn(page) == blockn)
			return page;

		page_next = nts_page_next(page); 
		if(page_next == NULL || nts_page_blockn(page_next) > blockn) 
			break;
		
		page = page_next;
	}
	
	Assert(page);

	/* not found.  Need to load from disk */
	page_next = nts_get_free_page(store);
	if(!page_next)
		return NULL;

	readOK = ntsReadBlock(store, blockn, page_next);
	if(!readOK)
	{
		nts_return_free_page(store, page_next);
		return NULL;
	}

	NTS_APPEND_1(page, page_next);
	return page_next;
}

/* Find the next page.  This function is likely to be faster than 
 * 	nts_load_page(nts_page_blockn(page) + 1);
 */
static NTupleStorePage *nts_load_next_page(NTupleStore* store, NTupleStorePage *page)
{
	int blockn = nts_page_blockn(page) + 1;
	bool fOK; 
	NTupleStorePage *next = nts_page_next(page);

	if(!next && store->rwflag != NTS_IS_READER)
		return NULL;

	if(!next || nts_page_blockn(next) > blockn)
	{
		next = nts_get_free_page(store);
		if (next == NULL)
			ereport(ERROR,
					(errcode(ERRCODE_OUT_OF_MEMORY),
					 errmsg("Can not allocate a new page in the tuplestore.")));
		fOK = ntsReadBlock(store, blockn, next);
		if(!fOK)
		{
			Assert(store->rwflag == NTS_IS_READER);
			nts_return_free_page(store, next);
			return NULL;
		}

		NTS_APPEND_1(page, next);
	}

	Assert(next && nts_page_blockn(next) == blockn);
	return next;
}

/* Find prev page.  Likely to be faster than nts_load_page(nts_page_blockn(page) - 1) */
static NTupleStorePage *nts_load_prev_page(NTupleStore *store, NTupleStorePage *page)
{
	int blockn = nts_page_blockn(page) - 1;
	bool fOK;
	NTupleStorePage *prev = NULL;

	if(blockn < nts_page_blockn(store->first_page))
		return NULL;

	prev = nts_page_prev(page);
	Assert(prev && nts_page_blockn(prev) <= blockn);

	if(nts_page_blockn(prev) == blockn)
		return prev;
	else
	{
		NTupleStorePage *p = nts_get_free_page(store);
		if(!p)
			return NULL;

		fOK = ntsReadBlock(store, blockn, p);
		Assert(fOK);

		NTS_APPEND_1(prev, p);

		return p;
	}
}

void
ntuplestore_destroy(NTupleStore *ts)
{
	NTupleStorePage *p = ts->first_page;
	ListCell *cell;

	/* For each accessor, we mark it has no owning store. */
	foreach(cell, ts->accessors)
	{
		NTupleStoreAccessor *acc = (NTupleStoreAccessor *) lfirst(cell);
		acc->store = NULL;
		acc->page = NULL;
	}

	while(p)
	{
		NTupleStorePage *pnext = nts_page_next(p); 
		pfree(p);
		p = pnext;
	}

	p = ts->first_free_page;
	while(p)
	{
		NTupleStorePage *pnext = nts_page_next(p); 
		pfree(p);
		p = pnext;
	}

	if(ts->pfile)
	{
		workfile_mgr_close_file(ts->work_set, ts->pfile);
		ts->pfile = NULL;
	}
	if(ts->plobfile)
	{
		workfile_mgr_close_file(ts->work_set, ts->plobfile);
		ts->plobfile = NULL;
	}

	if (ts->work_set != NULL)
	{
		workfile_mgr_close_set(ts->work_set);
		ts->work_set = NULL;
	}

	pfree(ts);
}

NTupleStore *
ntuplestore_create(int64 maxBytes)
{
	NTupleStore *store = (NTupleStore *) palloc(sizeof(NTupleStore));
	store->mcxt = CurrentMemoryContext;

	store->pfile = NULL;
	store->first_ondisk_blockn = 0;

	store->plobfile = NULL;
	store->lobbytes = 0;

	store->work_set = NULL;
	store->workfiles_created = false;

	Assert(maxBytes >= 0);
	store->page_max = maxBytes / BLCKSZ;
	/* give me at least 16 pages */
	if(store->page_max < 16)
		store->page_max = 16;

	store->first_page = (NTupleStorePage *) MemoryContextAlloc(store->mcxt, sizeof(NTupleStorePage));
	init_page(store->first_page);
	nts_page_set_blockn(store->first_page, 0);

	store->page_cnt = 1;
	store->pin_cnt = 0;

	store->last_page = store->first_page;
	nts_pin_page(store, store->first_page);
	nts_pin_page(store, store->last_page);

	store->first_free_page = NULL;

	store->rwflag = NTS_NOT_READERWRITER;
	store->accessors = NULL;
	store->fwacc = false;

	store->instrument = NULL;

	return store;
}

/*
 * Initialize a ntuplestore that is shared across slice through a ShareInputScan node
 *
 *   filename must be a unique name that identifies the share.
 *   filename does not include the pgsql_tmp/ prefix
 */
NTupleStore *
ntuplestore_create_readerwriter(const char *filename, int64 maxBytes, bool isWriter)
{
	NTupleStore* store = NULL;
	char filenameprefix[MAXPGPATH];
	char filenamelob[MAXPGPATH];

	snprintf(filenameprefix, sizeof(filenameprefix), "%s/%s", PG_TEMP_FILES_DIR, filename);

	snprintf(filenamelob, sizeof(filenamelob), "%s_LOB", filenameprefix);

	if(isWriter)
	{
		store = ntuplestore_create(maxBytes);
		store->pfile = ExecWorkFile_Create(filenameprefix, BUFFILE,
				true /*delOnClose */, 0 /* compressType */);
		store->rwflag = NTS_IS_WRITER;

		store->plobfile = ExecWorkFile_Create(filenamelob, BUFFILE,
				true /* delOnClose */, 0 /* compressType */ );
		store->lobbytes = 0;
	}
	else
	{
		store = (NTupleStore *) palloc(sizeof(NTupleStore));
		store->mcxt = CurrentMemoryContext;
		store->work_set = NULL;
		store->workfiles_created = false;

		store->pfile = ExecWorkFile_Open(filenameprefix, BUFFILE,
				false /* delOnClose */,
				0 /* compressType */);

		store->plobfile = ExecWorkFile_Open(filenamelob, BUFFILE,
				false /* delOnClose */,
				0 /* compressType */);

		ntuplestore_init_reader(store, maxBytes);
	}
	return store;
}

/*
 * Initializes a ntuplestore based on existing files.
 *
 * spill_filename and spill_lob_filename are required to have pgsql_tmp/ part of the name
 */
static void
ntuplestore_init_reader(NTupleStore *store, int maxBytes)
{
	Assert(NULL != store);
	Assert(NULL != store->pfile);
	Assert(NULL != store->plobfile);
	
	store->first_ondisk_blockn = 0;
	store->rwflag = NTS_IS_READER;
	store->lobbytes = 0;

	Assert(maxBytes >= 0);
	store->page_max = maxBytes / BLCKSZ;
	/* give me at least 16 pages */
	if(store->page_max < 16)
		store->page_max = 16;

	store->first_page = (NTupleStorePage *) MemoryContextAlloc(store->mcxt, sizeof(NTupleStorePage));
	init_page(store->first_page);

	bool fOK = ntsReadBlock(store, 0, store->first_page);
	if(!fOK)
	{
		/* empty file.  We set blockn anyway */
		nts_page_set_blockn(store->first_page, 0);
	}

	store->page_cnt = 1;
	store->pin_cnt = 0;

	nts_pin_page(store, store->first_page);

	store->last_page = NULL;
	store->first_free_page = NULL;

	store->accessors = NULL;
	store->fwacc = false;

	store->instrument = NULL;

}

/*
 * Create tuple store using the workfile manager to create spill files if needed.
 * The workSet needs to be initialized by the caller.
 */
NTupleStore *
ntuplestore_create_workset(workfile_set *workSet, int64 maxBytes)
{

	elog(gp_workfile_caching_loglevel, "Creating tuplestore with workset in directory %s", workSet->path);

	NTupleStore *store = ntuplestore_create(maxBytes);
	store->work_set = workSet;
	/* Creating new workset */
	store->rwflag = NTS_IS_WRITER;

	return store;
}

void 
ntuplestore_reset(NTupleStore *ts)
{
	NTupleStorePage *p = ts->first_page;

	Assert(list_length(ts->accessors) == 0); 
	Assert(!ts->fwacc);
	Assert(ts->rwflag == NTS_NOT_READERWRITER || !"Reset NYI for reader writer");

	while(p)
	{
		NTupleStorePage *next = nts_page_next(p); 
		ts->first_free_page = NTS_PREPEND_1(ts->first_free_page, p);
		p = next;
	}
	ts->pin_cnt = 0;

	Assert(ts->first_free_page != NULL);

	ts->first_page = ts->first_free_page;
	ts->first_free_page = nts_page_next(ts->first_page);
	init_page(ts->first_page);
	nts_page_set_blockn(ts->first_page, 0);

	ts->last_page = ts->first_page;
	nts_pin_page(ts, ts->first_page);
	nts_pin_page(ts, ts->last_page);

	ts->first_ondisk_blockn = 0;

	if(ts->plobfile)
	{
#ifdef USE_ASSERT_CHECKING
		int errorcode = 
#endif /* USE_ASSERT_CHECKING */
		    ExecWorkFile_Seek(ts->plobfile, 0 /* offset */, SEEK_SET);
		Assert(errorcode == 0);
	}

	ts->lobbytes = 0;
}

void 
ntuplestore_flush(NTupleStore *ts)
{
	NTupleStorePage *p = ts->first_page;

	Assert(ts->rwflag != NTS_IS_READER || !"Flush attempted for Reader");
	Assert(ts->pfile);

	while(p)
	{
		if(nts_page_is_dirty(p) && nts_page_slot_cnt(p) > 0)
		{
			if (!ntsWriteBlock(ts, p))
			{
				workfile_mgr_report_error();
			}
		}
		p = nts_page_next(p);
	}
	
	ExecWorkFile_Flush(ts->pfile);
	if (ts->plobfile != NULL)
	{
		ExecWorkFile_Flush(ts->plobfile);
	}
}

NTupleStoreAccessor* 
ntuplestore_create_accessor(NTupleStore *ts, bool isWriter)
{
	NTupleStoreAccessor *acc;
	MemoryContext oldcxt;

	/*
	 * The accessor is allocated in the same memory context as the tuplestore
	 * itself.
	 */
	oldcxt = MemoryContextSwitchTo(ts->mcxt);

	acc = (NTupleStoreAccessor *) palloc(sizeof(NTupleStoreAccessor));

	acc->store = ts;
	acc->isWriter = isWriter;
	acc->tmp_lob = NULL;
	acc->tmp_len = 0;
	acc->page = NULL;
	acc->pos.blockn = -1;
	acc->pos.slotn = -1;

	ntuplestore_acc_seek_first(acc);

	ts->accessors = lappend(ts->accessors, acc);

	AssertImply(isWriter, ts->rwflag != NTS_IS_READER);
	AssertImply(isWriter, !ts->fwacc);
	if(isWriter)
		ts->fwacc = true;

	MemoryContextSwitchTo(oldcxt);

	return acc;
}
	
void ntuplestore_destroy_accessor(NTupleStoreAccessor *acc)
{
	if(!acc->store)
	{
		Assert(!acc->page);
		pfree(acc);
	}
	else
	{
		if(acc->isWriter)
		{
			Assert(acc->store->fwacc);
			acc->store->fwacc = false;
		}

		if(acc->page)
			nts_unpin_page(acc->store, acc->page);

		acc->store->accessors = list_delete_ptr(acc->store->accessors, acc);
		if (acc->tmp_lob)
			pfree(acc->tmp_lob);
		
		pfree(acc);

	}
}

static long ntuplestore_put_lob(NTupleStore *nts, char* data, NTupleStoreLobRef *lobref)
{

	if(!nts->plobfile)
	{
		if (nts->work_set != NULL)
		{
			/* We have a usable workfile_set. Use that to generate temp files */
			ntuplestore_create_spill_files(nts);
		}
		else
		{
			char tmpprefix[MAXPGPATH];
			Assert(nts->rwflag == NTS_NOT_READERWRITER);
			Assert(nts->work_set == NULL);
			Assert(nts->lobbytes == 0);

			snprintf(tmpprefix, sizeof(tmpprefix), "%s/slice%d_ntuplestorelob", PG_TEMP_FILES_DIR, currentSliceId);
			nts->plobfile = ExecWorkFile_CreateUnique(tmpprefix, BUFFILE,
					true /*delOnClose */, 0 /* compressType */);
		}
	}

	lobref->start = nts->lobbytes;
#if USE_ASSERT_CHECKING
	bool res =
#endif
			ExecWorkFile_Write(nts->plobfile, data, lobref->size);

	Assert(res);
	nts->lobbytes += lobref->size;

	return lobref->size;
}

static long ntuplestore_get_lob(NTupleStore *nts, void *data, NTupleStoreLobRef *lobref)
{
	Assert(lobref->start >= 0);
	long ret = ExecWorkFile_Seek(nts->plobfile, lobref->start, SEEK_SET);
	Assert(ret == 0);

	ret = ExecWorkFile_Read(nts->plobfile, data, lobref->size);
	Assert(ret == lobref->size);

	return ret;
}

void ntuplestore_acc_put_tupleslot(NTupleStoreAccessor *tsa, TupleTableSlot *slot)
{
	char *dest_ptr;
	uint32  dest_len;
	MemTuple mtup;

	Assert(tsa->isWriter);
	Assert(tsa->store && tsa->store->first_page && tsa->store->last_page);

	/* first move the page to last page */
	if(!tsa->page || tsa->page != tsa->store->last_page)
	{
		if(tsa->page) 
			nts_unpin_page(tsa->store, tsa->page);
		tsa->page = tsa->store->last_page;
		nts_pin_page(tsa->store, tsa->page);
	}

	dest_ptr = nts_page_next_slot_data_ptr(tsa->page); 
	dest_len = (uint32) nts_page_avail_bytes(tsa->page); 

	Assert(dest_len <= NTS_MAX_ENTRY_SIZE);
	mtup = ExecCopySlotMemTupleTo(slot, NULL, dest_ptr, &dest_len);

	if(mtup != NULL) 
		update_page_slot_entry(tsa->page, (int) dest_len);
	else if(dest_len <= NTS_MAX_ENTRY_SIZE)
	{
		NTupleStorePage *page = nts_get_free_page(tsa->store);
		if(!page)
			elog(ERROR, "NTuplestore out of page");

		Assert(nts_page_slot_cnt(page) == 0 && nts_page_data_bcnt(page) == 0); 

		dest_ptr = nts_page_next_slot_data_ptr(page);
		dest_len = (unsigned int) nts_page_avail_bytes(page);
		mtup = ExecCopySlotMemTupleTo(slot, NULL, dest_ptr, &dest_len);
		Assert(mtup!=NULL);

		update_page_slot_entry(page, (int) dest_len);
		nts_page_set_blockn(page, nts_page_blockn(tsa->page) + 1); 
		NTS_APPEND_1(tsa->page, page);

		nts_unpin_page(tsa->store, tsa->page);
		nts_pin_page(tsa->store, page);
		tsa->page = page;

		nts_unpin_page(tsa->store, tsa->store->last_page);
		tsa->store->last_page = page;
		nts_pin_page(tsa->store, tsa->store->last_page);
	}
	else
	{
		char* lob = palloc(dest_len);
		NTupleStoreLobRef lobref;

		lobref.size = dest_len;

		mtup = ExecCopySlotMemTupleTo(slot, NULL, lob, &dest_len);
		Assert(mtup != NULL && dest_len == lobref.size);

		ntuplestore_put_lob(tsa->store, lob, &lobref);
		ntuplestore_acc_put_data(tsa, &lobref, -(int)sizeof(NTupleStoreLobRef)); 
		pfree(lob);
        return;
	}

	tsa->pos.blockn = nts_page_blockn(tsa->page); 
	tsa->pos.slotn = nts_page_slot_cnt(tsa->page) - 1; 
}

void ntuplestore_acc_put_data(NTupleStoreAccessor *tsa, void *data, int len)
{
	char *dest_ptr;
	int   dest_len;
	int   w_len = len > 0 ? len : -len;

	Assert(tsa->isWriter);
	Assert(tsa->store && tsa->store->first_page && tsa->store->last_page);
	Assert(len > 0 || len == (-(int)sizeof(NTupleStoreLobRef)));

	if (len > (int) NTS_MAX_ENTRY_SIZE)
	{
		NTupleStoreLobRef lobref;

		lobref.size = len;

		ntuplestore_put_lob(tsa->store, data, &lobref);
		ntuplestore_acc_put_data(tsa, &lobref, -(int)sizeof(NTupleStoreLobRef)); 
        return;
	}

	Assert(len <= (int) NTS_MAX_ENTRY_SIZE);

	/* first move the page to last page */
	if(!tsa->page || tsa->page != tsa->store->last_page)
	{
		if (tsa->page)
			nts_unpin_page(tsa->store, tsa->page);
		tsa->page = tsa->store->last_page;
		nts_pin_page(tsa->store, tsa->page);
	}

	dest_ptr = nts_page_next_slot_data_ptr(tsa->page); 
	dest_len = nts_page_avail_bytes(tsa->page); 

	if(dest_len >= w_len)
	{
		memcpy(dest_ptr, data, w_len);
		update_page_slot_entry(tsa->page, len);
	}
	else
	{
		NTupleStorePage *page = nts_get_free_page(tsa->store);
		if(!page)
			elog(ERROR, "NTuplestore out of page");

		Assert(nts_page_slot_cnt(page) == 0 && nts_page_data_bcnt(page) == 0); 

		dest_ptr = nts_page_next_slot_data_ptr(page); 
		memcpy(dest_ptr, data, w_len);

		update_page_slot_entry(page, len);
		nts_page_set_blockn(page, nts_page_blockn(tsa->page) + 1);
		NTS_APPEND_1(tsa->page, page);

		nts_unpin_page(tsa->store, tsa->page);
		nts_pin_page(tsa->store, page);
		tsa->page = page;

		nts_unpin_page(tsa->store, tsa->store->last_page);
		tsa->store->last_page = page;
		nts_pin_page(tsa->store, tsa->store->last_page);
	}

	tsa->pos.blockn = nts_page_blockn(tsa->page);
	tsa->pos.slotn = nts_page_slot_cnt(tsa->page) - 1; 
}

/* XXX Not done yet.
 * Postgres does not allow hole in the BufFile, therefore, even if we are trimming
 * a page, if, we are already in disk mode, we will have to flush the page (or write
 * any junk, but we need to write a page).  A better way is to may logical page blockn
 * to a physical blockn.  
 */
void ntuplestore_trim(NTupleStore *ts, NTupleStorePos *pos)
{
	NTupleStorePage *page = nts_load_page(ts, pos->blockn); 

	Assert(page); 
	nts_page_set_first_valid_slotn(page, pos->slotn);

	nts_unpin_page(ts, ts->first_page);

	while(ts->first_page != page)
	{
		NTupleStorePage *next = nts_page_next(ts->first_page);

		Assert(nts_page_pin_cnt(ts->first_page) == 0);

		/* Flush dirty page anyway, to prevent holes in file */
		if(nts_page_is_dirty(ts->first_page))
		{
			if(ts->pfile)
			{
				if (!ntsWriteBlock(ts, ts->first_page))
				{
					workfile_mgr_report_error();
				}
			}

		}

		ts->first_free_page = NTS_PREPEND_1(ts->first_free_page, ts->first_page); 
		ts->first_page = next;
	}

	Assert(ts->first_page);
	nts_pin_page(ts, ts->first_page);
}

static void ntuplestore_acc_advance_in_page(NTupleStoreAccessor *tsa, int* pn)
{
	if(*pn == 0)
		return ;

	if(*pn > 0) /* seeking forward */
	{
		Assert(tsa->pos.slotn + 1 >= nts_page_first_valid_slotn(tsa->page)); 

		if((tsa->pos.slotn + *pn) < nts_page_slot_cnt(tsa->page))
		{
			tsa->pos.slotn += *pn;
			*pn = 0;
		}
		else
			*pn -= (nts_page_slot_cnt(tsa->page) - tsa->pos.slotn) - 1;

	}
	else /* seeking backward */
	{
		if(tsa->pos.slotn + *pn >= nts_page_first_valid_slotn(tsa->page))
		{
			tsa->pos.slotn += *pn;
			*pn = 0;
		}
		else
			*pn += (tsa->pos.slotn - nts_page_first_valid_slotn(tsa->page));
	}
}

bool ntuplestore_acc_advance(NTupleStoreAccessor *tsa, int n)
{
	if(!tsa->page)
		return false;

	ntuplestore_acc_advance_in_page(tsa, &n);

	while(n != 0)
	{
		if(n > 0) /* seek forward */
		{
			NTupleStorePage *oldpage = tsa->page;
			tsa->page = nts_load_next_page(tsa->store, tsa->page);
			nts_unpin_page(tsa->store, oldpage);

			if(!tsa->page)
			{
				tsa->pos.blockn = -1;
				tsa->pos.slotn = -1;
				return false;
			}

			Assert(nts_page_blockn(tsa->page) == tsa->pos.blockn + 1);
			Assert(nts_page_valid_slot_cnt(tsa->page) > 0);

			nts_pin_page(tsa->store, tsa->page);
			tsa->pos.blockn = nts_page_blockn(tsa->page);
			tsa->pos.slotn = -1;
		}
		else /* seeking backward */
		{
			NTupleStorePage *oldpage = tsa->page;
			tsa->page = nts_load_prev_page(tsa->store, tsa->page);
			nts_unpin_page(tsa->store, oldpage);

			if(!tsa->page)
			{
				tsa->pos.blockn = -1;
				tsa->pos.slotn = -1;
				return false;
			}

			Assert(nts_page_blockn(tsa->page) == tsa->pos.blockn - 1);
			Assert(nts_page_valid_slot_cnt(tsa->page) > 0);

			nts_pin_page(tsa->store, tsa->page);
			tsa->pos.blockn = nts_page_blockn(tsa->page);
			tsa->pos.slotn = nts_page_slot_cnt(tsa->page);
		}

		ntuplestore_acc_advance_in_page(tsa, &n);
	}

	return true;
}

static bool ntuplestore_acc_current_data_internal(NTupleStoreAccessor *tsa, void **data, int *len)
{
	if(!tsa)
		return false;

	if(!tsa->page || tsa->pos.slotn == -1)
		return false;

	*data = (void *) nts_page_slot_data_ptr(tsa->page, tsa->pos.slotn); 
	*len = nts_page_slot_data_len(tsa->page, tsa->pos.slotn); 

	return true;
}

bool ntuplestore_acc_current_tupleslot(NTupleStoreAccessor *tsa, TupleTableSlot *slot)
{
	MemTuple tuple = NULL;
	int len = 0;
	bool fOK = ntuplestore_acc_current_data_internal(tsa, (void **) &tuple, &len);

	if(!fOK)
	{
		ExecClearTuple(slot);
		return false;
	}

	if(len < 0)
	{
		NTupleStoreLobRef *plobref = (NTupleStoreLobRef *) tuple;
		Assert(len == -(int)sizeof(NTupleStoreLobRef));
		
		tuple = (MemTuple) palloc(plobref->size);
		len = ntuplestore_get_lob(tsa->store, tuple, plobref);

		Assert(len == plobref->size);

		ExecStoreMinimalTuple(tuple, slot, true);
		return true;
	}

	ExecStoreMinimalTuple(tuple, slot, false);
	return true;
}

bool ntuplestore_acc_current_data(NTupleStoreAccessor *tsa, void **data, int *len)
{
	bool fOK = ntuplestore_acc_current_data_internal(tsa, (void **) data, len);

	if(!fOK)
	{
		return false;
	}

	if(*len < 0)
	{
		NTupleStoreLobRef *plobref = (NTupleStoreLobRef *) (*data);
		Assert(*len == -(int)sizeof(NTupleStoreLobRef));

		if (tsa->tmp_len < plobref->size)
		{
			if (tsa->tmp_lob)
				pfree(tsa->tmp_lob);
			tsa->tmp_lob = MemoryContextAlloc(tsa->store->mcxt, plobref->size);
			tsa->tmp_len = plobref->size;
		}

		*data = tsa->tmp_lob;
		*len = ntuplestore_get_lob(tsa->store, *data, plobref);

		Assert(*len == plobref->size);

		return true;
	}

	return true;
}

bool ntuplestore_acc_tell(NTupleStoreAccessor *tsa, NTupleStorePos *pos)
{
	AssertImply(tsa->pos.blockn==-1, tsa->pos.slotn==-1);

	if (pos != NULL)
	{
		pos->blockn = -1;
		pos->slotn = -1;
	}
	
	if(!tsa->page || tsa->pos.slotn == -1)
		return false;
	
	Assert(tsa->pos.blockn == nts_page_blockn(tsa->page));
	
	if(tsa->pos.slotn >= nts_page_slot_cnt(tsa->page)
	   || tsa->pos.slotn < nts_page_first_valid_slotn(tsa->page))
		return false;
	
	if(pos)
	{
		pos->blockn = tsa->pos.blockn;
		pos->slotn = tsa->pos.slotn;
	}

	return true;
}

bool ntuplestore_acc_seek(NTupleStoreAccessor *tsa, NTupleStorePos *pos)
{
	Assert(tsa && pos);
	if(pos->slotn < 0)
	{
		Assert(pos->slotn == -1);
		return false;
	}

	/* first seek page */
	if(!tsa->page || nts_page_blockn(tsa->page) != pos->blockn)
	{
		if(tsa->page)
			nts_unpin_page(tsa->store, tsa->page);

		tsa->page = nts_load_page(tsa->store, pos->blockn);
		if(!tsa->page)
			return false;

		nts_pin_page(tsa->store, tsa->page);
	}

	Assert(tsa->page && nts_page_blockn(tsa->page) == pos->blockn);
	Assert(nts_page_slot_cnt(tsa->page) > pos->slotn && nts_page_first_valid_slotn(tsa->page) <= pos->slotn);

	tsa->pos.blockn = pos->blockn;
	tsa->pos.slotn = pos->slotn;

	return true;
}

bool ntuplestore_acc_seek_first(NTupleStoreAccessor *tsa)
{
	ntuplestore_acc_seek_bof(tsa);
	return ntuplestore_acc_advance(tsa, 1);
}

bool ntuplestore_acc_seek_last(NTupleStoreAccessor *tsa)
{
	ntuplestore_acc_seek_eof(tsa);
	return ntuplestore_acc_advance(tsa, -1);
}

void  ntuplestore_acc_set_invalid(NTupleStoreAccessor *tsa)
{
	Assert(tsa);
	
	if(tsa->page)
		nts_unpin_page(tsa->store, tsa->page);

	tsa->page = NULL;
	tsa->pos.blockn = -1;
	tsa->pos.slotn = -1;
}

void ntuplestore_acc_seek_bof(NTupleStoreAccessor *tsa)
{
	Assert(tsa && tsa->store && tsa->store->first_page);

	if(tsa->page)
		nts_unpin_page(tsa->store, tsa->page);

	tsa->page = tsa->store->first_page;
	nts_pin_page(tsa->store, tsa->page);

	tsa->pos.blockn = nts_page_blockn(tsa->page);
	tsa->pos.slotn = nts_page_first_valid_slotn(tsa->page) - 1;
}

void ntuplestore_acc_seek_eof(NTupleStoreAccessor *tsa)
{
	Assert(tsa && tsa->store && tsa->store->last_page);

	if(tsa->page)
		nts_unpin_page(tsa->store, tsa->page);

	tsa->page = tsa->store->last_page;
	nts_pin_page(tsa->store, tsa->page);

	tsa->pos.blockn = nts_page_blockn(tsa->page);
	tsa->pos.slotn = nts_page_slot_cnt(tsa->page);
}

/*
 * Check if the tuple pointed by tsa1 is before the tuple pointed by tsa2.
 *
 * This function assumes that both tsa1 and tsa2 are pointing to a valid position.
 */
bool ntuplestore_acc_is_before(NTupleStoreAccessor *tsa1, NTupleStoreAccessor *tsa2)
{
	Assert(ntuplestore_acc_tell(tsa1, NULL) &&
		   ntuplestore_acc_tell(tsa2, NULL));
	if (tsa1->pos.blockn < tsa2->pos.blockn)
		return true;

	if (tsa1->pos.blockn > tsa2->pos.blockn)
		return false;
	
	if (tsa1->pos.slotn < tsa2->pos.slotn)
		return true;
	else
		return false;
}

/*
 * Use the associated workfile set to create spill files for this tuplestore.
 */
static void
ntuplestore_create_spill_files(NTupleStore *nts)
{
	Assert(nts->work_set != NULL);

	MemoryContext   oldcxt;
	oldcxt = MemoryContextSwitchTo(TopMemoryContext);

	nts->pfile = workfile_mgr_create_fileno(nts->work_set, WORKFILE_NUM_TUPLESTORE_DATA);
	nts->plobfile = workfile_mgr_create_fileno(nts->work_set, WORKFILE_NUM_TUPLESTORE_LOB);

	MemoryContextSwitchTo(oldcxt);
}

/* EOF */
