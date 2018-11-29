
#ifndef EXCEPT_H
#define EXCEPT_H

#ifndef SETJMP_H
#include <setjmp.h>
#endif

#ifndef STDIO_H
#include <stdio.h>
#endif

/* 
 * except.h - A very simplistic exception handling framework 
 *
 * This exception system makes use of a global linked list of exception
 * frames and thus is not threadsafe!
 *
 * It's also probably not safe to nest try blocks within a single function.
 *
 * Usage:
 *
 * // in main.c
 * ALLOW_EXCEPTIONS;
 *
 * int main(...) {...}
 * 
 * // elsewhere
 * void myfunction(...)
 * {
 *    ...
 *
 *	  XTRY 
 *    {
 *      ...
 *      some_other_function(...)
 *      ...
 *    }
 *    XCATCH(errcode)
 *    {
 *      // handle an expected exception code
 *      printf(xframe.exception);
 *	  }
 *    XCATCH_ANY
 *    {
 *      XRERAISE()
 *    }
 *	  XFINALLY
 *    {
 *      ...
 *    }
 *    XTRY_END;
 *    
 *    ...
 * }
 *
 * void some_other_function(...)
 * {
 *    ...
 *    XRAISE(errcode, "errmsg");
 * }
 *
 */

typedef struct xframe_ xframe_t;
struct xframe_ 
{
	jmp_buf             jumper;
    xframe_t           *next;
	volatile int        errcode;
	volatile void      *exception;
	volatile int        state;
	volatile char      *file;
	volatile int        lineno;
};
#define XFRAME_BASE    0x00000000
#define XFRAME_HANDLED 0x00000001
#define XFRAME_ANY     0x00000002
#define XFRAME_FINAL   0x00000004

/* The global exception frame */
extern xframe_t *global_exception_frame;

/* Declare this someplace global in main.c */
#define ALLOW_EXCEPTIONS						\
	xframe_t *global_exception_frame = NULL

/* we use MAX INT for the finnally errcode */
#define ERRCODE_FOR_FINALLY_BLOCK 2147483647
#define ERRCODE_FOR_EXIT_BLOCK    2147483646
#define ERRCODE_FOR_ANY_BLOCK     2147483645
#define ASSERTION_FAILURE         2147483644

#define XRAISE(ecode, v)												\
	do {																\
		if (!global_exception_frame) {									\
			fprintf(stderr, "Error: (%d) Unhandled Exception\n",		\
					ecode);												\
			fprintf(stderr, "Raised from %s at line %d\n",				\
					__FILE__, __LINE__);								\
			exit(1);													\
		}																\
		global_exception_frame->state = XFRAME_BASE;					\
		global_exception_frame->errcode = ecode;						\
		global_exception_frame->exception = (void *) v;					\
		global_exception_frame->file   = (char*) __FILE__;				\
		global_exception_frame->lineno = __LINE__;						\
		longjmp(global_exception_frame->jumper, ecode);					\
	} while(0)

/* 
 * Note that if the final frame hasn't been executed then we long 
 * jump back to the finaly block.  Before we do this we set the
 * frame state to FRAME_FINAL to ensure that the FRAME_HANDLED flag
 * is cleared, this is better than setting to FRAME_BASE only when
 * there isn't a final block defined. 
 */
#define XRERAISE()														\
	do {																\
		if (!xframe.errcode) {											\
			fprintf(stderr, "Error: Can't reraise null exception");		\
			fprintf(stderr, "Raised from %s at line %d\n",				\
					xframe.file, xframe.lineno);						\
			exit(1);													\
		}																\
		if (!(xframe.state & XFRAME_FINAL)) {							\
			xframe.state = XFRAME_FINAL;								\
			longjmp(xframe.jumper, ERRCODE_FOR_FINALLY_BLOCK);			\
		}																\
		global_exception_frame = xframe.next;							\
		if (!global_exception_frame) {									\
			fprintf(stderr, "Error: (%d) Unhandled Exception\n",		\
					xframe.errcode);									\
			fprintf(stderr, "Raised from %s:%d\n",						\
					xframe.file, xframe.lineno);						\
			fprintf(stderr, "Current frame at %s:%d\n",					\
					__FILE__, __LINE__);								\
			exit(1);													\
		}																\
		global_exception_frame->state     = XFRAME_BASE;				\
		global_exception_frame->errcode   = xframe.errcode;				\
		global_exception_frame->exception = xframe.exception;			\
		global_exception_frame->file      = xframe.file;				\
		global_exception_frame->lineno    = xframe.lineno;				\
		longjmp(global_exception_frame->jumper, xframe.errcode);		\
	} while(0)

/* 
 * A simple built in assertion macro
 */
#define XASSERT(x)								                        \
	do {																\
		if (!(x))														\
		{																\
			if (!global_exception_frame)								\
			{															\
				fprintf(stderr, "Error: Assertion failure at %s:%d\n",	\
						__FILE__, __LINE__);							\
				fprintf(stderr, "No exception recovery stack found\n");	\
			}															\
			else														\
			{															\
				XRAISE(ASSERTION_FAILURE, NULL);						\
			}															\
		}																\
	} while(0)

#define XTRY									               		    \
	do {																\
		xframe_t xframe;												\
		xframe.errcode   = 0;											\
		xframe.exception = NULL;										\
		xframe.next      = global_exception_frame;						\
		xframe.state     = XFRAME_BASE;									\
		global_exception_frame = &xframe;								\
		switch (setjmp(xframe.jumper))									\
		{																\
			case 0:

#define XCATCH(x)													    \
 	        longjmp(xframe.jumper, ERRCODE_FOR_EXIT_BLOCK);			    \
	        case x:											            \
			xframe.state |= XFRAME_HANDLED;

#define XCATCH_ANY											            \
 	        longjmp(xframe.jumper, ERRCODE_FOR_EXIT_BLOCK);	            \
	        case ERRCODE_FOR_ANY_BLOCK:							        \
			xframe.state |= XFRAME_HANDLED;

#define XFINALLY													    \
 	        longjmp(xframe.jumper, ERRCODE_FOR_EXIT_BLOCK);			    \
	        case ERRCODE_FOR_FINALLY_BLOCK:					            \
			global_exception_frame = xframe.next;						\
			xframe.state |= XFRAME_FINAL;

/*
 * XTRY_END
 *
 * 1) If we fell through, or got called directly mark the 'any' block
 *    as hit since some CATCH statement must have been hit (or there
 *    are none and it doesn't matter.
 *
 * 2) If we hit the default block then try to call the ANY block,
 *    if it doesn't exist we'll just fall right back here again.
 *
 * 3) If we haven't hit the FINALLY block yet, do so now, if none
 *    we will fall back into the default case.
 * 
 * 4) Once both ANY and FINALLY blocks have been dealt with we just
 *    need to clean up a bit.  Pop the exception frame off the stack
 *    and if the exception wasn't handled then we have to reraise it.
 */
#define XTRY_END													    \
	        case ERRCODE_FOR_EXIT_BLOCK:					            \
	        xframe.state |= XFRAME_ANY;								    \
	        default:									                \
			if (!(xframe.state & XFRAME_ANY))					  	    \
			{														    \
				xframe.state |= XFRAME_ANY;							    \
				longjmp(xframe.jumper, ERRCODE_FOR_ANY_BLOCK);		    \
			}														    \
	        if (!(xframe.state & XFRAME_FINAL))					  	    \
			{														    \
				xframe.state |= XFRAME_FINAL;						    \
				longjmp(xframe.jumper, ERRCODE_FOR_FINALLY_BLOCK);	    \
			}														    \
			break;												 	    \
		}												 		        \
		global_exception_frame = xframe.next;						    \
		if (!(xframe.state & XFRAME_HANDLED) && xframe.errcode)		    \
			XRERAISE();												    \
	} while (0)


#endif



