
#ifndef MAPRED_ERRORS_H
#define MAPRED_ERRORS_H

/* Error codes used by the mapreduce driver. */


/* Trying to raise this would cause weird things to happen, don't do it. */
#define NO_ERROR  0

/* Internal errors */
#define MAPRED_PARSE_INTERNAL      1    
#define MEMORY_ERROR               2
#define VERSION_ERROR              3
#define CONNECTION_ERROR           4

/* User errors */
#define MAPRED_PARSE_ERROR       100

/* Other errors */
#define MAPRED_SQL_ERROR         200

/* Signals */
#define USER_INTERUPT          10000   /* SIGINT */


struct mapred_parser_;
struct mapred_object_;
int       mapred_parse_error(struct mapred_parser_ *parser, char *fmt, ...);
int         mapred_obj_error(struct mapred_object_ *obj, char *fmt, ...);

#endif
