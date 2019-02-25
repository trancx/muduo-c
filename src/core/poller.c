/*
 * poller.c
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */

#include <stdlib.h>
#include <poller.h>

extern void channel_destroy(struct channel * );

#ifndef CONFIG_DEBUG_CORE
#undef DEBUG
#endif

/*
  struct pollfd
  {
    int fd;		 			File descriptor to poll.
    short int events;		Types of events poller cares about.
    short int revents;		Types of events that actually occurred.
  };
*/

struct addr {
	union {

	};
};


#define to_fd_map(ptr) \
	container_of(ptr, struct fd_map, brother)

void poller_init_map(struct poller * p ) {
	int i;
	for(i=0; i< FD_MAP_NR_HASH; i++ ) {
		INIT_HLIST_HEAD(&p->map[i]);
	}
}

void poller_init_cache(struct poller * p ) {
	int i;
	for(i=0; i < FD_MAP_CACHE_ENTRY; i++ ) {
		p->cache[i] = NULL;
	}
}

void poller_init(struct poller * poll) {
	v_init(poll->poll_list, struct pollfd );
	poller_init_map(poll);
	poller_init_cache(poll);
}

#define to_fmmap(ptr, member) \
	container_of(ptr, struct fd_map, brother)

static void poller_fdmap_destroy(struct poller * poll ) {
	/* hlist_for_each and free */
	struct channel * ch;
	struct fd_map * fm;
	struct hlist_node * pos;
	struct hlist_head * head;
	int i;

	for(i=0; i < FD_MAP_NR_HASH; ++i) {
		head = &poll->map[i];
		while( !hlist_empty(head) ) {
			pos = head->first;
			fm	= to_fmmap(pos, brother);
			ch = fm->ch;
			channel_destroy(ch);	/* simply call close callback! */
			hlist_del(pos);
			free(fm);
		}
	}
}

/**
 *  XXX 释放 poller 和 channel 有没有更好的办法？
 */
void poller_destroy(struct poller * poll ) {
	/* FIXME: destructor poller */
	/*
	 * fd map hash table, free free free!
	 */
	poller_fdmap_destroy(poll);
	v_free(poll->poll_list);
}

//#define POLLER_CHECK_CACHE

/* 另外隔离处理 是为了后来的延展， 不知道数据结构会怎么变 */
struct fd_map * poller_check_cache(struct poller * p, int fd ) {
	struct fd_map * fm = NULL;
	struct fd_map * ret = NULL;

	fm = p->cache[fd & FD_MAP_CACHE_ENTRY_MASK];
#ifdef POLLER_CHECK_CACHE
	pr_debug("%s cache id: %d\n", __FUNCTION__, fd & FD_MAP_CACHE_ENTRY_MASK );
#endif
	if( fm && fm->fd == fd ) {
#ifdef POLLER_CHECK_CACHE
		pr_debug("cache targeted! fd = %d\n", fm->fd);
#endif
		ret = fm;
	}
	return ret;
}

//#define DEBUG_MAP_CACHE

#ifndef DEBUG_MAP_CACHE
static inline void poller_update_cache(struct poller * p, struct fd_map * fm ) {
	p->cache[fm->fd & FD_MAP_CACHE_ENTRY_MASK] = fm;
}
#else
	extern void poller_update_cache(struct poller * p, struct fd_map * fm );
#endif

#ifdef DEBUG_MAP_CACHE
	void poller_update_cache(struct poller * p, struct fd_map * fm ) {
		int i;

		p->cache[fm->fd & FD_MAP_CACHE_ENTRY_MASK] = fm;
		for( i=0; i<FD_MAP_CACHE_ENTRY; i++) {
			if( p->cache[i] )
				printf("cache %d, fd: %d\n", i, p->cache[i]->fd);
		}
	}
#endif

static struct fd_map * poller_check_map(struct poller * p, int fd ) {
	struct fd_map * fm = NULL, * ret = NULL;
	struct hlist_head * head = NULL;
	struct hlist_node * pos = NULL;
	int hash = -1;

	hash = fd & FD_MAP_HASHP_MASK;
	head = &p->map[hash];
	/* KEY: trick: typeof(*tpos) */
	hlist_for_each_entry(fm, pos, head, brother) {
		if( fm->fd == fd ) {
			ret = fm;
			break;
		}
	}
	return ret;
}

/*  根据fd 插入哈希表 更新cache */
static void poller_insert_fdmap( struct poller * p, struct fd_map * fm) {
	int fd;
	struct hlist_head * hh;

	fd = fm->fd;
	BUG_ON( fd < 0 || fm->ch == NULL );
	pr_debug("fd: %d is inserted in map %s\n", fd, __FUNCTION__);

	hh = &p->map[fd & FD_MAP_HASHP_MASK];
	hlist_add_head(&fm->brother, hh);
	poller_update_cache(p, fm);
}

static void poller_del_fdmap( struct poller * p, int fd ) {
	struct fd_map * fm = NULL;

	fm = poller_check_cache(p, fd);
	if( !fm ) {
		fm = poller_check_map(p, fd);
	}
	BUG_ON( fm == NULL);
	if( fm ) {
		hlist_del(&fm->brother);
		pr_debug("fd: %d is deleted ::%s\n", fm->fd, __FUNCTION__);
		free(fm);
	}
}

static struct channel * get_channel(struct poller * p, int fd) {
	struct channel * ret = NULL;
	struct fd_map * fm_p;
	debug_func();
	fm_p = poller_check_cache(p, fd);
	if( !fm_p ) {
		fm_p = poller_check_map(p, fd);
		if( fm_p ) {
			poller_update_cache(p, fm_p); /* 更新cache */

		}
	}
	DIE( fm_p == NULL);
	ret = fm_p->ch;
	return ret;
}

/*
#define POLLIN		0x001		 There is data to read.
#define POLLPRI		0x002		 There is urgent data to read.
#define POLLOUT		0x004		 Writing now will not block.

#define POLLERR		0x008		 Error condition.
#define POLLHUP		0x010		 Hung up.
#define POLLNVAL	0x020		 Invalid polling request.

# define POLLMSG	0x400
# define POLLREMOVE	0x1000
# define POLLRDHUP	0x2000

*/

/* 0 - OK  	-1 - ERROR */
int poller_update_channel( struct poller * p, struct channel * c ) {
	int ret = -1;
	int fd;
	struct fd_map * fm = NULL;
	struct pollfd pfd, * pp;

	fd = c->fd;
	pr_debug("%s: fd = %d, index = %d\n", __FUNCTION__, fd, c->index );
	if( c->index < 0 ) {
		fm = malloc( sizeof(struct fd_map) );
		BUG_ON( !fm );
		fm->fd = fd;
		fm->ch = c;
		INIT_HLIST_NODE(&fm->brother);
		poller_insert_fdmap(p, fm);
		pfd.fd = fd;
		pfd.events = c->events;
		pfd.revents = 0;
		v_push(p->poll_list, pfd); /* looks strange, but it will work,,,, */
		c->index = v_size(p->poll_list) - 1;
		ret = 0;
	} else {
		/* according c->index find its position */
		pp = v_index(p->poll_list, c->index);
		BUG_ON( pp == NULL || pp->fd != c->fd );
		pp->events = c->events;
		if( pp->events == 0 ) {
			pr_debug("NO EVENT ! %d\n", c->fd);
		}
		ret = 0;
	}

	return ret;
}

/**
 *  这里有vector的复制操作，非常浪费时间。
 */
int poller_remove_channel(struct poller * p, struct channel * c ) {
	int ret = -1;
	int fd, index;
	struct channel * last;
	struct pollfd * pl;
	/**
	 * 1. delete in poll_list
	 * 2. delte in map
	 */
	fd = c->fd;
	index = c->index;
	pr_debug("%s: fd = %d, index = %d\n", __FUNCTION__, fd, index );
	if( index < 0 )
		goto out;
	/* 拿到数组最后一个pollfd->fd 相信我get_channel一般会命中cache */
	pl = v_end(p->poll_list);
	if( pl->fd != fd ) {
		last = get_channel(p, pl->fd);
		BUG_ON( last == NULL );
		v_del(p->poll_list, index); /* !! 1 */
		last->index = index;
		poller_del_fdmap(p, fd);	/* !! 2 */
	} else {
		/* 这说明去除的channel正好是最后一个 即vector->count = 1 */
		v_del(p->poll_list, index);
		poller_del_fdmap(p, fd);
//		BUG_ON( v_size(p->poll_list) != 0 );
		/* scrutiny */
	}
	c->index = -1; /* avoid recursive */
out:
	return ret;
}

/**
 * If this field is specified as zero, then
 * all events are ignored for fd and revents
 * returns zero.  SO, THERE IS BUG, WHEN disable all!
 * OR Always enable read?
 */
int poller_poll(struct poller * p, struct event_loop * el ) {
	int count = 0, num;
	size_t size = 0;
	int i;
	struct pollfd * array = NULL;
	struct vector * v = NULL;
	struct channel * c = NULL;

	debug_func();
	array = v_toarray(p->poll_list);
	size = v_size(p->poll_list);
	num = count = poll(array, size, 1000);
	pr_debug("%s: count = %d\n", __FUNCTION__, count);
	if( count ) { /* fill active list */
		v = &el->active_channels;
		BUG_ON( v == NULL );
		while( count-- ) {
			for_each_vector(i,array,p->poll_list) {
				if( array->revents > 0 ) {
					c = get_channel(p, array->fd);
					BUG_ON(  array->fd != c->fd );
					c->revents = array->revents;
					v_push(*v, c);
				}
			}
		}
	}

	return num;
}
