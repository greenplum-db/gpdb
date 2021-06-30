#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "postgres.h"
#include "common/unicode_norm.h"
#include "utils/memutils.h"

const char *progname;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size){
	if(size<5) {
		return 1;
	}

	// Null-terminate input test case
	char *new_str = (char *)malloc(size+1);
	if (new_str == NULL){
		return 0;
	}
	memcpy(new_str, data, size);
	new_str[size] = '\0';

	MemoryContextInit();
	pg_wchar   *pg_data, *result;
	int			data_len;

	pg_data = (pg_wchar *) palloc(((int)size + 2) * sizeof(pg_wchar));
	data_len = pg_mb2wchar(new_str, pg_data);
	result = unicode_normalize_kc(pg_data);
	
	free(new_str);
	pfree(pg_data);
	pfree(result);
	return 0;
}
