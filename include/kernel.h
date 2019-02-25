/*
 * 'kernel.h' contains some often-used function prototypes etc
 *
 *
 *
 *  	i take it from linux-kernel!  --trance
 */
#ifndef __KERNEL__
#define __KERNEL__

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

//#define ALIGN(x, align)
//	( ((x) +(align)-1 ) & ~((align)-1) )

#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))


#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

//#define DEBUG
//#define DEBUG_TIMER_QUEUE

#ifdef DEBUG
#define pr_debug(fmt,arg...) \
	printf(fmt,##arg)
#else
#define pr_debug(fmt,arg...) \
	do { } while (0)
#endif

#define debug_func() \
	pr_debug("in function: %s\n", __FUNCTION__)

#define BUG_ON(x) \
	do {	\
		if( x ) {	\
			printf("BUG! on " #x " in func: %s\n", __FUNCTION__);\
		}	\
	} while(0)

#define DIE(x) \
	do {	\
		if( x ) {	\
			printf("CRUSH as " #x " ");\
			printf("%s \n", __FUNCTION__); \
			exit(1); \
		}	\
	} while(0)


#define ERROR(fmt,arg...) \
	perror(fmt,##arg); \
	exit(-1)


#include <sys/syscall.h>
#include <unistd.h>

#ifdef SYS_gettid
static inline pid_t gettid(void ) {
	pid_t tid = syscall(SYS_gettid);
	return tid;
}
#endif

#endif
