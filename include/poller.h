/*
 * poller.h
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */

#ifndef INCLUDE_POLLER_H_
#define INCLUDE_POLLER_H_


#include <sys/poll.h>
#include <channel.h>
#include <eventloop.h>
#include <muduo.h>
#include <list.h>


extern void poller_init(struct poller * poll );
extern void poller_destroy(struct poller * p);

extern int poller_update_channel( struct poller * p, struct channel * c );
extern int poller_remove_channel(struct poller * p, struct channel * c );

extern int poller_poll(struct poller * p, struct event_loop * el );

/* 另外隔离处理 是为了后来的延展， 不知道数据结构会怎么变 */
//struct channel * poller_check_cache(struct poller * p, int fd );
//
//void poller_update_cache(struct poller * p, struct fd_map * fm );
//
//struct channel * poller_check_map(struct poller * p, int fd );
//
//static void poller_insert_fdmap( struct poller * p, struct fd_map * fm);
//
//static void poller_del_fdmap( struct poller * p, struct fd_map * fm );
//
//static struct channel * get_channel(struct poller * p, int fd);

/*
#define POLLIN		0x001		 There is data to read.
#define POLLPRI		0x002		 There is urgent data to read.
#define POLLOUT		0x004		Writing now will not block.

#define POLLERR		0x008		 Error condition.
#define POLLHUP		0x010		 Hung up.
#define POLLNVAL	0x020		 Invalid polling request.

# define POLLMSG	0x400
# define POLLREMOVE	0x1000
# define POLLRDHUP	0x2000

*/
/* 0 - OK  	-1 - ERROR */

#endif /* INCLUDE_POLLER_H_ */
