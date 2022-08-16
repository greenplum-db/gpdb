/**
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef INI_H
#define INI_H

#define INI_VERSION "0.1.1"

typedef struct ini_t ini_t;

ini_t*      ini_load_from_url(const char *url, const char *datadir);
ini_t*      ini_load(const char *filename);
void        ini_free(ini_t *ini);
bool        ini_section_exist(ini_t *ini, const char* section);
const char* ini_get(ini_t *ini, const char *section, const char *key);
int         ini_sget(ini_t *ini, const char *section, const char *value,
                     const char *scanfmt, void *dst);

int strcmpci(const char *a, const char *b);

#endif
