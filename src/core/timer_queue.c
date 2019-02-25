/*
 * timer_queue.c
 *
 *  WARNING:: ALL funcs for time_queue is not thread safe!
 *
 *  ::
 *  	对于非本线程，调用的eventloop的接口不能得到连续执行的timer,
 *  	因为不能取消，而且不安全，设计应该是外部调用的都是不可撤回的
 *  	定时函数，而我们本线程使用可以得到timer 的引用，因为是安全的
 *
 *  	还有就是外部的接口是通过pending_func实现,还可以计算的pending
 *  	func的平均调用时间，如果小于这个值直接调用就好了
 *
 *  	总之基于2：
 *  		1. 外部只能用timer,但是不能cancel，得自己处理
 *  		2. 内部可以支持refresh等操作，如果需要反复建议在函数中
 *  		插入重复添加timer的代码。
 *
 */

#include <timer_queue.h>
#include <channel.h>
#include <errno.h>
#include <sys/types.h>

#define to_timerqueue(ptr, name) \
	container_of(ptr, struct timer_queue, name)

/* list_entry is the same!  */
#define to_timer(ptr, name) \
	container_of(ptr, struct timer, name)


#ifdef CONFIG_DEBUG_TIMERQUEUE
static inline void tq_debug_timerqueue(struct timer_queue * tq ) {
	struct timer * t = NULL;
	int i = 0;

	puts("*************************");
	list_for_each_entry(t, &tq->single_timer_q, tq_node) {
		printf("%d %lus:%lums \n", ++i, t->expire.tv_sec,t->expire.tv_nsec/1000000 );
		if( i > 10 )
			break; /* avoid infinite loop! */
	}
	puts("*************************");
}

static inline void tq_debug_timer(struct timer * t) {
	printf("timer %lus:%lums \n", t->expire.tv_sec,t->expire.tv_nsec/1000000 );
}
#else
static inline void tq_debug_timerqueue(struct timer_queue * tq ) { }
static inline void tq_debug_timer(struct timer * t) { }
#endif

/**
 *  ASSUME: left fileds will be initialized later
 */
static inline void timer_init(struct timer * t, struct timer_queue * owner) {
//	memset(t, 0, sizeof(*t));   WASTE CIRCLE !!
	INIT_LIST_HEAD(&t->tq_node);
	t->tq = owner;
}

static inline void timer_set_value(struct timer * t,timer_cb cb,
		void * args, int sec, int ms ) {
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);

	now.tv_sec += sec;
	if( ms ) {
		printf("%d align to ", ms);
		ms = ALIGN(ms, MINIMAL_INTERVAL);
		printf("%d\n", ms);
	}
	now.tv_nsec += ms * 1000000;
	while( now.tv_nsec >= 1000000000 ) {
		now.tv_nsec -= 1000000000;
		now.tv_sec++;
	}
	t->cb = cb;
	t->arg = args;
	t->expire = now;
}

static inline void timer_handle_callback( struct timer * t) {
	t->cb(t->arg);
}

/** 	check  now is after exam
 *  	1 true  0 false
 */
int static inline timer_expired(struct timespec * now, struct timespec * exam ) {
	int ret = 0;

	if( now->tv_sec > exam->tv_sec ) {
		ret = 1;
	} else if ( now->tv_sec == exam->tv_sec
		&& now->tv_nsec >= exam->tv_nsec) {
		ret = 1;
	}
#ifdef CONFIG_DEBUG_TIMERQUEUE
	if( ret )
		printf("(%s): timer expired!! \n", __FUNCTION__);
#endif
	return ret;
}

/* checkpoints:
 *
 * 1. new creation
 * 2. deletion/free
 *
 * an expiration no need to check 	(这个必须更新) S: 当处理完队列为空
 * refreshment will call insert   	这个情况队列不会空
 *
 *  ####free will call delete####  rare case
 * 当删除timer为最后一个的时候，队列为空，此时并没有处理timerfd，我们选择
 * 直接忽略，让内核调用，在handle callback的时候不处理就好了。
 *
 * 如此当处理完expiration,队列为空，也不用做任何事情，代码也更加简单。
 */
static int tq_update_expiration( struct timer_queue * tq ) {
	int ret = 0;
	struct itimerspec next;
	struct timer * t;
	struct list_head * q;

	q = &tq->single_timer_q;
	if( !list_empty(q) ) {
		t = to_timer(q->next, tq_node);
		next.it_interval.tv_nsec = 0;
		next.it_interval.tv_nsec = 0;
		next.it_value = t->expire;
		ret = timerfd_settime(tq->timefd, CLOCK_MONOTONIC, &next, NULL);
		/* more check for the old ! */
		tq->next_expire = t->expire;
#ifdef CONFIG_DEBUG_TIMERQUEUE
		printf("next expire: %lu:%lu\n", t->expire.tv_sec, t->expire.tv_nsec/1000000);
#endif
	}
#ifdef CONFIG_DEBUG_TIMERQUEUE
	else {
		printf("(%s): now the queue is empty!\n", __FUNCTION__);
	}
#endif
	return ret;
}

/**
 *   when the HEAD of the queue differs frome the NEXT_EXPIRE ,
 *   we need to update!
 *
 */
static int tq_need_update_expiration(struct timer_queue * tq) {
	int ret = 0;
	struct timer * cdd; /* candidate */
	struct list_head * q;
	struct timespec * ts;

	q = &tq->single_timer_q;
	if( !list_empty(q) ) {
		cdd = to_timer(q->next, tq_node);
		ts = &tq->next_expire;
		if( cdd->expire.tv_nsec != ts->tv_nsec ||
				cdd->expire.tv_sec != ts->tv_sec)
			ret = 1;
	}
#ifdef CONFIG_DEBUG_TIMERQUEUE
	else {
		printf("%s: timerqueue now is empty, need update!\n", __FUNCTION__);
	}
#endif
	/*
	 	 when queue is empty， e.g. delete a timer
	 	 IGNORE it, 我认为内核管理timer开销更大，干脆让他
	 	 调用，我们在handle的时候直接不处理的就好了，因为删除
	 	 一个timer,并且不插入回去是比较少见的，所以，在update
	 	 函数，我们对于空的列表也是直接忽略。
	 */
	return ret;
}


/* we can try to make use of another structure to boost it
 *
 * for a timer, if not in the queue, it must be empty!!
 * 		1. an allocation
 * 		2. a refreshment
 *  Both conditions can ASSURE the timer.tq_node is empty!
 */
static void tq_insert_timer(struct timer * t) {
	struct timer_queue * tq = NULL;
	struct timer * next = NULL;

	tq = t->tq;
//	DIE( tq == NULL );
	tq_debug_timerqueue(tq);
	tq_debug_timer(t);
	if( list_empty(&tq->single_timer_q) ) {
		goto easy;
	}
	list_for_each_entry(next, &tq->single_timer_q, tq_node) {
		if( timer_expired(&t->expire, &next->expire) )
			continue;
		list_add_tail(&t->tq_node, &next->tq_node);
		pr_debug("after INSERTION\n");
		tq_debug_timerqueue(tq);
		break;
	}
easy:
	if( list_empty(&t->tq_node) ) {
		pr_debug("empty\n");
		list_add_tail(&t->tq_node, &tq->single_timer_q);
		pr_debug("after INSERTION\n");
		tq_debug_timerqueue(tq);
	}
	if( tq_need_update_expiration(tq) ) {
		pr_debug("(%s): INSERTION , update expiration\n", __FUNCTION__);
		tq_update_expiration(tq);
	}
}

/* detach it, maybe we wanna refresh it! and init !  */
static inline void tq_del_timer(struct timer * t) {
	list_del_init(&t->tq_node); /* key for _insert_timer() */
	if( tq_need_update_expiration(t->tq) ) {
		pr_debug("(%s): DELETION , update expiration\n", __FUNCTION__);
		tq_update_expiration(t->tq);
	}
}

/* Without check update, because we will re-attach it
 * and tq_insert_timer() will check.
 */
static inline void __tq_del_timer(struct timer * t ) {
	list_del_init(&t->tq_node);
}


/* we need scrutiny check!! 参数检查 */
int tq_refresh_timer(struct timer * t, timer_cb cb,
		void * args, int sec, int ms ) {
	int ret = 0;
	if( !t || !cb || ( !sec && !ms) ) {
		ret = -1;
		return ret;
	}
	__tq_del_timer(t);	/* detach without check */
	timer_set_value(t, cb, args, sec, ms);
	tq_insert_timer(t);
	return ret;
}

/* extern:  to alloc a free timer and init */
static struct timer * tq_alloc_timer(struct timer_queue * tq) {
	struct timer * ret = NULL;
	struct list_head * l;

	if( !list_empty(&tq->free_timer) ) {
		l = tq->free_timer.next;
		list_del(l);
		ret = to_timer(l, tq_node);
//		DIE( ret == NULL );
	} else {
		ret = malloc(sizeof(struct timer));
		if( !ret )
			puts("memory exhausted!");
		DIE( ret == NULL );
	}
	timer_init(ret, tq);

	return ret;
}

/**
 * @tq: 	timer_queue
 * @cb: 	callback function when expired
 * @args: 	args for this func
 * @sec: 	seconds delay
 * @ms:		microseconds delay ( at least 100ms )
 *
 * 	return a reference to authorized user.
 */
struct timer * tq_add_timer(struct timer_queue * tq, timer_cb cb,  void * args,
				int sec, int ms ) {
	struct timer * new;

	if( !tq || !cb || ( !sec && !ms) ) {
		printf("patameters illegal!\n");
		new = NULL;
		goto out;
	}
	new = tq_alloc_timer(tq);	/* timer->owner = tq */
	timer_set_value(new, cb, args, sec, ms);
	tq_insert_timer(new);
out:
	return new;
}

/*	FIXME： extern we should set a limit!!
 * 用户不会主动 malloc, 只会tq_add_timer() to get a reference
 *
 *	对于不是本线程的函数，不可以使用，因为其他线程只有一次性timer使用
 */
void tq_free_timer(struct timer * t) {
//	tq_del_timer(t); BUG
	list_add(&t->tq_node, &t->tq->free_timer);
}

/**
 *  the code become nice, when not support interval!
 *  FIXME: There's a special condition, when the deletion
 *  happends to the ealiest timer, it may do nothing,
 *  or the queue is empty! is's easy to modify.
 */
static void tq_do_expiration(struct timer_queue * tq ) {
	struct timespec now;
	struct timer * t = NULL;
	struct list_head * h;

	tq_debug_timerqueue(tq);
	clock_gettime(CLOCK_MONOTONIC, &now);
	while( !list_empty(&tq->single_timer_q) ) {
		h = tq->single_timer_q.next;
		t = to_timer(h, tq_node);
		if( timer_expired(&now, &t->expire) ) {
			list_del(h); /* BUG: POISON will be used for free_timer() */
			timer_handle_callback(t);
//			tq_free_timer(t);  who call the alloc will handle it!
		} else {
			break; /* FIXME : continue */
		}
	}

}

static void tq_read_timerfd(struct timer_queue * tq ) {
	ssize_t size;
	uint64_t count;
	int timerfd;
again:
	timerfd = tq->timefd;
	size = read(timerfd, &count, sizeof(uint64_t));
	if( size < 0) {
		if( errno == -EAGAIN || errno == -EWOULDBLOCK || errno == -EINVAL) {
			size = read(timerfd, &count, sizeof(eventfd_t)); /* try one more time*/
			pr_debug("%s: read fail and repeat!\n", __FUNCTION__);
			goto again;
		}
	}
	pr_debug("alarmed %lu times since last expiration\n", count);

}

/**
 * timefd is written by OS, now it's time to handle the
 * expired timer.	timer_queue is not need to be locked,
 * its owner is eventloop, others cannot  run.
 *
 * DONT SUPPORT INTERVAL(CONTINUAL) TIMER!!
 */
void tq_callback_for_channel(struct channel * c ) {
	struct timer_queue * tq;

	tq = to_timerqueue(c, timer_ch);
	tq_read_timerfd(tq);
	tq_do_expiration(tq);	/* handle expired timer(s) */
	tq_update_expiration(tq);
}

static void tq_free_list(struct timer_queue * tq ) {
	struct timer * t = NULL;
	struct list_head * h;

	/* timer cache */
	while( !list_empty(&tq->free_timer) ) {
		h = tq->free_timer.next;
		list_del(h);
		t = to_timer(h, tq_node);
		free(t);	/* fairwell! */
	}
	/* unhandled timers, the references held by the users..dont work! */
	while( !list_empty(&tq->single_timer_q) ) {
		h = tq->single_timer_q.next;
		list_del(h);
		t = to_timer(h, tq_node);
		free(t);	/* fairwell! */
	}
}

void timer_queue_destroy(struct timer_queue * tq) {
//	channel_disable_read(&tq->timer_ch);  /* SCRUTINY CHECK! */
	if( tq == NULL ) {
		puts("try to free a NULL timer_queue");
		return;
	}
//	channel_destroy(&tq->timer_ch);  poller will handle
	tq_free_list(tq);
	/* how to handle the unhandled timers? reference maybe held by users*/
	close(tq->timefd);
}

/**
 *  SCRUTINY CHECK!
 */
void timer_queue_init(struct timer_queue * tq, struct event_loop * el) {
	int timefd;

	timefd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);	/*  用 MONOTONIC 就 OK */
	if( timefd < 0 ) {
		perror("timerfd creation failed");
	}
	tq->timefd = timefd;
	channel_init(&tq->timer_ch, el, timefd, el->owner);
	channel_set_read_callback(&tq->timer_ch, tq_callback_for_channel);
	channel_enable_read(&tq->timer_ch);

	INIT_LIST_HEAD(&tq->free_timer);
//	INIT_LIST_HEAD(&tq->continual_time_q);
	INIT_LIST_HEAD(&tq->single_timer_q);
}
