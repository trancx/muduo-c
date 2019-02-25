/*
 * main.c
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */

#include <sys/types.h>
#include <sys/poll.h>
#include <string.h>
#include <eventloop.h>
#include <channel.h>
#include <timer_queue.h>
#include <stdarg.h>
#include <stdio.h>

struct event_loop * lop;
int fd;
char buf[50];


void test_call_back(struct channel * c) {
	puts("channel call back~~");
	size_t size = read(fd, buf, 50);
	while( size  == 50 ) {
		size = read(fd,buf, 50);
	}
}

void  test_pending(void * arg) {
	puts("WARN:: pending func");
}

void thread_fun2(void * arg) {
	el_add_pending_functions(lop, test_pending, NULL);
}

void test_timer_cb(void * args) {
	int * t;
	t = args;
	printf("it's timer %d\n", *t);
}

int argint = 233;

void test_exit(void * arg) {
	el_exit(lop);
}

void thread_fun(void * arg) {
//	pthread_t child;
	el_add_pending_functions(lop, test_pending, NULL);
	el_run_after(lop, test_timer_cb, &argint, 1000);
	el_run_after(lop, test_exit, NULL, 5000 * 2);
//	sleep(4);
//	pthread_create(&child, NULL, (void *)thread_fun2, NULL);
	sleep(6);
//	el_exit(lop);
//	pthread_join(child, NULL);
}



#include <sys/timerfd.h>

/**
 * pthread_self() 返回的就是 pthread_t
 * 跟linux getpid（） 不一样
 * 但是它们都是在线程里面唯一的
 *
 *
  	printf("main pid: %u\n", gettid());
	printf("main pthread self %lu\n", pthread_self());
	printf("thread pthread_t %lu\n", pid);

	Finally we adopt pthread_t the return value by pthread_self()

 */


#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define LOVELY_PORT "3268"

#include "server.h"

char * messge =
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n"
		"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\n";

void myconnect(int fd, struct connection * c,
		struct sockaddr * sa, socklen_t addrlen ) {

	char clienthost[NI_MAXHOST] = {0,};
	char clientservice[NI_MAXSERV] = {0, };

	getnameinfo(sa, addrlen,
	       clienthost, sizeof(clienthost),
	       clientservice, sizeof(clientservice),
	       NI_NUMERICHOST);

	printf("Received request from host=[%s] port=[%s]\n",
		   clienthost, clientservice);

	connection_write(c, messge, strlen(messge));
}


void when_close(int fd, struct connection * c) {

	printf("OOPS, fd %d the connnection is cut!\n", fd);

}

/* pending func */
int main(int argc, char * argv[]) {
	int ret = 0;
	struct server s;
	pthread_t pid;
	server_init(&s, LOVELY_PORT, AF_UNSPEC);
	server_set_conn_cb(&s, myconnect);
	server_set_close_cb(&s, when_close);

	lop = s.el;

	pthread_create(&pid, NULL, (void *)thread_fun, NULL);
	/* 0: default length */
	server_run(&s);

	pthread_join(pid, NULL);
//	event_loop(s.el);
	event_loop_destroy(lop);
	return ret;
}

//int main( int argc, char * argv[] ) {
//	int ret = 0;
//
//	struct event_loop el;
////	struct channel c;
//	pthread_t id, pid;
//	struct timer_queue * tq;
//	struct timer * pool[4];
//	int args[4] = { 1,2,3,4};
////	fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
//	lop = &el;
//	id = pthread_self();
//	event_loop_init(&el, id);
//
//	tq = el.tq;
//	int i;
//	for(i=0; i < 4; i++ ) {
//		pool[i] = tq_add_timer(tq, test_timer_cb, &args[i], 5 , 100 + i*100 );
//	}
////	while(1);
////	channel_init(&c, &el, fd, id);
////	channel_set_read_callback(&c, test_call_back);
////	channel_enable_read(&c);
//
//	pthread_create(&pid, NULL, (void *)thread_fun, NULL);
//	tq_refresh_timer(pool[0], test_timer_cb, &args[0], 2, 0 );
//	tq_refresh_timer(pool[1], test_timer_cb, &args[1], 1, 0 );
//	tq_refresh_timer(pool[2], test_timer_cb, &args[2], 1, 500 );
////	tq_free_timer(pool[1]);
////	while(1);
////	struct itimerspec howlong;
////	memset(&howlong, 0, sizeof howlong);
////	howlong.it_value.tv_sec = 2;
////	howlong.it_interval.tv_sec = 2;
////	timerfd_settime(fd, 0, &howlong, NULL);
//
//	event_loop(&el);
//
//	pthread_join(pid, NULL);
//	event_loop_destroy(&el);
////	close(fd);
//
//
////	VECTOR_DEFINE(struct pollfd *, test);
////	VECTOR_DEFINE(struct pollfd *, test2);
////	struct pollfd ** p;
////	int i;
////
////	struct pollfd * p1;
////	struct pollfd * p2;
////	struct pollfd * p3;
////
////	p1 = p2 = p3 = NULL;
////	p2 = (void *)0xccc00;
////	p3 = (void *)0x12345;
////	v_push(test, p1);
////	v_push(test, p2);
////	v_push(test, p3);
////
////	v_swap(test2,test);
////	for_each_vector(i,p,test2) {
////		printf("element %d: 0x%016x\n", i, *p);
////	}
////
////	v_free(test);
////	v_free(test2);
//
//	return ret;
//}



//int FindMax (int n, ...)
//{
//  int i,val,largest = 0;
//  va_list vl;
//  va_start(vl,n);
////  largest=va_arg(vl,int);
//  for (i=0;i<n;i++)
//  {
//    val=va_arg(vl,int);
//    printf("%d ", val);
//  }
//  puts("");
//  va_end(vl);
//  return largest;
//}
//
//int main ()
//{
//  int m;
//  m= FindMax (7,702,422,631,834,892,104,772);
//  printf ("The largest value is: %d\n",m);
//  return 0;
//}
