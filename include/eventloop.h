/*
 * eventloop.h
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */

#ifndef INCLUDE_EVENTLOOP_H_
#define INCLUDE_EVENTLOOP_H_




#include <muduo.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include <timer_queue.h>

typedef  void (*el_action_t)(void * );

#define EVENT_LOOP_INLOOP   	(0x1)
#define EVENT_LOOP_PENDING		(0x2)
#define EVENT_LOOP_POLLING		(0x4)
#define EVENT_LOOP_EXITING		(0x8)
#define EVENT_LOOP_HDCALLBACK	(0x10)
#define EVENT_LOOP_WREVENTFD	(0x20)

extern void event_loop_init(struct event_loop * el, pthread_t owner );
extern void event_loop_destroy(struct event_loop * el );


extern void event_loop(struct event_loop * el );
extern void el_exit(struct event_loop *);

extern void el_next_poll_nonblock(struct event_loop * el );


/* if in this thread and do it immediately*/
extern void el_add_pending_functions(struct event_loop * el, el_action_t act, void * arg );

extern void el_run_after(struct event_loop * el,
		timer_cb act, void * arg, int ms);

#endif /* INCLUDE_EVENTLOOP_H_ */
