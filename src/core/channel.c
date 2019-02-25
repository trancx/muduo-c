/*
 * channel.c
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */


#include <channel.h>

/* LOG_HERE */

#define __USE_GNU
void channel_handler(struct channel * c ) {
	c->handling = 1;
#ifdef CONFIG_DEBUG_CHANNEL
	pr_debug("channel %d  is handling, revents: %d\n", c->fd, c->revents);
#endif

	if( c->revents & POLLNVAL ) {
		printf("POLLINVAL with this channel\n");
	}

	if( c->revents & (POLLERR | POLLNVAL) ) {
		printf("error with this channel\n");
		if( c->e_cb )
			c->e_cb(c);
		channel_invalidate(c);
	}

	if( (c->revents & POLLHUP) && !( c->revents & POLLIN) ) {
		printf("channel needs to be closed\n");
		if( c->c_cb )
			c->c_cb(c);
		channel_invalidate(c);
	}

	if( c->revents & (POLLIN | POLLPRI  ) ) { /* | POLLRDHUP */
		if( c->r_cb )
			c->r_cb(c);
	}

	if( c->revents & POLLOUT ) { /* Writing now will not block. */
		if( c->w_cb)
			c->w_cb(c);
	}

	c->handling = 0;
}

/* when call other _funcs in body, donot use INLINE */
void channel_enable_read(struct channel * c) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d enable read\n", c->fd);
#endif
	c->events |= (POLLIN | POLLPRI);
	if( !c->r_cb)
		BUG_ON("read cb is NULL!\n");
	else
		poller_update_channel(c->el->poll, c);
}

#ifdef CONFIG_DEBUG_POLLPRI
void channel_enable_urgent(struct channel * c ) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d enable urgent\n", c->fd);
#endif
	c->events |= ( POLLPRI); /* POLLIN | */
	poller_update_channel(c->el->poll, c);
}
#endif


void channel_disable_read(struct channel * c ) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d disable read\n", c->fd);
#endif
	if( c->events & (POLLIN | POLLPRI) ) {
		c->events &= ~( POLLIN | POLLPRI);
		poller_update_channel(c->el->poll, c);
	}
}

/* when write blocks, we set this let the buffer send the left */
void channel_enable_write(struct channel * c) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d enable write\n", c->fd);
#endif
	c->events |= POLLOUT;
	if( c->w_cb )
		poller_update_channel(c->el->poll, c);
	else
		BUG_ON("write cb is null\n");
}

void channel_disable_write(struct channel * c) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d disable write\n", c->fd);
#endif
	if( c->events & POLLOUT) {
		c->events &= ~POLLOUT;
		poller_update_channel(c->el->poll, c);
	}
}

/**
 * channel is READABLE and WRITEBLE, DOES NOTHING
 */
void channel_enable_rw(struct channel * c ) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d enable read & write\n", c->fd);
#endif
	if( !(c->events & (POLLIN | POLLPRI) ) ||
			!(c->events & POLLOUT) )   {
		c->events |= (POLLIN | POLLPRI | POLLOUT);
		poller_update_channel(c->el->poll, c);
	}
}

void channel_disable_rw(struct channel * c) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d diable read & write\n", c->fd);
#endif
	if( c->events & (POLLIN | POLLPRI | POLLOUT) ) {
		c->events = 0;
		poller_update_channel(c->el->poll, c);
	}
}

/**
 * REGULATIONs:
 *  	1. close callback no need to call channel_destroy() & channel_invalidate
 *  	2. channel_invaliate() will not call close_callback()
 *  	   since it's informed the upper before.
 *  	3. channel_destror() is designed for the use when forced to close
 *  	   a channel and the upper is unknown. so the user(the upper) will not use it
 *  	   but the CLOSE_CALLBACK will be called!
 *		4. upper layer can call channel_invalidate when no need for a
 *		   channel deliberately.!!
 *
 *  	因为无法避免递归，干脆上层比如Connecion，的close callback就不需要回调
 *  	任何channel 函数，因为底层通知你，因为它知道如何处理，而它只需要处理自己
 *  	那一层就好了。
 *
 *  	但是上层可能需要让一个Channel无效，比如关闭一个connection，那就需要调用
 *  	channel_invalidate()!
 */

void channel_invalidate(struct channel * c ) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d is invalidated！\n", c->fd);
#endif
	poller_remove_channel(c->el->poll, c);
	c->events = 0;
	c->revents = 0;
}

/* call close callback??  YES. cause the upper is unknow  */
void channel_destroy(struct channel * c ) {
#ifdef CONFIG_DEBUG_CHANNEL
	printf("channel %d is going to be destroyed\n", c->fd);
#endif
	if( c->c_cb ) {
		c->c_cb(c);
	}
}

/**						NOTES
 *
 *  1.  if reciever has not read all the data, error will be sent,
 * 		as an unexepected cut of power or the reciever deliberately
 * 		make it. SO the channel will be marked error.
 * 		BUT if the data has been read, the process killed by the reciever
 * 		will not cause an error, and instead the fd will be readable and the return
 * 		value of read() will be zero, indicating the end of the connection.
 * 	2.
 */
