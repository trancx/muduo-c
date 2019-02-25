/*
 * channel.h
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */

#ifndef INCLUDE_CHANNEL_H_
#define INCLUDE_CHANNEL_H_

#include <sys/poll.h>
#include <muduo.h>
#include <poller.h>

#ifndef NULL
#define NULL ((void *)0)
#endif


static inline void channel_init(struct channel * c, struct event_loop * el, int fd, pthread_t owner) {
	c->el = el;
	c->fd = fd;
	c->revents = 0;
	c->events = 0;
	c->index = -1;
	c->flag = 0;
	c->r_cb = c->c_cb = c->e_cb = c->w_cb = NULL;
	c->handling = 0;
	c->owner = owner; /*  current thread pid */
}

extern void channel_invalidate(struct channel * );

static inline void channel_set_fd(struct channel * c, int fd ) {
	c->fd = fd;
}

static inline void channel_set_read_callback(struct channel * c, channel_action_t act ) {
	c->r_cb = act;
}

static inline void channel_set_close_callback(struct channel * c, channel_action_t act ) {
	c->c_cb = act;
}

static inline void channel_set_write_callback(struct channel * c, channel_action_t act ) {
	c->w_cb = act;
}

static inline void channel_set_error_callback(struct channel * c, channel_action_t act ) {
	c->e_cb = act;
}

extern void channel_handler(struct channel * );

extern void channel_enable_read(struct channel * );
extern void channel_disable_read(struct channel * );
/* when write blocks, we set this let the buffer send the left */
extern void channel_enable_write(struct channel * );
extern void channel_disable_write(struct channel *);

extern void channel_enable_rw(struct channel *);
extern void channel_disable_rw(struct channel *);


#ifdef CONFIG_DEBUG_POLLPRI
extern void channel_enable_urgent(struct channel * );
#endif

#endif /* INCLUDE_CHANNEL_H_ */
