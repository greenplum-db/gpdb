#include <stdint.h>

#include "postgres.h"

#include "fmgr.h" // for PG_MODULE_MAGIC

#include "cdb/cdbappendonlyam.h" // for MAX_APPENDONLY_BLOCK_SIZE
#include "access/aomd.h"         // for ao hooks
#include "storage/smgr.h"        // for heap hooks

PG_MODULE_MAGIC;

static file_read_buffer_modify_hook_type     pre_file_read_hook     = NULL;
static file_write_buffer_modify_hook_type    pre_file_write_hook    = NULL;
static file_extend_buffer_modify_hook_type   pre_file_extend_hook   = NULL;
static ao_file_read_buffer_modify_hook_type  pre_ao_file_read_hook  = NULL;
static ao_file_write_buffer_modify_hook_type pre_ao_file_write_hook = NULL;

// process local buffer
static char file_write_hook_buffer[BLCKSZ]                       = {'\0'};
static char ao_file_write_hook_buffer[MAX_APPENDONLY_BLOCK_SIZE] = {'\0'};

static void
file_read_hook_invert(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer)
{
	for (int i = 0; i < BLCKSZ; i++)
	{
		buffer[i] = ~((uint8_t)buffer[i]);
	}

	// run file read hook after we decrypt the buffer, read hook need a decrypted data
	if (pre_file_read_hook)
	{
		pre_file_read_hook(reln, forknum, blocknum, buffer);
	}
}

static char *
file_extend_hook_invert(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer)
{
	if (pre_file_extend_hook)
	{
		buffer = pre_file_extend_hook(reln, forknum, blocknum, buffer);
	}

	// extend with NULL means only alloc disk
	if (buffer == NULL)
	{
		return buffer;
	}

	for (int i = 0; i < BLCKSZ; i++)
	{
		file_write_hook_buffer[i] = ~((uint8_t)buffer[i]);
	}

	return file_write_hook_buffer;
}

static char *
file_write_hook_invert(SMgrRelation reln, ForkNumber forknum, BlockNumber blocknum, char *buffer)
{
	if (pre_file_write_hook)
	{
		buffer = pre_file_write_hook(reln, forknum, blocknum, buffer);
	}

	for (int i = 0; i < BLCKSZ; i++)
	{
		file_write_hook_buffer[i] = ~((uint8_t)buffer[i]);
	}

	return file_write_hook_buffer;
}

static void
ao_file_read_hook_invert(File file, char *buffer, int actualLen, off_t offset)
{
	for (int i = 0; i < actualLen; i++)
	{
		buffer[i] = ~((uint8_t)buffer[i]);
	}

	if (pre_ao_file_read_hook)
	{
		pre_ao_file_read_hook(file, buffer, actualLen, offset);
	}
}

static char *
ao_file_write_hook_invert(File file, char *buffer, int amount, off_t offset)
{
	if (pre_ao_file_write_hook)
	{
		buffer = pre_ao_file_write_hook(file, buffer, amount, offset);
	}

	for (int i = 0; i < amount; i++)
	{
		ao_file_write_hook_buffer[i] = ~((uint8_t)buffer[i]);
	}

	return ao_file_write_hook_buffer;
}

void _PG_init(void);
void
_PG_init(void)
{
	pre_file_read_hook = file_read_buffer_modify_hook;
	file_read_buffer_modify_hook = file_read_hook_invert;

	pre_file_write_hook = file_write_buffer_modify_hook;
	file_write_buffer_modify_hook = file_write_hook_invert;

	pre_file_extend_hook = file_extend_buffer_modify_hook;
	file_extend_buffer_modify_hook = file_extend_hook_invert;

	pre_ao_file_read_hook = ao_file_read_buffer_modify_hook;
	ao_file_read_buffer_modify_hook = ao_file_read_hook_invert;

	pre_ao_file_write_hook = ao_file_write_buffer_modify_hook;
	ao_file_write_buffer_modify_hook = ao_file_write_hook_invert;
}
