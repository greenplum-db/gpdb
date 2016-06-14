#ifdef WIN32
/* don't build this for now */
#else
#include "transform.h"

/*
 * debugging/dumping/error handling
 */

char*
event_type(yaml_event_t* ep) 
{
    switch (ep->type) 
	{
        case YAML_NO_EVENT:             return("YAML_NO_EVENT\n");              break;
        case YAML_STREAM_START_EVENT:   return("YAML_STREAM_START_EVENT\n");    break;
        case YAML_STREAM_END_EVENT:     return("YAML_STREAM_END_EVENT\n");      break;
        case YAML_DOCUMENT_START_EVENT: return("YAML_DOCUMENT_START_EVENT\n");  break;
        case YAML_DOCUMENT_END_EVENT:   return("YAML_DOCUMENT_END_EVENT\n");    break;
        case YAML_ALIAS_EVENT:          return("YAML_ALIAS_EVENT\n");           break;
        case YAML_SCALAR_EVENT:         return("YAML_SCALAR_EVENT\n");          break;
        case YAML_SEQUENCE_START_EVENT: return("YAML_SEQUENCE_START_EVENT\n");  break;
        case YAML_SEQUENCE_END_EVENT:   return("YAML_SEQUENCE_END_EVENT\n");    break;
        case YAML_MAPPING_START_EVENT:  return("YAML_MAPPING_START_EVENT\n");   break;
        case YAML_MAPPING_END_EVENT:    return("YAML_MAPPING_END_EVENT\n");     break;
        default:                        return("unknown event\n");              break;
    }
}

void
debug_event_type(yaml_event_t* ep) 
{
	printf("%s", event_type(ep));
}


/* forward decl for debug_keyvalue */
void 
debug_mapping(struct mapping* map, int indent);

void
debug_keyvalue(struct keyvalue* kv, int indent)
{
    printf("%*s%s: ", indent, "", kv->key);
    if (kv->type == KV_SCALAR)
        printf("%s\n", kv->scalar);
    else 
    {
        printf("\n");
        if (kv->map)
            debug_mapping(kv->map, indent+1);
    }
}

void
debug_mapping(struct mapping* map, int indent)
{
    struct keyvalue* kv;
    for (kv = map->kvlist; kv; kv=kv->nxt)
        debug_keyvalue(kv, indent);
}


/*
 * yaml structural parse errors
 */

int
unexpected_event(struct streamstate* stp) 
{
    fprintf(stderr, "%s:%zu:%zu:unexpected event: %s\n",
			stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1, 
			event_type(&stp->event));
	return 1;
}

int
error_parser_initialize_failed(struct streamstate* stp) 
{
    fprintf(stderr, "%s:0:0:yaml_parser_initialize failed\n", stp->filename);
	return 1;
}

int
error_parser_parse_failed(struct streamstate* stp) 
{
    fprintf(stderr, "%s:%zu:%zu:yaml_parser_parse failed\n",
           stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1);
	return 1;
}

int
error_invalid_stream_start(struct streamstate* stp) 
{
    fprintf(stderr, "%s:%zu:%zu:stream_start_event but stream has already been started\n",
			stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1);
	return 1;
}

int
error_stream_not_started(struct streamstate* stp, char* what) 
{
    fprintf(stderr, "%s:%zu:%zu:%s but stream has not been started\n",
			stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1, what);
	return 1;
}

int
error_invalid_document_start(struct streamstate* stp) 
{
    fprintf(stderr, "%s:%zu:%zu:document_start but already within a document\n",
			stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1);
	return 1;
}

int
error_document_not_started(struct streamstate* stp, char* what) 
{
    fprintf(stderr, "%s:%zu:%zu:%s but not within a document\n",
			stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1, what);
	return 1;
}

int
error_no_current_mapping(struct streamstate* stp, char* what) 
{
    fprintf(stderr, "%s:%zu:%zu:%s but not within a mapping\n",
			stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1, what);
	return 1;
}


/*
 * parse tree construction
 */

char* 
copy_scalar(struct streamstate* stp)
{
    unsigned char* src  = stp->event.data.scalar.value;
    size_t         len  = stp->event.data.scalar.length;    
    char*          copy = malloc(len+1);
    if (copy) 
    {
        memcpy(copy, src, len);
        copy[len] = 0;
    } else
        snprintf(stp->errbuf, sizeof(stp->errbuf), 
				 "%s:%zu:%zu:allocation failed for malloc(%zu)", 
                 stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1, len+1);
    return copy;
}

struct keyvalue*
new_keyvalue(struct streamstate* stp, struct keyvalue* nxt)
{
    struct keyvalue* kv = calloc(1, sizeof(struct keyvalue));
    if (kv)
        kv->nxt = nxt;
    else
        snprintf(stp->errbuf, sizeof(stp->errbuf), 
				 "%s:%zu:%zu:allocation failed for calloc(1, sizeof(struct keyvalue))",
                 stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1);
    return kv;


}

struct mapping*
new_mapping(struct streamstate* stp, struct mapping* nxt)
{
    struct mapping* map = calloc(1, sizeof(struct mapping));
    if (map)
        map->nxt = nxt;
    else
        snprintf(stp->errbuf, sizeof(stp->errbuf), 
				 "%s:%zu:%zu:allocation failed for calloc(1, sizeof(struct mapping))",
                 stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1);
    return map;
}

int
handle_mapping_start(struct streamstate* stp)
{
    stp->curmap = new_mapping(stp, stp->curmap);
    if (! stp->curmap)
        return 1;

    struct keyvalue* kv = stp->curkv;
    if (kv) 
    {
        /*
         * this mapping is the value for the current keyvalue
         */
        kv->type      = KV_MAPPING;
        kv->map       = stp->curmap;
        kv->valuemark = stp->event.start_mark;
        stp->curkv = NULL;
    } 
    else 
    {
        /*
         * this mapping is the top level document.
         */
        if (! stp->document)
            stp->document = stp->curmap;
    }
    return 0;
}

int
handle_mapping_end(struct streamstate* stp)
{
    stp->curmap = stp->curmap->nxt; /* pop mapping from stack */
    return 0;
}

int
handle_scalar(struct streamstate* stp)
{
    struct mapping*  map = stp->curmap;
    struct keyvalue* kv  = stp->curkv;
    if (kv) 
    {
        /*
         * this scalar is the value for current keyvalue
         */
        kv->type         = KV_SCALAR;
        kv->scalar       = copy_scalar(stp);
        if (! kv->scalar)
            return 1;
        kv->valuemark    = stp->event.start_mark;
        stp->curkv       = NULL;
    } 
    else 
    {
        /*
         * this scalar is the name of a new keyvalue pair
         */
        if (stp->event.data.scalar.length > MAX_KEYLEN) 
        {
            snprintf(stp->errbuf, sizeof(stp->errbuf), 
					 "%s:%zu:%zu:key name exceeds maximum length %d",
                     stp->filename, stp->event.start_mark.line+1, stp->event.start_mark.column+1, MAX_KEYLEN);
            return 1;
        }
        kv = stp->curkv  = map->kvlist = new_keyvalue(stp, map->kvlist);
        if (! kv)
            return 1;
        kv->key          = copy_scalar(stp);
        if (! kv->key)
            return 1;
        kv->keymark      = stp->event.start_mark;
    }
    return 0;
}


int
stage1_parse(struct streamstate* stp, FILE* file, int verbose)
{
    int rc = 0;
    int done  = 0;
    int error = 0;

    rc = yaml_parser_initialize(&stp->parser);
	if (! rc) 
		return error_parser_initialize_failed(stp);

    yaml_parser_set_input_file(&stp->parser, file);

    while (!done) 
    {
        rc = yaml_parser_parse(&stp->parser, &stp->event);
        if (!rc) 
		{
			error = error_parser_parse_failed(stp);
			break;
		}

		if (verbose)
			debug_event_type(&stp->event);

        rc = 0;
        switch (stp->event.type) 
        {
            case YAML_STREAM_START_EVENT:
				if (stp->stream_start)
					rc = error_invalid_stream_start(stp);
				else
					stp->stream_start = 1;
                break;

            case YAML_DOCUMENT_START_EVENT:
				if (stp->document_start)
					rc = error_invalid_document_start(stp);
				else
					stp->document_start = 1;
                break;

            case YAML_MAPPING_START_EVENT:
				if (! stp->stream_start)
					rc = error_stream_not_started(stp, "mapping_start_event");
				else if (! stp->document_start)
					rc = error_document_not_started(stp, "mapping_start_event");
				else
					rc = handle_mapping_start(stp);
                break;


            case YAML_MAPPING_END_EVENT:
				if (! stp->stream_start)
					rc = error_stream_not_started(stp, "mapping_end_event");
				else if (! stp->document_start)
					rc = error_document_not_started(stp, "mapping_end_event");
				else if (stp->curmap == NULL) 
					rc = error_no_current_mapping(stp, "mapping_end_event");
				else
					rc = handle_mapping_end(stp);
                break;

            case YAML_SCALAR_EVENT:
				if (! stp->stream_start)
					rc = error_stream_not_started(stp, "scalar_event");
				else if (! stp->document_start)
					rc = error_document_not_started(stp, "scalar_event");
				else if (stp->curmap == NULL) 
					rc = error_no_current_mapping(stp, "scalar_event");
				else
					rc = handle_scalar(stp);
                break;

            case YAML_DOCUMENT_END_EVENT:
				if (! stp->stream_start)
					rc = error_stream_not_started(stp, "document_end_event");
				else if (! stp->document_start)
					rc = error_document_not_started(stp, "document_end_event");
				else
					stp->document_start = 0;
                break;

            case YAML_STREAM_END_EVENT:
				if (! stp->stream_start)
					rc = error_stream_not_started(stp, "stream_end_event");
				else
				{
					stp->stream_start = 0;
					done = 1;
				}
                break;
            default:
                rc = unexpected_event(stp);
                break;
        }

        yaml_event_delete(&stp->event);
        if (rc) 
        {
            error = 1;
            break;
        }
    }

    yaml_parser_delete(&stp->parser);
    return error;
}

/*
 * debugging/dumping/error handling
 */

void
dump_transform(struct transform* tr)
{
	printf(" %s:\n", tr->kv->key);
	printf("  TYPE: %s\n", tr->type == TR_INPUT ? "input" : (tr->type == TR_OUTPUT ? "output" : "unknown")); 
	printf("  COMMAND: %s\n", tr->command);
	printf("  STDERR: %s\n", tr->errs == TR_ER_CONSOLE ? "console" : (tr->errs == TR_ER_SERVER ? "server" : "unknown")); 
	printf("  CONTENTS: %s\n", tr->content == TR_FN_DATA ? "data" : (tr->content == TR_FN_PATHS ? "paths" : "unknown"));
	if (tr->safe)
		printf("  SAFE: %s\n", tr->safe);
}

void
debug_transforms(struct transform* trlist)
{
    struct transform* tr;
    printf("TRANSFORMATIONS:\n");
    for (tr = trlist; tr; tr=tr->nxt) 
		dump_transform(tr);
}

int
format_onlyfilename(struct parsestate* psp, char* fmt)
{
    snprintf(psp->errbuf, sizeof(psp->errbuf), fmt, psp->filename);
    return 1;
}

int
format_key1(struct parsestate* psp, char* fmt, char* key, yaml_mark_t mark)
{
    snprintf(psp->errbuf, sizeof(psp->errbuf), fmt, psp->filename, mark.line+1, mark.column+1, key);
    return 1;
}

int
format_key2(struct parsestate* psp, char* fmt, char* key, yaml_mark_t mark, char* value)
{
    snprintf(psp->errbuf, sizeof(psp->errbuf), fmt, psp->filename, mark.line+1, mark.column+1, key, value);
    return 1;
}

int
error_failed_to_find_transformations(struct parsestate* psp)
{
    return format_onlyfilename(psp, "%s:0:0:failed to find TRANSFORMATIONS");
}

int
error_transformations_not_proper_yaml_map(struct parsestate* psp, struct keyvalue* kv)
{
    char* fmt = ERRFMT "key '%s' is not a proper YAML map\n";
    return format_key1(psp, fmt, kv->key, kv->keymark);
}

int
error_transformation_not_proper_yaml_map(struct parsestate* psp, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': not a proper YAML map\n";
    return format_key1(psp, fmt, kv->key, kv->keymark);
}

int
error_failed_to_find_type_for_transformation(struct parsestate* psp, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': failed to find TYPE\n";
    return format_key1(psp, fmt, kv->key, kv->keymark);
}

int
error_transformation_type_not_scalar(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': TYPE not a scalar (expected TYPE:input or TYPE:output)\n";
    return format_key1(psp, fmt, trkv->key, kv->keymark);
}

int
error_invalid_type_for_transformation(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': invalid TYPE: '%s' (expected TYPE:input or TYPE:output)\n";
    return format_key2(psp, fmt, trkv->key, kv->keymark, kv->scalar);
}

int
error_failed_to_find_command_for_transformation(struct parsestate* psp, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': failed to find COMMAND\n";
    return format_key1(psp, fmt, kv->key, kv->keymark);
}

int
error_invalid_command_for_transformation(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': invalid COMMAND (expected a simple non-empty string)\n";
    return format_key1(psp, fmt, trkv->key, kv->keymark);
}

int
error_stderr_not_scalar(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
	char* fmt = ERRFMT "transformation '%s': STDERR not a scalar (expected a simple non-empty string)\n";
	return format_key1(psp, fmt, trkv->key, kv->keymark);
}

int
error_invalid_stderr_for_transformation(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
	char* fmt = ERRFMT "transformation '%s': invalid STDERR (expected STDERR:console or STDERR:server)\n";
	return format_key1(psp, fmt, trkv->key, kv->keymark);
}

int
error_content_not_scalar(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
	char* fmt = ERRFMT "transformation '%s': CONTENT not a scalar (expected CONTENT:data or CONTENT:paths)\n";
	return format_key1(psp, fmt, trkv->key, kv->keymark);
}

int
error_invalid_content_for_transformation(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': invalid CONTENT (expected CONTENT:data or CONTENT:paths)\n";
    return format_key1(psp, fmt, trkv->key, kv->keymark);
}

int
error_safe_not_scalar(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv)
{
    char* fmt = ERRFMT "transformation '%s': SAFE not a scalar (expected a simple non-empty string)\n";
    return format_key1(psp, fmt, trkv->key, kv->keymark);
}

int
error_safe_not_valid_regex(struct parsestate* psp, struct keyvalue* trkv, struct keyvalue* kv, regex_t* r, int rc)
{
	char* fmt = ERRFMT "transformation '%s': could not compile SAFE regex: ";
	int   len = snprintf(psp->errbuf, sizeof(psp->errbuf), fmt, psp->filename, kv->keymark.line+1, kv->keymark.column+1, trkv->key);
	char* end = psp->errbuf + len;
	char* lim = psp->errbuf + sizeof(psp->errbuf) - 2;
	if (end < lim) {
		regerror(rc, r, end, lim-end);
		end += strlen(end);
		*end++ = '\n';
		*end++ = 0;
	}
	return 1;
}


/*
 * transform validation and construction
 */

struct transform*
new_transform(struct parsestate* psp, struct transform* nxt)
{
    struct transform* tr = calloc(1, sizeof(struct transform));
    if (tr)
        tr->nxt = nxt;
    else
        snprintf(psp->errbuf, sizeof(psp->errbuf), 
				 "%s:0:0:allocation failed for calloc(1, sizeof(struct transform))\n",
                 psp->filename);
    return tr;
}

struct keyvalue*
find_keyvalue(struct keyvalue* kvlist, char* name)
{
    struct keyvalue* kv;
    for (kv = kvlist; kv; kv=kv->nxt) 
    {
        if (strcmp(kv->key, name) == 0)
            return kv;
    }
    return NULL;
}

int
validate_transform(struct parsestate* psp, struct transform* tr, struct mapping* map)
{
    struct keyvalue* kv = NULL;

    kv = find_keyvalue(map->kvlist, "TYPE");
    if (! kv)
        return error_failed_to_find_type_for_transformation(psp, tr->kv);

    if (kv->type != KV_SCALAR)
        return error_transformation_type_not_scalar(psp, tr->kv, kv);

    if (0 == strcasecmp(kv->scalar, "input"))
        tr->type = TR_INPUT;
    else if (0 == strcasecmp(kv->scalar, "output"))
        tr->type = TR_OUTPUT;
    else
        return error_invalid_type_for_transformation(psp, tr->kv, kv);
    
    kv = find_keyvalue(map->kvlist, "COMMAND");
    if (! kv)
        return error_failed_to_find_command_for_transformation(psp, tr->kv);

    if (kv->type != KV_SCALAR || strlen(kv->scalar) < 1)
        return error_invalid_command_for_transformation(psp, tr->kv, kv);

    tr->command = kv->scalar;

	kv = find_keyvalue(map->kvlist, "STDERR");
	if (kv) 
	{
		if (kv->type != KV_SCALAR)
			return error_stderr_not_scalar(psp, tr->kv, kv);

		if (0 == strcasecmp(kv->scalar, "console"))
			tr->errs = TR_ER_CONSOLE;
		else if (0 == strcasecmp(kv->scalar, "server"))
			tr->errs = TR_ER_SERVER;
		else
			return error_invalid_stderr_for_transformation(psp, tr->kv, kv);
	}
	else
	{
		/* 'server' is the default when stderr is not specified */
		tr->errs = TR_ER_SERVER;
	}

    kv = find_keyvalue(map->kvlist, "CONTENT");
    if (kv) 
	{
		if (kv->type != KV_SCALAR)
			return error_content_not_scalar(psp, tr->kv, kv);

		if (0 == strcasecmp(kv->scalar, "data"))
			tr->content = TR_FN_DATA;
		else if (0 == strcasecmp(kv->scalar, "paths"))
			tr->content = TR_FN_PATHS;
		else
			return error_invalid_content_for_transformation(psp, tr->kv, kv);
	}
	else
	{
		/* 'data' is the default when filename_contents is not specified */
		tr->content = TR_FN_DATA;
	}

    kv = find_keyvalue(map->kvlist, "SAFE");
    if (kv) 
	{
		int rc;

		if (kv->type != KV_SCALAR || strlen(kv->scalar) < 1)
			return error_safe_not_scalar(psp, tr->kv, kv);

		tr->safe = kv->scalar;

		rc = regcomp( &(tr->saferegex), tr->safe, REG_EXTENDED|REG_NOSUB);
		if (rc != 0)
			return error_safe_not_valid_regex(psp, tr->kv, kv, &(tr->saferegex), rc);
	}

    return 0;
}


int
validate(struct parsestate* psp, struct streamstate* stp)
{
    int rc;
    struct mapping*  document  = stp->document;
    struct mapping*  trmapping = NULL;
    struct keyvalue* kv;

    kv = find_keyvalue(document->kvlist, "TRANSFORMATIONS");
    if (! kv)
        return error_failed_to_find_transformations(psp);
    if (kv->type != KV_MAPPING)
        return error_transformations_not_proper_yaml_map(psp, kv);

    trmapping = kv->map;
    for (kv = trmapping->kvlist; kv; kv=kv->nxt) 
    {
        if (kv->type != KV_MAPPING)
            return error_transformation_not_proper_yaml_map(psp, kv);
    
        struct transform* tr = psp->trlist = new_transform(psp, psp->trlist);
        tr->kv = kv;
        rc = validate_transform(psp, tr, kv->map);
        if (rc)
            return 1;
    }
    return 0;
}


/*
 * configure transforms
 */

int
transform_config(const char* filename, struct transform** trlistp, int verbose)
{
    struct parsestate ps;
    memset(&ps, 0, sizeof(ps));
    ps.filename = filename;

    FILE* file = fopen(ps.filename, "rb");
	if (! file) 
	{
		fprintf(stderr, "unable to open file %s: %s\n", ps.filename, strerror(errno));
		return 1;
	}

    struct streamstate st;
    memset(&st, 0, sizeof(st));
    st.filename = ps.filename;

    int rc = stage1_parse(&st, file, verbose);
    fclose(file);

    if (rc) 
		fprintf(stderr, "failed to parse file: %s\n", ps.filename);

	if (rc || verbose) {
        if (st.document)
            debug_mapping(st.document, 0);
	}

	if (rc)
        return 1;

	if (validate(&ps, &st))
    {
		fprintf(stderr, "failed to validate file: %s\n", ps.filename);
        fprintf(stderr, "%s", ps.errbuf);
        return 1;
    }

    (*trlistp) = ps.trlist;
    return 0;
}


/*
 * lookup transformation
 */

struct transform*
transform_lookup(struct transform* trlist, const char* name, int for_write, int verbose)
{
	struct transform* tr;

	for (tr = trlist; tr; tr = tr->nxt)
    {

        /* ignore transforms not of proper type */
        if (for_write) 
        {
            if (tr->type != TR_OUTPUT)
                continue;
        } 
        else 
        {
            if (tr->type != TR_INPUT)
                continue;
        }

        /* when we've found a match, return the corresponding command */
        if (0 == strcmp(tr->kv->key, name)) 
		{
			if (verbose)
				dump_transform(tr);

            return tr;
		}
    }
    return NULL;
}


/*
 * transformation accessors
 */

char*
transform_command(struct transform* tr)
{
	return tr->command;
}

int
transform_stderr_server(struct transform* tr)
{
	return (tr->errs == TR_ER_SERVER);
}

int
transform_content_paths(struct transform* tr)
{
	return (tr->content == TR_FN_PATHS);
}

char*
transform_safe(struct transform* tr)
{
	return tr->safe;
}

regex_t*
transform_saferegex(struct transform* tr)
{
	return &(tr->saferegex);
}

#endif
