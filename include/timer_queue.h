/*
 * timer_queue.h
 *
 *  Created on: Apr 7, 2018
 *      Author: trance
 */

#ifndef INCLUDE_TIMER_QUEUE_H_
#define INCLUDE_TIMER_QUEUE_H_

#include <muduo.h>
#include <sys/timerfd.h>
#include <sys/time.h>
/** 			Main issues
 * 1.	timer 必须保证不能死递归
 * 2.	timer 属于一个Event loop
 * 3.	增加timer 那些函数应该在pending里面调用
 * 4.	而处理超时函数 必须由timer callback
 *    问题关键在于多次更新timer的时候 怎么设置timerfd
 *    即多次调用pending增加timer 应该保证timerfd的
 *    timerspec 保证最先到来的哪个时间 这样就可以保证
 *    及时性，然后每次更新下一个，当然我们处理timer就应该
 *    当前已经超时的处理了，不然设置的下一个时间是流逝的
 *    时间
 * 5. channel 的参数问题，callback可以得到 tq，判断时间
 * 	  可是插入timer,如果要求别的thread调用，就得使用pending
 * 	  那么pending其实是没有参数的，插入什么呢？
 *
 * 	  上层调用的时间函数，应该是 间隔，重复，callback_func
 */

typedef void (*timer_cb)(void * );

#define MINIMAL_INTERVAL 128 /* at least 128ms from 'now' */


/**			FIXME:
 * 因为添加的timer要求其他的线程也能添加，那么就要求使用pending func了
 * 那么pending_fun怎么才能到达这个timer呢？ 要求OOP， 那么必须得有个机制
 * 1.	使用全局变量，比如全局的等待加入的timer,那么就会用锁
 * 2.	更改pending_func为特别的一个node, 这样要求所有想实现pending都内嵌node
 * 3.	像boost:bind,function一样 ，所有的函数都包装成结构体，那么调用的时候就会有点奇怪。
 */
struct timer {
	struct timer_queue * tq;	/* my creator */
	timer_cb cb;
	void * arg;					/*  args for callback */
	struct timespec expire;	/* infomation */
	struct list_head tq_node; 	/* ndoe of timer queue */
};

extern void timer_queue_init(struct timer_queue * tq,
		struct event_loop * el);
extern void timer_queue_destroy(struct timer_queue * tq);

extern int tq_refresh_timer(struct timer * t, timer_cb cb,
		void * args, int sec, int ms );

/* attach it to the free list, when users nolonger need it  */
extern void tq_free_timer(struct timer * t);

/* interface to user */
extern struct timer * tq_add_timer(struct timer_queue * tq, timer_cb cb,
		void * args, int sec, int ms );


#endif /* INCLUDE_TIMER_QUEUE_H_ */
