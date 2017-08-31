/*-------------------------------------------------------------------------
 *
 * cdb_lockbox.c
 *	  Utilities for reading and writing the gpddboost credentials file.
 *
 * Portions Copyright (c) 1996-2003, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 * Portions Copyright (c) 2016-Present Pivotal Software, Inc.
 *
 * IDENTIFICATION
 *	  src/bin/pg_dump/cdb/cdb_lockbox.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres_fe.h"

#include "cdb_dump_util.h"
#include "cdb_lockbox.h"

/* This should be at least as large as DDBOOST_PASSWORD_MAXLENGTH! */
#define OBFUSCATE_PAYLOAD_LENGTH	40

#define OBFUSCATE_MAGIC 1

/*
 * Format for obfuscated strings. lb_obfuscate returns this in base64 encoded
 * form.
 */
typedef struct
{
	/* constant to identify the format. */
	char		magic;

	/* seed used for the obfuscation */
	unsigned char seed[sizeof(uint32)];

	unsigned char payload[OBFUSCATE_PAYLOAD_LENGTH];
}	obfuscate_buf;

/*
 * A random number generator, used by lb_obfuscate(). The algorithm is taken from
 * the POSIX spec for rand().
 */
static unsigned char
lb_obfuscate_rand(uint32 *seed) /* RAND_MAX assumed to be 32767. */
{
	*seed = (*seed) * 1103515245 + 12345;
	return ((unsigned char) ((*seed) / 65536) % 32768);
}

/*
 * Obfuscate a string.
 *
 * The obfuscation is totally reversible, but makes it unreadable to a casual
 * observer. This is intended to prevent an administrator from accidentally
 * seeing a password she does not want to see, not as a way to keep the bad
 * guys out.
 */
char *
lb_obfuscate(const char *input)
{
	int			input_len;
	int			i;
	uint32		seed;
	uint32		nseed;
	obfuscate_buf ob_buf;

	input_len = strlen(input);
	if (input_len > OBFUSCATE_PAYLOAD_LENGTH)
	{
		mpp_err_msg("ERROR", "ddboost", "password is too long, the maximum is %d characters\n",
					OBFUSCATE_PAYLOAD_LENGTH);
		return NULL;
	}

	ob_buf.magic = OBFUSCATE_MAGIC;

	seed = (uint32) time(NULL);
	nseed = htonl(seed);
	memcpy(ob_buf.seed, &nseed, sizeof(uint32));

	for (i = 0; i < input_len; i++)
		ob_buf.payload[i] = ((unsigned char) input[i]) ^ lb_obfuscate_rand(&seed);
	/* pad with zeros */
	for (; i < OBFUSCATE_PAYLOAD_LENGTH; i++)
		ob_buf.payload[i] = 0 ^ lb_obfuscate_rand(&seed);

	return DataToBase64((char *) &ob_buf, sizeof(obfuscate_buf));
}

/*
 * Reverse of lb_obfuscate().
 */
char *
lb_deobfuscate(const char *input)
{
	unsigned int len;
	obfuscate_buf ob_buf;
	char	   *buf;
	uint32		nseed;
	uint32		seed;
	int			i;
	char		retbuf[OBFUSCATE_PAYLOAD_LENGTH + 1];

	buf = Base64ToData(input, &len);
	if (!buf)
		return NULL;

	if (len != sizeof(obfuscate_buf))
	{
		free(buf);
		return NULL;
	}

	memcpy(&ob_buf, buf, sizeof(obfuscate_buf));
	free(buf);

	if (ob_buf.magic != OBFUSCATE_MAGIC)
		return NULL;

	memcpy(&nseed, ob_buf.seed, sizeof(uint32));
	seed = ntohl(nseed);

	for (i = 0; i < OBFUSCATE_PAYLOAD_LENGTH; i++)
		retbuf[i] = (char) (ob_buf.payload[i] ^ lb_obfuscate_rand(&seed));
	retbuf[i] = '\0';

	return strdup(retbuf);
}

/*
 * Write configuration to given file.
 *
 * Returns 0 on success. On error, returns -1 and reports an error message with
 * mpp_err_msg().
 */
int
lb_store(const char *filepath, const lockbox_content * content)
{
	FILE	   *fp;
	int			i;

	fp = fopen(filepath, "wb");
	if (fp == NULL)
	{
		mpp_err_msg("ERROR", "ddboost", "could not open credentials file \"%s\" for writing: %s\n",
					filepath, strerror(errno));
		return -1;
	}

	/* Write a file header, as a comment. */
	fprintf(fp, "# gpddboost config file\n");

	/* Write all the key-value pairs. */
	for (i = 0; i < content->nitems; i++)
	{
		lockbox_item *item = &content->items[i];
		char	   *p;

		fprintf(fp, "%s='", item->key);
		for (p = item->value; *p != '\0'; p++)
		{
			if (*p == '\'')
				fputc('\'', fp);
			fputc(*p, fp);
		}
		fprintf(fp, "'\n");
	}

	if (ferror(fp))
	{
		mpp_err_msg("ERROR", "ddboost", "error writing credentials file \"%s\"\n",
					filepath);
		fclose(fp);
		return -1;
	}

	if (fclose(fp) != 0)
	{
		mpp_err_msg("ERROR", "ddboost", "error writing credentials file \"%s\"\n",
					strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * Reads a configuration from given file in user's home directory.
 *
 * Returns a lockbox_content object containing all the key-value pairs
 * from the file. On error, returns NULL, and reports an error message with
 * mpp_err_msg().
 */
lockbox_content *
lb_load(const char *filepath)
{
	FILE	   *fp;
	lockbox_content *content;
	char		linebuf[1024];
	char		keybuf[1024];
	char		valuebuf[1024];
	char	   *lp;
	char	   *kp;
	char	   *vp;

	content = malloc(sizeof(lockbox_content));
	if (content == NULL)
	{
		mpp_err_msg("ERROR", "ddboost", "out of memory\n");
		return NULL;
	}
	content->items = NULL;
	content->nitems = 0;

	fp = fopen(filepath, "rb");
	if (fp == NULL)
	{
		free(content);
		mpp_err_msg("ERROR", "ddboost", "could not open credentials file \"%s\" for reading: %s\n",
					filepath, strerror(errno));
		return NULL;
	}

	/* Parse the file, line by line */
	while (fgets(linebuf, sizeof(linebuf) - 1, fp) != NULL)
	{
		lockbox_item *item;

		lp = linebuf;
		while (*lp == ' ' || *lp == '\t')
			lp++;				/* ignore leading space */

		if (*lp == '\0' || *lp == '#')
			continue;			/* ignore empty lines and comments */

		kp = keybuf;
		for (;;)
		{
			if (*lp == '\0')
			{
				mpp_err_msg("ERROR", "ddboost", "missing '=' in credentials file\n");
				goto fail;
			}
			else if (*lp == '=')
				break;
			*(kp++) = *(lp++);
		}
		*kp = '\0';

		/* skip = */
		lp++;
		if (*lp != '\'')
		{
			mpp_err_msg("ERROR", "ddboost", "missing ' in credentials file\n");
			goto fail;
		}
		lp++;

		/* parse value, e.g. 'foo''bar' */
		vp = valuebuf;
		for (;;)
		{
			if (*lp == '\0' || *lp == '\n')
			{
				mpp_err_msg("ERROR", "ddboost", "unexpected end-of-line in credentials file\n");
				goto fail;
			}
			else if (lp[0] == '\'' && lp[1] == '\'')
			{
				/* escaped quote character */
				*(vp++) = '\'';
				lp += 2;
			}
			else if (lp[0] == '\'')
			{
				lp++;
				break;			/* end of value string */
			}
			else
				*(vp++) = *(lp++);
		}
		*vp = '\0';

		while (*lp == ' ' || *lp == '\t' || *lp == '\n')
			lp++;				/* ignore trailing space */

		if (*lp != '\0')
		{
			mpp_err_msg("ERROR", "ddboost", "extra characters at end of line in credentials file: %s\n");
			goto fail;
		}

		content->items = realloc(content->items, (content->nitems + 1) * sizeof(lockbox_item));
		if (content->items == NULL)
		{
			mpp_err_msg("ERROR", "ddboost", "out of memory\n");
			goto fail;
		}

		item = &content->items[content->nitems++];

		item->key = strdup(keybuf);
		item->value = strdup(valuebuf);
		if (item->key == NULL || item->value == NULL)
		{
			mpp_err_msg("ERROR", "ddboost", "out of memory\n");
			goto fail;
		}
	}

	if (ferror(fp))
	{
		mpp_err_msg("ERROR", "ddboost", "error reading credentials file \"%s\"\n",
					filepath);
		goto fail;
	}

	if (fclose(fp) != 0)
	{
		fp = NULL;
		mpp_err_msg("ERROR", "ddboost", "error closing credentials file \"%s\"\n",
					strerror(errno));
		goto fail;
	}

	return content;

fail:

	if (content->items != NULL)
	{
		int i;

		for (i = 0; i < content->nitems; i++)
		{
			lockbox_item *item = &content->items[i];

			if (item->key != NULL)
				free(item->key);
			if (item->value != NULL)
				free(item->value);
		}

		free(content->items);
	}

	free(content);
	if (fp != NULL)
		fclose(fp);
	return NULL;
}

/* Find item with given key from a lockbox_content object. */
char *
lb_get_item(lockbox_content *content, const char *key)
{
	int			i;

	for (i = 0; i < content->nitems; i++)
	{
		if (strcmp(content->items[i].key, key) == 0)
			return content->items[i].value;
	}

	return NULL;				/* not found */
}

/* Same as lb_get_item(), but reports an error if no item if found */
char *
lb_get_item_or_error(lockbox_content *content, const char *key,
					 const char *filepath)
{
	char	   *value;

	value = lb_get_item(content, key);
	if (!value)
		mpp_err_msg("ERROR", "ddboost", "'%s' not found in credentials file \"%s\"\n", key, filepath);

	return value;
}
