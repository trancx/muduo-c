/*
 * muduo.h
 *
 *  Created on: Apr 7, 2018
 *      Author: trance
 */

#ifndef INCLUDE_MUDUO_H_
#define INCLUDE_MUDUO_H_

#include <list.h>
#include <vector.h>
#include <pthread.h>

struct channel;
struct timer_queue;
struct poller;
struct event_loop;

typedef  void (*channel_action_t)(struct channel * );

/**
 *  Transparent to the upper layer
 */
struct channel {
	struct event_loop * el;
	int fd;
	short int events;
	short int revents;
	int index; /* used by Poller. */
	unsigned long flag; /*  FIXME: may be deleted */
	channel_action_t r_cb;
	channel_action_t c_cb;
	channel_action_t e_cb;
	channel_action_t w_cb;
	/* read close */
	int handling;
	pthread_t owner;
};



#define FD_MAP_NR_HASH ( 1 << 5 )
#define FD_MAP_HASHP_MASK ( FD_MAP_NR_HASH - 1 )

#define FD_MAP_CACHE_ENTRY (FD_MAP_NR_HASH >> 2 )
#define FD_MAP_CACHE_ENTRY_MASK ( FD_MAP_CACHE_ENTRY-1 )


struct fd_map {
	int fd;
	struct channel * ch;
	struct hlist_node brother;
};

struct poller {
	/* fd <-> channel  map*/
	/*  fd array  */
	struct hlist_head map[FD_MAP_NR_HASH];
	struct fd_map * cache[FD_MAP_CACHE_ENTRY];
	struct vector poll_list;  /* 要实现 删除 插s入 等操作。。 */
	/**  对于上层我们只用 fd 意味着所有fd->channel 都由poller解决，
	 * 所以它得复杂一点  但是一开始很简单的，最多后来就是红黑数的map了
	 * 1. update map				**called by event_loop
	 * 2. (e)poll system call
	 * 3. get_active_channels()     **called by event_loop
	 * 4. update_channel(int fd, flag+read+write+....)
 	 * 5. delete_channel()
	 * 6. more func about fd channer search
	 */
//	pthread_t owner;
};

/**
 *  只有早期的内核的timer知识，那时候的很简单，直接比较jiffies
 *  我也这样写把，注意写 maintainable 函数
 *
 *  free timer 里面存放的是之前使用的timer失效之后的去处
 *  但是应该设置一个阀值，当超过多少个空闲的，就直接释放就好了。
 */
struct timer_queue{
	int timefd;
	struct channel timer_ch;
	struct list_head free_timer; /* timer we can quickly obtain */
//	struct list_head continual_timer_q;
	struct list_head single_timer_q;
	struct timespec next_expire;	/* update next expiration if necessary */
};


struct event_loop {
	struct poller * poll;
	struct timer_queue * tq;
	/* these two used by next_poll_nonblock() ! */
	int wakeup_fd;
	struct channel event_ch;

	pthread_mutexattr_t attr;
	pthread_mutex_t mux;
	struct vector pending_funcs;
	struct vector local_funcs;

	struct vector active_channels; /* active channel list! */

	/**/
	unsigned long status;
	pthread_t owner;
};

typedef struct var_list {
	int count;
	void * entry[0];
} var_t;

#endif /* INCLUDE_MUDUO_H_ */
