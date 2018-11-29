#ifndef MAPRED_H
#define MAPRED_H

#include <postgres_fe.h>

#ifndef YAML_H
#include <yaml.h>
#endif

#ifndef LIBPQ_FE_H
#include <libpq-fe.h>
#endif

#ifndef true
#define true  1
#endif
#ifndef false
#define false 0
#endif
#ifndef boolean
#define boolean int
#endif

typedef enum {
	ERROR_NONE = 0,          /* No error */
	ERROR_WARNING,           /* Warning message */
	ERROR_PARSE_FAILURE,     /* YAML parse failure */
	ERROR_UNEXPECTED_EVENT,  /* YAML parse failure */
	ERROR_UNEXPECTED_EOS,    /* YAML parse failure */
	ERROR_OBJECT_MISMATCH,   /* User .yml document problem */
	ERROR_UNKNOWN_KEY,       /* User .yml document problem */
	ERROR_BAD_VALUE,         /* User .yml document problem */
	ERROR_MEMORY,            /* Memory allocation failure */
	ERROR_IMPOSSIBLE,        /* Internal assertion failure */
} mapred_error_t;


typedef struct mapred_object_ mapred_object_t;
typedef struct mapred_clist_  mapred_clist_t;
typedef struct mapred_plist_  mapred_plist_t;
typedef struct mapred_olist_  mapred_olist_t;

/* Various Enumerations */
typedef enum {
	MAPRED_NO_KIND      = 0,
	MAPRED_DOCUMENT     = 1,
	MAPRED_INPUT        = 2,
	MAPRED_OUTPUT       = 3,
	MAPRED_MAPPER       = 4,
	MAPRED_TRANSITION   = 5,
	MAPRED_COMBINER     = 6,
	MAPRED_FINALIZER    = 7,
	MAPRED_REDUCER      = 8,
	MAPRED_TASK         = 9,
	MAPRED_EXECUTION    = 10,
	MAPRED_ADT          = 11        /* internal use only */
} mapred_kind_t;
#define MAPRED_MAXKIND    11

extern const char *mapred_kind_name[];
extern const char *default_parameter_names[MAPRED_MAXKIND+1][2];
extern const char *default_return_names[MAPRED_MAXKIND+1][2];

typedef enum {
	MAPRED_MODE_NONE = 0,
	MAPRED_MODE_SINGLE,
	MAPRED_MODE_MULTI,
	MAPRED_MODE_ACCUMULATED,        /* not implemented */
	MAPRED_MODE_WINDOWED,           /* not implemented */
	MAPRED_MODE_INVALID
} mapred_mode_t;

typedef enum {
	MAPRED_INPUT_NONE = 0,
	MAPRED_INPUT_FILE,
	MAPRED_INPUT_GPFDIST,
	MAPRED_INPUT_TABLE,
	MAPRED_INPUT_QUERY,
	MAPRED_INPUT_EXEC,
	MAPRED_INPUT_INVALID
} mapred_input_kind_t;

typedef enum {
	MAPRED_OUTPUT_NONE = 0,
	MAPRED_OUTPUT_FILE,
	MAPRED_OUTPUT_TABLE,
	MAPRED_OUTPUT_INVALID
} mapred_output_kind_t;

typedef enum {
	MAPRED_FORMAT_NONE = 0,
	MAPRED_FORMAT_TEXT,
	MAPRED_FORMAT_CSV,
	MAPRED_FORMAT_INVALID
} mapred_format_t;

typedef enum {
	MAPRED_OUTPUT_MODE_NONE = 0,
	MAPRED_OUTPUT_MODE_REPLACE,
	MAPRED_OUTPUT_MODE_APPEND,
	MAPRED_OUTPUT_MODE_INVALID
} mapred_output_mode_t;



/*
 * Many objects in the map-reduce schema reference other objects by name.
 * During the parse we store these with just the name and later we hook
 * up all the references.
 */
typedef struct mapred_reference_ {
	char                 *name;
    mapred_object_t      *object;
} mapred_reference_t;

/* character list */
struct mapred_clist_ {
	char                 *value;
	mapred_clist_t       *next;
};

/* parameter list */
struct mapred_plist_ {
	char                 *name;
	char                 *type;
	mapred_plist_t       *next;
};

/* object list */
struct mapred_olist_ {
	mapred_object_t      *object;
	mapred_olist_t       *next;
};

/* Object types */
typedef struct {
	mapred_input_kind_t    type;
	char                  *desc;
	mapred_clist_t        *files;
	mapred_plist_t        *columns;
	mapred_format_t        format;
	char                  *delimiter;
	char                  *encoding;
	char                  *null;
	char                  *quote;
	char                  *escape;
	int                    error_limit;
} mapred_input_t;

typedef struct {
	mapred_output_kind_t   type;
	char                  *desc;
	mapred_output_mode_t   mode;
	mapred_format_t        format;
	char                  *delimiter;
	char                  *encoding;
} mapred_output_t;

typedef struct {
	char                *body;
	char                *language;
	char                *library;
	mapred_plist_t      *parameters;
	mapred_plist_t      *returns;
	mapred_plist_t      *internal_returns;
	mapred_reference_t   rtype;
	mapred_mode_t        mode;
	int                  flags;
#define mapred_function_strict    0x00000001
#define mapred_function_immutable 0x00000002
#define mapred_function_unordered 0x00000004
	int                  lineno;
} mapred_function_t;

/* Mappers, Transitions, Combiners, and Finalizers are all just functions */
typedef mapred_function_t mapred_mapper_t;
typedef mapred_function_t mapred_transition_t;
typedef mapred_function_t mapred_combiner_t;
typedef mapred_function_t mapred_finalizer_t;

typedef struct {
	mapred_reference_t   transition;
	mapred_reference_t   combiner;
	mapred_reference_t   finalizer;
	char                *initialize;
	mapred_plist_t      *parameters;     /* points into transition plist */
	mapred_plist_t      *returns;
	mapred_clist_t      *keys;
	mapred_clist_t      *ordering;
} mapred_reducer_t;

typedef struct {
	mapred_reference_t  input;
	mapred_reference_t  mapper;
	mapred_reference_t  reducer;
	mapred_reference_t  output;
	boolean             execute;
	int                 flags;
#define mapred_task_resolving   0x00000001
#define mapred_task_resolved    0x00000002

	/* calculated parameters, setup during resolve_dependencies */

	mapred_plist_t     *parameters;     /* points into input plist */
	mapred_plist_t     *grouping;
	mapred_plist_t     *returns;
} mapred_task_t;


/*
 * Whenever a function returns more than a single column an abstract
 * data type will be transparently created.
 */
typedef struct {
	mapred_plist_t     *returns;
} mapred_adt_t;

typedef struct
{
	char   *buffer;
	size_t  bufsize;
	size_t  position;
	size_t  grow;
} buffer_t;

typedef struct {
	char           *version;
	char           *database;
	char           *user;
	char           *host;
	int             port;
	mapred_olist_t *objects;
	mapred_olist_t *execute;
	int             flags;
#define mapred_document_defines  0x00000001
#define mapred_document_executes 0x00000002
#define mapred_document_error    0x10000000
	int             id;
	char           *prefix;
	buffer_t       *errors;
} mapred_document_t;

/* Union of all mapred object types */
struct mapred_object_ {
	mapred_kind_t           kind;
	char                   *name;
	int                     line;
	boolean                 created;
	boolean                 internal;
	union {
		mapred_document_t   document;
		mapred_input_t      input;
		mapred_output_t     output;
		mapred_function_t   function;
		mapred_reducer_t    reducer;
		mapred_task_t       task;
		mapred_adt_t        adt;
	} u;
};

/* Functions */
void mapred_run_document(PGconn *conn, mapred_document_t *doc);
mapred_olist_t* mapred_parse_file(FILE *file);
mapred_olist_t* mapred_parse_string(unsigned char *yaml);

void mapred_dump_yaml(mapred_object_t *obj);

void mapred_destroy_object(mapred_object_t **obj);
void mapred_destroy_olist(mapred_olist_t **olist);
void mapred_destroy_plist(mapred_plist_t **plisth);
void mapred_destroy_clist(mapred_clist_t **clist);

/* Global variables, defined in main.c */
extern int global_verbose_flag;
extern int global_debug_flag;
extern int global_print_flag;
extern int global_explain_flag;
extern mapred_plist_t *global_plist;

/* Flags set in global_explain_flag */
#define global_explain 0x00000001
#define global_analyze 0x00000002

#endif
