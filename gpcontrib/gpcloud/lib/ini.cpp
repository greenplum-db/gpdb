/**
 * Copyright (c) 2015 rxi
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <curl/curl.h>

#include "ini.h"

struct ini_t {
  char *data;
  char *end;
};

/* Case insensitive string compare */
int strcmpci(const char *a, const char *b) {
  for (;;) {
    int d = tolower(*a) - tolower(*b);
    if (d != 0 || !*a) {
      return d;
    }
    a++, b++;
  }
}

/* Returns the next string in the split data */
static char* next(ini_t *ini, char *p) {
  p += strlen(p);
  while (p < ini->end && *p == '\0') {
    p++;
  }
  return p;
}

static void trim_back(ini_t *ini, char *p) {
  while (p >= ini->data && (*p == ' ' || *p == '\t' || *p == '\r')) {
    *p-- = '\0';
  }
}

static char* discard_line(ini_t *ini, char *p) {
  while (p < ini->end && *p != '\n') {
    *p++ = '\0';
  }
  return p;
}

static char *unescape_quoted_value(ini_t *ini, char *p) {
  /* Use `q` as write-head and `p` as read-head, `p` is always ahead of `q`
   * as escape sequences are always larger than their resultant data */
  char *q = p;
  p++;
  while (p < ini->end && *p != '"' && *p != '\r' && *p != '\n') {
    if (*p == '\\') {
      /* Handle escaped char */
      p++;
      switch (*p) {
        default   : *q = *p;    break;
        case 'r'  : *q = '\r';  break;
        case 'n'  : *q = '\n';  break;
        case 't'  : *q = '\t';  break;
        case '\r' :
        case '\n' :
        case '\0' : goto end;
      }

    } else {
      /* Handle normal char */
      *q = *p;
    }
    q++, p++;
  }
end:
  return q;
}

/* Splits data in place into strings containing section-headers, keys and
 * values using one or more '\0' as a delimiter. Unescapes quoted values */
static void split_data(ini_t *ini) {
  char *value_start, *line_start;
  char *p = ini->data;

  while (p < ini->end) {
    switch (*p) {
      case '\r':
      case '\n':
      case '\t':
      case ' ':
        *p = '\0';
        /* Fall through */

      case '\0':
        p++;
        break;

      case '[':
        p += strcspn(p, "]\n");
        *p = '\0';
        break;

      case ';':
        p = discard_line(ini, p);
        break;

      default:
        line_start = p;
        p += strcspn(p, "=\n");

        /* Is line missing a '='? */
        if (*p != '=') {
          p = discard_line(ini, line_start);
          break;
        }
        trim_back(ini, p - 1);

        /* Replace '=' and whitespace after it with '\0' */
        do {
          *p++ = '\0';
        } while (*p == ' ' || *p == '\r' || *p == '\t');

        /* Is a value after '=' missing? */
        if (*p == '\n' || *p == '\0') {
          p = discard_line(ini, line_start);
          break;
        }

        if (*p == '"') {
          /* Handle quoted string value */
          value_start = p;
          p = unescape_quoted_value(ini, p);

          /* Was the string empty? */
          if (p == value_start) {
            p = discard_line(ini, line_start);
            break;
          }

          /* Discard the rest of the line after the string value */
          p = discard_line(ini, p);

        } else {
          /* Handle normal value */
          p += strcspn(p, "\n");
          trim_back(ini, p - 1);
        }
        break;
    }
  }
}

#define S3MAXPGPATH 1024
const static int extssl_protocol  = CURL_SSLVERSION_TLSv1;
static const char* extssl_cert = "gpfdists/client.crt";
static const char* extssl_key = "gpfdists/client.key";
static const char* extssl_ca = "gpfdists/root.crt";
#define INICURL_EASY_SETOPT(h, opt, val) \
    do { \
        int			e; \
        if ((e = curl_easy_setopt(h, opt, val)) != CURLE_OK){  \
            curl_easy_cleanup(curl);  \
            return NULL; \
        } \
    } while(0)

static CURL *create_curl_from_url(const char *url, const char *datadir) {
    char extssl_key_full[S3MAXPGPATH] = {0};
    char extssl_cer_full[S3MAXPGPATH] = {0};
    char extssl_cas_full[S3MAXPGPATH] = {0};
    CURL *curl = NULL;

    curl = curl_easy_init();
    INICURL_EASY_SETOPT(curl, CURLOPT_URL, url);
    // Add https support
    if (strncmp(url, "https", 5) == 0) {
        snprintf(extssl_cer_full, S3MAXPGPATH, "%s/%s", datadir, extssl_cert);
        /* set the cert for client authentication */
        INICURL_EASY_SETOPT(curl, CURLOPT_SSLCERT, extssl_cer_full);
        INICURL_EASY_SETOPT(curl, CURLOPT_SSLKEYTYPE,"PEM");
        snprintf(extssl_key_full, S3MAXPGPATH, "%s/%s", datadir, extssl_key);
        /* set the private key (file or ID in engine) */
        INICURL_EASY_SETOPT(curl, CURLOPT_SSLKEY, extssl_key_full);
        snprintf(extssl_cas_full, S3MAXPGPATH, "%s/%s", datadir, extssl_ca);
        /* set the file with the CA certificates, for validating the server */
        INICURL_EASY_SETOPT(curl, CURLOPT_CAINFO, extssl_cas_full);
        /* set cert verification */
        INICURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYPEER, 1);
        /* set host verification */
        INICURL_EASY_SETOPT(curl, CURLOPT_SSL_VERIFYHOST, 2);
        /* set protocol */
        INICURL_EASY_SETOPT(curl, CURLOPT_SSLVERSION, extssl_protocol);
    }
    return curl;
}
static size_t
get_s3_param(char *buffer, size_t size, size_t nitems, void *userp)
{
    ini_t *ini = (ini_t *)userp;
    int nbytes = size * nitems;
    ini->data = (char*) malloc(nbytes+1);
    if (ini->data == NULL) {
        return 0;
    }
    ini->data[nbytes] = '\0';
    ini->end = ini->data  + nbytes;
    memcpy(ini->data, buffer, nbytes);

    return nbytes;
}
ini_t* ini_load_from_url(const char *url, const char *datadir) {
    ini_t *ini = NULL;
    CURL *curl = NULL;
    curl_slist *headers = NULL;
    CURLcode e;

    curl_global_init(CURL_GLOBAL_ALL);
    /* Init ini struct */
    ini = (ini_t*) malloc(sizeof(*ini));
    if (!ini) {
        goto fail;
    }
    memset(ini, 0, sizeof(*ini));
    /* Curl param */
    curl = create_curl_from_url(url, datadir);
    if (curl == NULL) {
        goto fail;
    }
    // Add header "S3_Param_Req"
    headers = curl_slist_append(NULL, "S3_Param_Req: true");
    if (headers == NULL) {
        goto fail;
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_s3_param);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, ini);
    /* Load file content into memory, null terminate, init end var */
    e = curl_easy_perform(curl);
    if (e != CURLE_OK) {
        goto fail;
    }
    /* Prepare data */
    split_data(ini);
    /* Clean up and return */
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return ini;

fail:
    if (ini) ini_free(ini);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return NULL;
}

ini_t* ini_load(const char *filename) {
  ini_t *ini = NULL;
  FILE *fp = NULL;
  int n, sz;

  /* Init ini struct */
  ini = (ini_t*) malloc(sizeof(*ini));
  if (!ini) {
    goto fail;
  }
  memset(ini, 0, sizeof(*ini));

  /* Open file */
  fp = fopen(filename, "rb");
  if (!fp) {
    goto fail;
  }

  /* Get file size */
  fseek(fp, 0, SEEK_END);
  sz = ftell(fp);
  if(sz < 0) {
      goto fail;
  }
  rewind(fp);

  /* Load file content into memory, null terminate, init end var */
  ini->data = (char*) malloc(sz + 1);
  ini->data[sz] = '\0';
  ini->end = ini->data  + sz;
  n = fread(ini->data, 1, sz, fp);
  if (n != sz) {
    goto fail;
  }

  /* Prepare data */
  split_data(ini);

  /* Clean up and return */
  fclose(fp);
  return ini;

fail:
  if (fp) fclose(fp);
  if (ini) ini_free(ini);
  return NULL;
}

void ini_free(ini_t *ini) {
  free(ini->data);
  free(ini);
}

bool ini_section_exist(ini_t *ini, const char *section) {
  char *p = ini->data;
  while (p < ini->end) {
    if (*p == '[') {
      /* Handle section */
      char* current_section = p + 1;
      if (!section || !strcmpci(section, current_section)) {
        return true;
      }
    }
    p = next(ini, p);
  }

  return false;
}

const char* ini_get(ini_t *ini, const char *section, const char *key) {
  const char *current_section = "";
  char *val;
  char *p = ini->data;

  if (*p == '\0') {
    p = next(ini, p);
  }

  while (p < ini->end) {
    if (*p == '[') {
      /* Handle section */
      current_section = p + 1;

    } else {
      /* Handle key */
      val = next(ini, p);
      if (!section || !strcmpci(section, current_section)) {
        if (!strcmpci(p, key)) {
          return val;
        }
      }
      p = val;
    }

    p = next(ini, p);
  }

  return NULL;
}

int ini_sget(
  ini_t *ini, const char *section, const char *value,
  const char *scanfmt, void *dst
) {
  const char *val = ini_get(ini, section, value);
  if (!val) {
    return 0;
  }
  if (scanfmt) {
    sscanf(val, scanfmt, dst);
  } else {
    *((const char**) dst) = val;
  }
  return 1;
}
