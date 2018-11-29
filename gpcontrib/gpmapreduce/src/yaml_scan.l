%option outfile="yaml_scan.c"
%option 8bit never-interactive nodefault nounput noyywrap noinput
%option prefix="yaml_scalar_yy"


/* 
 * This would be nice, but it isn't necessary and isn't supported by
 * the old version of flex we have on the build machines.
 *
 * If this ever changes make sure to edit parser.h to #define the
 * relevant items.
*/
/* %option header-file="yaml_scan.h" */
/* %option reentrant */

%{
#include <parser.h>
#include <yaml_parse.h>       /* Get the token types */

/* 
 * Setup prototypes since we build with -Wmissing-prototypes and flex isn't 
 * kind enough to generate them.
 */
#define unify_version(a,b,c) ((a<<16)+(b<<8)+c)
#if unify_version(YY_FLEX_MAJOR_VERSION,YY_FLEX_MINOR_VERSION,YY_FLEX_SUBMINOR_VERSION) < unify_version(2,5,35)
int   yaml_scalar_yyget_lineno(void);
FILE* yaml_scalar_yyget_in(void);
FILE* yaml_scalar_yyget_out(void);
int   yaml_scalar_yyget_leng(void);
char* yaml_scalar_yyget_text(void);

void  yaml_scalar_yyset_lineno(int);
void  yaml_scalar_yyset_in(FILE *);
void  yaml_scalar_yyset_out(FILE *);

int   yaml_scalar_yyget_debug(void);
void  yaml_scalar_yyset_debug(int);

int     yaml_scalar_yylex_destroy(void);
#endif

%}
   /* 
    * States:
    *  INITIAL = default state
    *   <kw>   = keyword
    */
%x kw
%% 

    /* Look at our parser context and set the current state accordingly */
    if (parser->frame < 0 || parser->frame >= MAX_CONTEXT_DEPTH)
       BEGIN(INITIAL);
    else
    {
       switch (parser->context[parser->frame])
       {
          case CONTEXT_NONE:
          case CONTEXT_HASH_VALUE:
          case CONTEXT_LIST:
             BEGIN(INITIAL);
             break;

          case CONTEXT_HASH_KEY:
             BEGIN(kw);      /* Switch to keyword context */
             break;

          default:
             printf("WARNING: bad parse context state\n");
             BEGIN(INITIAL);
       }
    }


   /* Keyword tokens */
<kw>columns      return _COLUMNS_;
<kw>consolidate  return _CONSOLIDATE_;
<kw>database     return _DATABASE_;
<kw>define       return _DEFINE_;
<kw>delimiter    return _DELIMITER_;
<kw>encoding     return _ENCODING_;
<kw>error_limit  return _ERROR_LIMIT_;
<kw>escape       return _ESCAPE_;
<kw>exec         return _EXEC_;
<kw>execute      return _EXECUTE_;
<kw>file         return _FILE_;
<kw>finalize     return _FINALIZE_;
<kw>format       return _FORMAT_;
<kw>function     return _FUNCTION_;
<kw>gpfdist      return _GPFDIST_;
<kw>host         return _HOST_;
<kw>initialize   return _INITIALIZE_;
<kw>input        return _INPUT_;
<kw>keys         return _KEYS_;
<kw>language     return _LANGUAGE_;
<kw>library      return _LIBRARY_;
<kw>map          return _MAP_;
<kw>mode         return _MODE_;
<kw>name         return _NAME_;
<kw>null         return _NULL_;
<kw>optimize     return _OPTIMIZE_;
<kw>output       return _OUTPUT_;
<kw>parameters   return _PARAMETERS_;
<kw>port         return _PORT_;
<kw>query        return _QUERY_;
<kw>quote        return _QUOTE_;
<kw>reduce       return _REDUCE_;
<kw>returns      return _RETURNS_;
<kw>run          return _RUN_;
<kw>source       return _SOURCE_;
<kw>table        return _TABLE_;
<kw>target       return _TARGET_;
<kw>task         return _TASK_;
<kw>transition   return _TRANSITION_; 
<kw>user         return _USER_;
<kw>version      return _VERSION_;
<kw>ordering     return _ORDERING_;

   /* Matching specific fields */
[a-z_]([a-z0-9_])*       return _ID_;

   /* This is a bit of a hack to support schema qualification, the correct
    * way to do this would require changing the lexer to be reentrant within
    * yaml scalar values which would be a lot more work.
    */
[a-z_]([a-z0-9_])*\.[a-z_]([a-z0-9_])*       return _ID_;

[0-9]+                   return _INTEGER_;
[1-9][0-9]*(\.[0-9]){3}  return _VERSION_STRING_;

  /* 
   * We will be calling this on the SCALAR tokens in a yaml file, if the
   * token doesn't exactly match one of the predefined tokens above then 
   * just return it as a 'STRING'
   */
<*>.*(\n.*)*    return _STRING_;
<<EOF>>         return 0;
%%
