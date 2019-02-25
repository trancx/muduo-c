/*
 * eventloop.c
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */


#include <malloc.h>
#include <stdio.h>
#include <errno.h>


#include <eventloop.h>
#include <channel.h>
#include <poller.h>
#include <timer_queue.h>


#ifndef CONFIG_DEBUG_CORE
#undef DEBUG
#endif


typedef struct pending {
	el_action_t act;
	void * arg;
} el_pending_t;



#ifdef CONFIG_DEBUG_EVENTLOOP
static void el_debug_status(struct event_loop * el ) {
	__builtin_prefetch(&el->status);
	if( el->status & EVENT_LOOP_INLOOP ) {
		puts("EL is alive");
	}
	if( el->status & EVENT_LOOP_HDCALLBACK ) {
		puts("EL is handling callback");
	}
	if( el->status & EVENT_LOOP_PENDING ) {
		puts("EL is pending");
	}
	if( el->status & EVENT_LOOP_WREVENTFD ) {
		puts("EL has write a eventfd");
	}
	if( el->status & EVENT_LOOP_POLLING) {
		puts("EL is polling/sleeping");
	}
	if( el->status & EVENT_LOOP_EXITING ) {
		puts("EL is exiting");
	}
	if(  0 ) {

	}
}
#else
	static void el_debug_status(struct event_loop * el ) { }
#endif

/**
 *  XXX: 这里说明 对于channel callback 不能是void 否则 callback can do nothing
 *  那么返回的必须是必须是 channel 因为这就是保证了channel 只知道自己 不知道谁在使用它
 *  是不是这样？ 是的 就算是内嵌入 我们用container_of 都可以得到自己想要的！！！！
 *
 *  非常重要的收获
 *
 *  FIXME: 对于read write的编程规范涉及甚少，所以这里必须完善错误的处理，这里
 *  对于read_eventfd 其实有问题，如果说是因为wakeup才读，读一次就OK了，因为
 *  写多次，只会改变里面计数的值。
 */
void el_read_eventfd(struct channel * c) {
	debug_func();
	int fd;
	eventfd_t t;
	struct event_loop * el;
	ssize_t size;
//	el = c->el;  这是巧合，正好 el ch 耦合
	el = container_of(c, struct event_loop, event_ch); /* So, embed is AWESOME! */
	fd = c->fd;

/*	if( el->status & EVENT_LOOP_WREVENTFD ) {  just in case! NONEED*/
again:
/* we assume that this wouldnt block for long time! */
		size = read(fd, &t, sizeof(eventfd_t));
		if( size < 0) {
			switch( errno ) {
				case EWOULDBLOCK:
					goto again;
					break;
				default:
					DIE(1); /* DIE_HERE write_fd has encouter a problem */
			}
		}
		pr_debug("read eventfd: %lu\n", t);
		el->status &= ~EVENT_LOOP_WREVENTFD;
}

static void el_wakeup_fd_init(struct event_loop * el ) {
	el->wakeup_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
	if( el->wakeup_fd == -1 ) {
		perror("error create the eventfd");
		/* DIE_HERE */
	}
	channel_init(&el->event_ch, el, el->wakeup_fd, el->owner);
	pr_debug("event fd: %d\n", el->wakeup_fd);
	channel_set_read_callback(&el->event_ch, el_read_eventfd); /* el_read_eventfd */
	channel_enable_read(&el->event_ch);
}

static inline void el_mutex_init(struct event_loop * el ) {
	pthread_mutexattr_init(&el->attr);
	pthread_mutexattr_settype(&el->attr, PTHREAD_MUTEX_NORMAL);
	pthread_mutex_init(&el->mux, &el->attr);
}

void event_loop_init(struct event_loop * el, pthread_t owner ) {
	el->poll = NULL;
	el->poll = malloc(sizeof(struct poller));
	el->tq = malloc(sizeof(struct timer_queue));
	if( !el->poll || !el->tq ) {
		perror("memory exhausted!");
		exit(1);
	}
	poller_init(el->poll);
	timer_queue_init(el->tq, el);

	el->status = 0;
	el_mutex_init(el);
	v_init(el->pending_funcs, el_pending_t); /* accessed by other thread */
	v_init(el->local_funcs, el_pending_t);  /* local cache of it*/

	v_init(el->active_channels, struct channel *);
	el->owner = owner; /*  owner */
	el_wakeup_fd_init(el);  /* 这里需要使用到 owner poller  */
}

void event_loop_destroy(struct event_loop * el ) {
	/* FIXME: destructor el */

	timer_queue_destroy(el->tq);
//	channel_destroy(&el->event_ch);   poller destroy will handle

	poller_destroy(el->poll); /* poller 必须最后被释放 */
	v_free(el->pending_funcs);
	v_free(el->active_channels);
	v_free(el->local_funcs);
	close(el->wakeup_fd);
	free(el->poll);
	free(el->tq);
}

void el_exit(struct event_loop * el ) {
	debug_func();
	if( el->owner != pthread_self() ) {
		el_next_poll_nonblock(el);
		pr_debug(" %lu call exit\n", pthread_self());
		el_debug_status(el);
		pr_debug("%s->el_poll_nonblock\n",__FUNCTION__);
	}
	el->status |= EVENT_LOOP_EXITING;
}

/*  打算把pending vector直接交换指针，这样会不会影响cache?
 	其他线程都是调用共享vector 每次直接改指针似乎会让cache无效
 	不如复制好了 因为这样直接读，不会使cache无效，而本地的vector
 	只有自己使用, ！！ 不过我们还是得清空共享的vector 那就没必要
 	复制了，直接交换指针,
 	FIXME: 考虑用atomic access 而不是 lock
 */
static void el_do_pending_functions(struct event_loop * el ) {
	int i;
	el_pending_t * ppf;
	debug_func();
	el->status |= EVENT_LOOP_PENDING;
	BUG_ON( !v_empty(el->local_funcs) );
	pthread_mutex_lock(&el->mux);
	//	v_cpy(lhs, rhs)  deprecated!
	v_swap(el->pending_funcs, el->local_funcs);
	pthread_mutex_unlock(&el->mux);
	for_each_vector(i, ppf, el->local_funcs) {
		ppf->act(ppf->arg);
	}
	v_clear(el->local_funcs);
	el->status &= ~EVENT_LOOP_PENDING;
}

/* if in this thread and do it immediately*/
void el_add_pending_functions(struct event_loop * el, el_action_t act, void * arg ) {
	/**
	 * 借用muduo思想，1. 如果owner调用，直接call
	 * 对于其他线程， 2. 如果本线程正在callback
	 * 反正下面就会调用 那就不管先，3. 如果正在
	 * 在pending 我们得write_fd 保证下次的
	 * poll不要睡眠，还有等着它操作的pending
	 *
	 * 如果是空的不要上锁！建议设置一个flag!
	 */
	if( el->owner == pthread_self() ) {
		act(arg); /* 不管是不是退出，都得执行 */
	}  else  {
		if ( !(el->status & EVENT_LOOP_EXITING) ) {
			el_pending_t pf;
			pf.act = act;
			pf.arg = arg;
			pthread_mutex_lock(&el->mux);
			v_push(el->pending_funcs, pf);
			pthread_mutex_unlock(&el->mux);
			if( !(el->status & EVENT_LOOP_HDCALLBACK) ) {
				pr_debug("%s->el_poll_nonblock\n",__FUNCTION__);
				el_debug_status(el);
				el_next_poll_nonblock(el);
			}
		} else {
			pr_debug("eventloop already exit but called add_pend_funcs\n");
		}
	}
	/* FIXME: when el->flag & EXITTING, how to code ? */
}

/**
 * XXX: 还有一种方法就是在 设置pending func那里设置EMPTY flag
 * 然后每次处理do_pending之前就判断是不是空的
 *	trick处理完本地的Pending( swap 下来的) 再判断一次empty flag
 *	这样就不用等下次了，这和中断处理的多CPU配合异曲同工
 *
 * 那么对于第一个加入进去的函数 add_pending 上锁第一件事就是把flag清空了
 * 这样上面的cpu处理完了 读flag又不是空，说明后面又有新的，所以上锁
 * 再处理，但是这里必须保证pending不能无止境的加入，不然就出不去了。
 *
 * 然后在发现EL正在pending 我们就不用调用next_poll_nonblock了，
 * 因为出来之后它发现empty已经被请空 就会在拿下来处理
 */
void el_next_poll_nonblock(struct event_loop * el ) {
	eventfd_t i = 1UL;
	int fd;
	ssize_t size;

	debug_func();
	if( !(el->status & EVENT_LOOP_WREVENTFD) ) {
		fd = el->wakeup_fd;
		BUG_ON( fd <= 2 );
again:	/* we assume that this wouldnt block for long time! */
		size = write(fd, &i, sizeof(eventfd_t));

		if( size < 0 ) {
			if( errno == EWOULDBLOCK || errno == EAGAIN ) {
				pr_debug("%s: write fail and repeat!\n", __FUNCTION__);
				goto again;
			}
		}
		el->status |= EVENT_LOOP_WREVENTFD;
	}
#ifdef CONFIG_DEBUG_EVENTLOOP
	else {
		pr_debug("%s reentrant\n",__FUNCTION__);
	}
#endif
}

struct timer_vars {
	union {
		struct event_loop * el;
		struct timer * t;
	} owner;
	timer_cb act;
	void * arg;
	int ms;
};

/* when expired! */
static void el_forward_timer(void * arg) {
	struct timer * t = NULL;
	struct timer_vars * tvp;

	tvp = arg;
	tvp->act(tvp->arg); /* control transfer! */
	t = tvp->owner.t;
	tq_free_timer(t);
	free(tvp);
}

/* when handle pending function */
static void eL_add_timer(void * arg) {
	struct event_loop * el;
	struct timer_vars * tvp;
	struct timer_queue * tq;
	struct timer * timer;
	int secs, ms;

	tvp = arg;
	el = tvp->owner.el;
	tq = el->tq;
	ms = tvp->ms;
	/* for speed! */
	secs = ms >> 10;
	ms = ms & ~( 1 << 10);
	timer = tq_add_timer(tq, el_forward_timer, tvp, secs, ms);
	tvp->owner.t = timer; /* magic! */
}

/* what should be the primitive type the callback func?
   如此实现，会让eventloop和time_queue紧密连接一起了。。
*/
/* INROVOCABLE    extern ! */
void el_run_after(struct event_loop * el,
		timer_cb act, void * arg, int ms) {
	struct timer_vars * tvp;
	tvp = malloc(sizeof(*tvp));
	if( !tvp ) {
		perror("MEMORY exhausted when creting timer_vars ");
		DIE(1); /* DIE_HERE */
	}
	if( ms < 128 )
		ms = 128;
	tvp->act = act;
	tvp->owner.el = el;
	tvp->arg = arg;
	tvp->ms = ms;
	el_add_pending_functions(el, eL_add_timer, tvp);
}

/**
 * 1. call poller_poll
 * 2. call  channel_handler(&channel)
 * 3. do pending_functions
 * 4.
 */
void event_loop(struct event_loop * el ) {
	int i;
	struct channel ** c; /* 必须是类型的指针！！ */
	debug_func();

	if( !(el->status &  EVENT_LOOP_INLOOP) ) {
		el->status |= EVENT_LOOP_INLOOP;
		while( !(el->status & EVENT_LOOP_EXITING )  ) {
			el->status |= EVENT_LOOP_POLLING;
			poller_poll(el->poll, el);
//			sleep(2);
			el->status &= ~EVENT_LOOP_POLLING;
			el->status |= EVENT_LOOP_HDCALLBACK;
			for_each_vector(i, c, el->active_channels) {
				channel_handler(*c); /* KEY: star star star*/
			}
			el->status &= ~EVENT_LOOP_HDCALLBACK;
			el_do_pending_functions(el);
			v_clear(el->active_channels);
		}
		el->status &= ~EVENT_LOOP_INLOOP;
	} else {
		perror("event_loop::loop reentrant!\n");
	}
}

