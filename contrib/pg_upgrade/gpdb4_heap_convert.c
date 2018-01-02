/*
 *	gpdb4_heap_convert.c
 *
 *	Support for convertion heap tables from GPDB4 format.
 */

#include "pg_upgrade.h"

#include "access/xlogdefs.h"
#include "storage/bufpage.h"
#include "storage/checksum.h"

/*
 * Page format version that we convert to.
 * (14 is hardcoded here, rather than using PG_PAGE_LAYOUT_VERSION, because
 * that's the format this routine converts to at the moment. If the "current"
 * page version changes, we need a new routine to convert to that.
 */
#define TARGET_PAGE_VERSION 14

typedef struct VERSION4_XLogRecPtr
{
	uint32		xlogid;			/* log file #, 0 based */
	uint32		xrecoff;		/* byte offset of location in log file */
} VERSION4_XLogRecPtr;

typedef struct
{
	/* XXX LSN is member of *any* block, not only page-organized ones */
	VERSION4_XLogRecPtr	pd_lsn;			/* LSN: next byte after last byte of xlog
								 * record for last change to this page */
	uint16		pd_tli;			/* least significant bits of the TimeLineID
								 * containing the LSN */
	uint16		pd_flags;		/* flag bits, see below */
	LocationIndex pd_lower;		/* offset to start of free space */
	LocationIndex pd_upper;		/* offset to end of free space */
	LocationIndex pd_special;	/* offset to start of special space */
	uint16		pd_pagesize_version;
	ItemIdData	pd_linp[1];		/* beginning of line pointer array */
} VERSION4_PageHeaderData;

#define VERSION4_SizeOfPageHeaderData (offsetof(VERSION4_PageHeaderData, pd_linp[0]))

#define VERSION4_LP_UNUSED		0		/* unused (should always have lp_len=0) */
#define VERSION4_LP_USED		0x01	/* this line pointer is being used */
#define VERSION4_LP_DELETE		0x02	/* item is to be deleted */
#define VERSION4_LP_DEAD		3		/* dead, may or may not have storage */

#define VERSION4_HEAP_HASCOMPRESSED		0x0008	/* has compressed stored attribute(s) */
#define VERSION4_HEAP_HASEXTENDED		0x000C	/* the two above combined */
#define VERSION4_HEAP_HASOID			0x0010	/* has an object-id field */


/*
 * Information about the relation we're currently converting.
 */
static bool		curr_hasnumerics;
static AttInfo *curr_atts;
static int		curr_natts;

static char		overflow_buf[BLCKSZ];
static int		overflow_blkno;
static int		curr_dstfd;

static void make_room(migratorContext *ctx, char *page);
static void convert_heaptuple(migratorContext *ctx, HeapTupleHeader htup);
static void overflow_tuple(migratorContext *ctx, HeapTupleHeader htup, int len);
static void page_init(Page page);


static const char *
convert_gpdb4_heap_page(migratorContext *ctx, char *page)
{
	VERSION4_PageHeaderData *oldhdr;
	PageHeader	newhdr;
	OffsetNumber off;
	OffsetNumber maxoff;

	if (PageGetPageSize(page) != BLCKSZ)
		return "invalid block size on page";

	/* Can only convert from GPDB4 format */
	if (PageGetPageLayoutVersion(page) != 4)
		return "invalid page version";

	oldhdr = (VERSION4_PageHeaderData *) page;

	/* Other checks that PageHeaderIsValid() normally performs */
	if (!((oldhdr->pd_flags & ~PD_VALID_FLAG_BITS) == 0 &&
		  oldhdr->pd_lower >= SizeOfPageHeaderData &&
		  oldhdr->pd_lower <= oldhdr->pd_upper &&
		  oldhdr->pd_upper <= oldhdr->pd_special &&
		  oldhdr->pd_special <= BLCKSZ &&
		  oldhdr->pd_special == MAXALIGN(oldhdr->pd_special)))
		return "invalid header";

	/*
	 * Ok, it's a valid heap page, from GPDB4. We know how to convert that!
	 */

	/* 1. pd_prune_xid was added to page header */

	/* 2. Line pointer flags were changed */

	/* 3. HEAP_COMPRESSED flag was removed */

	/* 4. On-disk representation of numerics was changed */

	/*
	 * Also, money datatype was widened from 32 to 64 bits. (pg_upgrade
	 * should've refused the upgrade)
	 */

	/*
	 * First, check if there is enough space on the page, after we expand the header.
	 */
	oldhdr = (VERSION4_PageHeaderData *) page;
	newhdr = (PageHeader) page;

	/*
	 * If there isn't enough space on this page for the new header field, relocate a tuple
	 */
	make_room(ctx, page);

	/*
	 * There is space. Move the line pointers. We also convert the line pointer flags
	 * while we're at it. Begin from end to beginning, so that we don't overwrite items
	 * we haven't processed yet.
	 */
	maxoff = (oldhdr->pd_lower - VERSION4_SizeOfPageHeaderData) / sizeof(ItemIdData); /* PageGetMaxOffsetNumber */
	for (off = maxoff; off >= 1; off--)
	{
		ItemIdData iid = oldhdr->pd_linp[off - 1];	/* PageGetItemId */

		if (iid.lp_flags == VERSION4_LP_UNUSED)
			iid.lp_flags = LP_UNUSED;
		else if (iid.lp_flags == VERSION4_LP_USED)
			iid.lp_flags = LP_NORMAL;
		else
		{
			/* LP_DELETE and  LP_USED were never used on heap pages. */
			return "unexpected LP_DELETE or LP_DEAD line pointer on old-format heap page";
		}

		newhdr->pd_linp[off - 1] = iid;
	}
	newhdr->pd_lower = (char *) &newhdr->pd_linp[maxoff] - (char *) page;

	/* Initialize the field that was added after version 4 format */
	newhdr->pd_prune_xid = 0;

	/*
	 * Ok, the page header and line pointers are in the new format now. Mangle
	 * the tuples themselves
	 */
	for (off = 1; off <= maxoff; off++)
	{
		ItemId iid = PageGetItemId(page, off);	/* we can use PageGetItemId now */
		HeapTupleHeader htup;

		if (!ItemIdIsNormal(iid))
			continue;

		htup = (HeapTupleHeader) PageGetItem(page, iid);

		convert_heaptuple(ctx, htup);
	}

	/*
	 * Finally, change the version number.
	 */
	PageSetPageSizeAndVersion(page, BLCKSZ, TARGET_PAGE_VERSION);

	return NULL;
}

static void
make_room(migratorContext *ctx, char *page)
{
	VERSION4_PageHeaderData *oldhdr = (VERSION4_PageHeaderData *) page;
	int			i;
	int			victim_lp_off = -1;
	int			victim_lp_len = -1;
	int			victim_i = -1;
	int			space;
	OffsetNumber maxoff;
	HeapTupleHeader htup;

	space =  (int) oldhdr->pd_upper - (int) oldhdr->pd_lower;

	if (space >= SizeOfPageHeaderData - VERSION4_SizeOfPageHeaderData)
		return;

	maxoff = (oldhdr->pd_lower - VERSION4_SizeOfPageHeaderData) / sizeof(ItemIdData); /* PageGetMaxOffsetNumber */
	
	/* First see if we can squeeze out enough unused line pointers to make space. */
	for (i = maxoff; i >= 1; i--)
	{
		ItemIdData iid = oldhdr->pd_linp[i - 1];	/* PageGetItemId */

		if (iid.lp_flags == VERSION4_LP_UNUSED)
		{
			if (i < maxoff)
				memmove(&oldhdr->pd_linp[i - 1],
						&oldhdr->pd_linp[i],
						maxoff - i);
			oldhdr->pd_lower -= sizeof(ItemIdData);

			/* Did we make enough room? */
			space = (int) oldhdr->pd_upper - (int) oldhdr->pd_lower;
			if (space >= SizeOfPageHeaderData - VERSION4_SizeOfPageHeaderData)
				return;
		}
	}

	/*
	 * Pick a victim tuple. We choose the tuple with smallest offset within the
	 * page, so that we don't need to move the other tuple to free the space
	 */
	for (i = 1; i <= maxoff; i++)
	{
		ItemIdData iid = oldhdr->pd_linp[i - 1];	/* PageGetItemId */

		if (iid.lp_flags == VERSION4_LP_UNUSED)
		{
			/* shouldn't happen, as we just squeezed out all unused line pointers */
			pg_log(ctx, PG_FATAL, "unexpected LP_UNUSED line pointer on old-format page\n");
		}
		else if (iid.lp_flags == VERSION4_LP_USED)
		{
			if (victim_lp_off == -1 || iid.lp_off < victim_lp_off)
			{
				victim_i = i;
				victim_lp_len = iid.lp_len;
				victim_lp_off = iid.lp_off;
			}
		}
		else
		{
			/* LP_DEAD nor LP_DELETE were used on heap pages in GPDB4. */
			pg_log(ctx, PG_FATAL, "unexpected line pointer flags on old-format page: 0x%x\n", iid.lp_flags);
		}
	}

	/* Ensure that we have a victim tuple here before assuming so */
	if (victim_lp_len == -1)
		pg_log(ctx, PG_FATAL, "victim tuple for moving wasn't found\n");

	/* Cross-check */
	if (oldhdr->pd_upper != victim_lp_off)
		pg_log(ctx, PG_FATAL, "mismatch between lp_off in line pointers and pd_upper\n");

	/* Convert the victim tuple, as it's still in the old format */
	htup = (HeapTupleHeader) (((char *) page) + victim_lp_off);
	convert_heaptuple(ctx, htup);

	/* Add it to the overflow page */
	overflow_tuple(ctx, htup, victim_lp_len);

	/* Mark the line pointer as unused, and adjust pd_upper */
	oldhdr->pd_linp[victim_i - 1].lp_flags = VERSION4_LP_UNUSED;
	oldhdr->pd_upper += MAXALIGN(victim_lp_len);
}

static void
flush_overflow_page(migratorContext *ctx)
{
	if (!PageIsNew(overflow_buf))
	{
		if (ctx->checksum_mode == CHECKSUM_ADD)
			((PageHeader) overflow_buf)->pd_checksum =
				pg_checksum_page(overflow_buf, overflow_blkno);

		if (write(curr_dstfd, overflow_buf, BLCKSZ) != BLCKSZ)
			pg_log(ctx, PG_FATAL, "can't write overflow page to destination\n");
		pg_log(ctx, PG_DEBUG, "flushed overflow block\n");

		overflow_blkno++;
	}

	/* Re-initialize the overflow buffer as an empty page. */
	page_init(overflow_buf);
	overflow_blkno = 0;
}

/*  Like PageInit() in the backend */
static void
page_init(Page page)
{
	PageHeader	p = (PageHeader) page;

	/* Make sure all fields of page are zero, as well as unused space */
	MemSet(p, 0, BLCKSZ);

	/* p->pd_flags = 0;								done by above MemSet */
	p->pd_lower = SizeOfPageHeaderData;
	p->pd_upper = BLCKSZ;
	p->pd_special = BLCKSZ; /* no special area on heap pages */
	PageSetPageSizeAndVersion(page, BLCKSZ, TARGET_PAGE_VERSION);
	/* p->pd_prune_xid = InvalidTransactionId;		done by above MemSet */
}

static void
overflow_tuple(migratorContext *ctx, HeapTupleHeader htup, int size)
{
	PageHeader	phdr = (PageHeader) overflow_buf;
	Size		alignedSize;
	int			lower;
	int			upper;
	ItemId		itemId;
	OffsetNumber offsetNumber;

	/* This is a stripped down version of backend's PageAddItem */

	offsetNumber = PageGetMaxOffsetNumber(overflow_buf) + 1;

	if (offsetNumber > MaxHeapTuplesPerPage)
	{
		flush_overflow_page(ctx);
		overflow_tuple(ctx, htup, size);
		return;
	}

	lower = phdr->pd_lower + sizeof(ItemIdData);

	alignedSize = MAXALIGN(size);

	upper = (int) phdr->pd_upper - (int) alignedSize;

	if (lower > upper)
	{
		flush_overflow_page(ctx);
		overflow_tuple(ctx, htup, size);
		return;
	}
	
	/* OK to insert the item. */
	
	itemId = PageGetItemId(phdr, offsetNumber);

	/* set the item pointer */
	ItemIdSetNormal(itemId, upper, size);

	/* copy the item's data onto the page */
	memcpy((char *) overflow_buf + upper, (char *) htup, size);

	/* adjust page header */
	phdr->pd_lower = (LocationIndex) lower;
	phdr->pd_upper = (LocationIndex) upper;
}

static void
convert_heaptuple(migratorContext *ctx, HeapTupleHeader htup)
{
	/* HEAP_HASEXTENDED and HEAP_HASCOMPRESSED flags were removed. Clear them out */
	htup->t_infomask &= ~(VERSION4_HEAP_HASEXTENDED | VERSION4_HEAP_HASCOMPRESSED);

	/* HEAP_HASOID flag was moved */
	if (htup->t_infomask & VERSION4_HEAP_HASOID)
	{
		htup->t_infomask &= ~(VERSION4_HEAP_HASOID);
		htup->t_infomask |= HEAP_HASOID;
	}

	/* Any numeric columns? */
	if (curr_hasnumerics)
	{
		/* This is like heap_deform_tuple() */

		bool		hasnulls = (htup->t_infomask & HEAP_HASNULL) != 0;
		int			attnum;
		char	   *tp;
		long		off;
		bits8	   *bp = htup->t_bits;		/* ptr to null bitmap in tuple */

		tp = (char *) htup + htup->t_hoff;

		off = 0;

		for (attnum = 0; attnum < curr_natts; attnum++)
		{
			AttInfo	   *thisatt = &curr_atts[attnum];

			if (hasnulls && att_isnull(attnum, bp))
				continue;

			if (thisatt->attlen == -1)
			{
				off = att_align_pointer(off, thisatt->attalign, -1,
										tp + off);
			}
			else
			{
				/* not varlena, so safe to use att_align_nominal */
				off = att_align_nominal(off, thisatt->attalign);
			}

			if (thisatt->is_numeric)
			{
				/*
				 * Before PostgreSQL 8.3, the n_weight and n_sign_dscale fields
				 * were the other way 'round. Swap them.
				 *
				 * FIXME: need to handle toasted datums here.
				 */
				Datum		datum = PointerGetDatum(tp + off);

				if (VARATT_IS_COMPRESSED(datum))
					pg_log(ctx, PG_FATAL, "converting compressed numeric datums is not implemented\n");
				else if (VARATT_IS_EXTERNAL(datum))
					pg_log(ctx, PG_FATAL, "converting toasted numeric datums is not implemented\n");
				else
				{
					char	   *numericdata = VARDATA_ANY(DatumGetPointer(datum));
					int			sz = VARSIZE_ANY_EXHDR(DatumGetPointer(datum));
					uint16		tmp;

					if (sz < 4)
						pg_log(ctx, PG_FATAL, "unexpected size for numeric datum: %d\n", sz);

					memcpy(&tmp, &numericdata[0], 2);
					memcpy(&numericdata[0], &numericdata[2], 2);
					memcpy(&numericdata[2], &tmp, 2);
				}
			}

			off = att_addlength_pointer(off, thisatt->attlen, tp + off);
		}
	}
}

const char *
convert_gpdb4_heap_file(migratorContext *ctx,
						const char *src, const char *dst,
						bool has_numerics, AttInfo *atts, int natts)
{
	int			src_fd;
	int			dstfd;
	int			blkno;
	char		buf[BLCKSZ];
	ssize_t		bytesRead;
	const char *msg = NULL;

	curr_hasnumerics = has_numerics;
	curr_atts = atts;
	curr_natts = natts;

	page_init(overflow_buf);
	overflow_blkno = 0;

	if ((src_fd = open(src, O_RDONLY, 0)) < 0)
		return "can't open source file";

	if ((dstfd = open(dst, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) < 0)
	{
		close(src_fd);
		return "can't create destination file";
	}

	blkno = 0;
	curr_dstfd = dstfd;
	
	while ((bytesRead = read(src_fd, buf, BLCKSZ)) == BLCKSZ)
	{
		msg = convert_gpdb4_heap_page(ctx, buf);
		if (msg)
			break;

		/*
		 * GPDB 4.x doesn't support checksums so we don't need to worry about
		 * retaining an existing checksum like for upgrades from 5.x. If we're
		 * not adding them we want a zeroed out portion in the header
		 */
		if (ctx->checksum_mode == CHECKSUM_ADD)
			((PageHeader) buf)->pd_checksum = pg_checksum_page(buf, blkno);
		else
			memset(&(((PageHeader) buf)->pd_checksum), 0, sizeof(uint16));

		if (write(dstfd, buf, BLCKSZ) != BLCKSZ)
		{
			msg = "can't write new page to destination";
			break;
		}

		blkno++;
	}

	flush_overflow_page(ctx);
	
	close(src_fd);
	close(dstfd);

	if (msg)
		return msg;
	else if (bytesRead != 0)
		return "found partial page in source file";
	else
		return NULL;
}
