/*
 * xlogdump_statement.c
 *
 * a collection of functions to build/re-produce (fake) SQL statements
 * from xlog records.
 */
#include "xlogdump_statement.h"

#include "access/tupmacs.h"
#include "catalog/pg_type.h"
#include "storage/bufpage.h"
#include "utils/datetime.h"
#include "utils/timestamp.h"

#include "xlogdump_oid2name.h"

static int printValue(const char *, const int, const attrib_t, const uint32);

#if PG_VERSION_NUM < 80400
 #ifdef HAVE_INT64_TIMESTAMP
 typedef int64 TimeOffset;
 #else
 typedef double TimeOffset;
 #endif
#endif

#if PG_VERSION_NUM < 80300
#define MaxHeapTupleSize  (BLCKSZ - MAXALIGN(sizeof(PageHeaderData)))
#endif

static void
dump_xlrecord(const char *data, const int datlen)
{
#ifdef DEBUG
	int i;

	for (i=0 ; i<datlen ; i++)
	{
		if ( i%16==0 )
			printf("\n%4d: ", i);
		printf("%c(%02x) ", isprint(*(data+i) & 0xff) ? *(data+i) & 0xff : '?', *(data+i) & 0xff);
	}
	printf("\n");
#endif
}

static void
print_column_values(char *tupdata, int tuplen, const bits8 *nullBitMap, const xl_heap_header hhead,
		    const char *op, const char *relname)
{
	/* FIXME: 1024 as maximum number of the columns */
	attrib_t att[1024];
	int cols;
	int offset = 0;
	int i;

	cols = relname2attr_begin(relname);
		
	printf("%s: %d row(s) found in the table `%s'.\n", op, cols, relname);

	for (i=0 ; i<cols ; i++)
	{
		relname2attr_fetch(i, &att[i]);
		printf("%s: column %d, name %s, type %d, ", op, i, att[i].attname, att[i].atttypid);

		/* is the attribute value null? */
		if((hhead.t_infomask & HEAP_HASNULL) && (att_isnull(i, nullBitMap)))
		{
			printf("value null\n");
		}
		else
	        {
			printf("value ");
			offset = printValue(tupdata, offset, att[i], tuplen);
			printf("\n");

			if ( offset<0 )
				break;
		}
	}
	
	relname2attr_end();
}

/*
 * Print a insert command that contains all the data on a xl_heap_insert
 */
void
printInsert(xl_heap_insert *xlrecord, const uint32 datalen, const char *relName)
{
	char tupdata[MaxHeapTupleSize];
	char *tup;
	xl_heap_header hhead;
	bits8 nullBitMap[MaxNullBitmapLen];

	MemSet((char *) tupdata, 0, MaxHeapTupleSize * sizeof(char));
	MemSet(nullBitMap, 0, MaxNullBitmapLen);
	
	if(datalen > MaxHeapTupleSize)
		return;

	/* Copy the heap header into hhead, 
	   the the heap data into data 
	   and the tuple null bitmap into nullBitMap */
	memcpy(&hhead, (char *) xlrecord + SizeOfHeapInsert, SizeOfHeapHeader);

	/* FIXME: Why is '+1' needed for offset of the data? */
	memcpy(&tupdata, (char *) xlrecord + SizeOfHeapInsert + SizeOfHeapHeader + 1, datalen);
	tup = tupdata;

	memcpy(&nullBitMap,
	       (bits8 *) xlrecord + SizeOfHeapInsert + SizeOfHeapHeader,
	       BITMAPLEN(HeapTupleHeaderGetNatts(&hhead)) * sizeof(bits8));
	
	if (oid2name_enabled())
	{
		dump_xlrecord(tupdata, datalen);

		// Get relation field names and types
		print_column_values(tupdata, datalen, nullBitMap, hhead, "INSERT", relName);
	}
	else
	{
		fprintf(stderr, "ERROR: --statements needs --oid2name to be enabled.\n");
	}
}

/*
 * Print a update command that contains all the data on a xl_heap_update
 */
void
printUpdate(xl_heap_update *xlrecord, uint32 datalen, const char *relName)
{
	char tupdata[MaxHeapTupleSize];
	char *tup;
	xl_heap_header hhead;
	bits8 nullBitMap[MaxNullBitmapLen];

	MemSet((char *) tupdata, 0, MaxHeapTupleSize * sizeof(char));
	MemSet(nullBitMap, 0, MaxNullBitmapLen);
	
	if(datalen > MaxHeapTupleSize)
		return;

	/* Copy the heap header into hhead, 
	   the the heap data into data 
	   and the tuple null bitmap into nullBitMap */
	memcpy(&hhead, (char *) xlrecord + SizeOfHeapUpdate, SizeOfHeapHeader);

	/* FIXME: Why is '+1' needed for offset of the data? */
	memcpy(&tupdata, (char *) xlrecord + SizeOfHeapUpdate + SizeOfHeapHeader + 1, datalen);
	tup = tupdata;

	memcpy(&nullBitMap,
	       (bits8 *) xlrecord + SizeOfHeapUpdate + SizeOfHeapHeader,
	       BITMAPLEN(HeapTupleHeaderGetNatts(&hhead)) * sizeof(bits8));

	// Get relation field names and types
	if (oid2name_enabled())
	{
		dump_xlrecord(tupdata, datalen);

		// Get relation field names and types
		print_column_values(tupdata, datalen, nullBitMap, hhead, "UPDATE", relName);
	}
	else
	{
		fprintf(stderr, "ERROR: --statements needs --oid2name to be enabled.\n");
	}
}


/*
 * Print the field based on a chunk of xlog data and the field type.
 * The tuplen is just for error detection on variable length data,
 * actualy is based on the xlog record total length.
 */
static int
printValue(const char *tup, const int offset, const attrib_t att, const uint32 tuplen)
{
	char *data = (char *)tup + offset;
	int16 int16_val;
	int32 int32_val;
	int64 int64_val;
	float4 float4_val;
	float8 float8_val;
	int i;
	int new_offset = offset;

	/*
	 * Calculate new offset if padding exists.
	 *
	 * See src/backend/access/common/heaptuple.c:DataFill()
	 * for more details on how the data is packed.
	 */
	if ( att.attbyval=='t' )
	{
		new_offset = att_align_nominal(offset, att.attalign);
		data = (char *)tup + new_offset;
	}
	else if (att.attlen == -1 && !VARATT_IS_1B(data) )
	{
		/*
		 * If a varlena has a 4 byte offset for the info bits,
		 * the offset needs to be re-calculated with an alignment
		 * prior to reading the info bits itself.
		 */
		new_offset = att_align_nominal(offset, att.attalign);
		data = (char *)tup + new_offset;
	}
#ifdef DEBUG
	printf("(offset=%d, new_offset=%d) ", offset, new_offset);
#endif

	// Just print out the value of a specific data type from the data array
	switch (att.atttypid)
	{
		case INT2OID:
			memcpy(&int16_val, data, sizeof(int16));
			printf("%d", int16_val);
			new_offset += sizeof(int16);
			break;

		case INT4OID:
		case OIDOID:
		case REGPROCOID:
		case XIDOID:
			memcpy(&int32_val, data, sizeof(int32));
			printf("%d", int32_val);
			new_offset += sizeof(int32);
			break;

		case INT8OID:
			memcpy(&int64_val, data, sizeof(int64));
			printf(INT64_FORMAT, int64_val);
			new_offset += sizeof(int64);
			break;

		case FLOAT4OID:
			memcpy(&float4_val, data, sizeof(float4));
			printf("%f", float4_val);
			new_offset += sizeof(float4);
			break;

		case FLOAT8OID:
			memcpy(&float8_val, data, sizeof(float8));
			printf("%f", float8_val);
			new_offset += sizeof(float8);
			break;

		case CHAROID:
			printf("%d", *data);
			new_offset += sizeof(char);
			break;

		case VARCHAROID:
		case TEXTOID:
		case BPCHAROID: /* blank-packed char == char(X) */
		  {
			char *ptr;
			int len;

			i = 0;

			len = VARSIZE_ANY(data);
			ptr = VARDATA_ANY(data);

#ifdef DEBUG
			printf("(varatt_is_4b=%d, varatt_is_1b=%d, ",
			       VARATT_IS_4B(data),
			       VARATT_IS_1B(data));
			printf("len=%d, tuplen=%d) ", len, tuplen);
#endif

			if ( VARATT_IS_4B(data) )
			{
				i += 4;
#ifdef DEBUG
				printf("(%02x %02x %02x %02x) ", *(data), *(data+1), *(data+2), *(data+3));
#endif
			}
			else
			{
				i += 1;
#ifdef DEBUG
				printf("(%02x) ", *(data));
#endif
			}

			if (len<0 || tuplen<len)
			{
				fprintf(stderr, "ERROR: Invalid field len\n");
				new_offset += tuplen;
				break;
			}

			printf("'");
			for (; i<len ; i++)
			{
				if ( *(data+i)=='\0' )
					break;
				printf("%c", *(data+i));
			}
			printf("'");

			new_offset += len;
			break;
		  }

		case NAMEOID:
			for(i = 0; i < NAMEDATALEN && *(data+i) != '\0'; i++)
				printf("%c", *(data+i));
				
			new_offset += NAMEDATALEN;
			break;

		case BOOLOID:
			printf("%c", (*data == 0 ? 'f' : 't'));
			new_offset += sizeof(bool);
			break;

		case TIMESTAMPOID:
		  {
			int y,m,d;
			int hh,mm,ss;
			fsec_t ff;
			Timestamp date;
			Timestamp time;

			memcpy(&time, data, sizeof(Timestamp));
#ifdef __DEBUG
			printf("(ts=%f) ", time);
#endif

			TMODULO(time, date, (double) SECS_PER_DAY);
#ifdef __DEBUG
#ifdef HAVE_INT64_TIMESTAMP
			printf("(date=%lld, time=%lld) ", date, time);
#else
			printf("(date=%f, time=%f) ", date, time);
#endif
#endif

			date += POSTGRES_EPOCH_JDATE;

			j2date(date, &y, &m, &d);
			dt2time(time, &hh, &mm, &ss, &ff);

			printf("%04d-%02d-%02d ", y, m, d);
#ifdef HAVE_INT64_TIMESTAMP
			printf("%02d:%02d:%02d.%d", hh, mm, ss, ff);
#else
			printf("%02d:%02d:%02.6f", hh, mm, ss+ff);
#endif

			new_offset += sizeof(Timestamp);
			break;
		  }

		default:
			printf("(unsupported type %d)", att.atttypid);
			if ( att.attlen>0 )
				new_offset += att.attlen;
			else
				new_offset = -1;
		  	break;
			
	}

	return new_offset;
}


/*
 * src/backend/utils/adt/datetime.c
 */
void
j2date(int jd, int *year, int *month, int *day)
{
	unsigned int julian;
	unsigned int quad;
	unsigned int extra;
	int			y;

	julian = jd;
	julian += 32044;
	quad = julian / 146097;
	extra = (julian - quad * 146097) * 4 + 3;
	julian += 60 + quad * 3 + extra / 146097;
	quad = julian / 1461;
	julian -= quad * 1461;
	y = julian * 4 / 1461;
	julian = ((y != 0) ? ((julian + 305) % 365) : ((julian + 306) % 366))
	  + 123;
	y += quad * 4;
	*year = y - 4800;
	quad = julian * 2141 / 65536;
	*day = julian - 7834 * quad / 256;
	*month = (quad + 10) % 12 + 1;
	
	return;
}	/* j2date() */

/*
 * src/backend/utils/adt/timestamp.c
 */
void
dt2time(Timestamp jd, int *hour, int *min, int *sec, fsec_t *fsec)
{
	TimeOffset      time;

	time = jd;

#ifdef HAVE_INT64_TIMESTAMP
	*hour = time / USECS_PER_HOUR;
	time -= (*hour) * USECS_PER_HOUR;
	*min = time / USECS_PER_MINUTE;
	time -= (*min) * USECS_PER_MINUTE;
	*sec = time / USECS_PER_SEC;
	*fsec = time - (*sec * USECS_PER_SEC);
#else
	*hour = time / SECS_PER_HOUR;
	time -= (*hour) * SECS_PER_HOUR;
	*min = time / SECS_PER_MINUTE;
	time -= (*min) * SECS_PER_MINUTE;
	*sec = time;
	*fsec = time - *sec;
#endif
}       /* dt2time() */
