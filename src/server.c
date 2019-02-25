/*
 * server.c
 *
 *  Created on: Apr 22, 2018
 *      Author: trance
 */
#include <sys/un.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
extern int accept4 (int __fd, __SOCKADDR_ARG __addr,
		    socklen_t *__restrict __addr_len, int __flags);

#include <netdb.h>
#include <string.h>

#include <channel.h>
#include <kernel.h>


#include "server.h"

/**
 * tcb的目标是多页管理，演变成一个好点的buffer管理器！
 * 可以考虑radix-tree
 *
 */

#define TCB_GROW_BATCH 1
#define PAGE_SIZE	(1UL << 12)

void tcb_init(struct tcb * t) {
	t->t_buf = NULL;
#ifdef CONFIG_DEBUG_TCB
	t->t_beg = PAGE_SIZE - 1;
	t->t_end = t->t_beg;
#else
	t->t_beg = 0;
	t->t_end = 0;
#endif
	t->t_left = 0;
	t->t_size = 0;
}

void tcb_destroy(struct tcb * t ) {
	free(t->t_buf);
}


int tcb_grow(struct tcb * t ) {
	size_t now;

	now = t->t_size + TCB_GROW_BATCH * PAGE_SIZE;

	t->t_buf = realloc(t->t_buf, now);
	t->t_size = now;
	t->t_left += TCB_GROW_BATCH * PAGE_SIZE;

	return 0;
}

/* 这个函数必须是在 end != size 的情况下调用的, USELESS  */
ssize_t tcb_extract(struct tcb * tcb, void * to, size_t size ) {
	void * from;
	size_t beg, end;
	size_t batch;

again:
	beg = tcb->t_beg;
	end = tcb->t_end;
	from = tcb->t_buf + beg;

	if( beg < end )
		batch = end - beg;
	else
		batch = tcb->t_size - beg;

	if( batch > size )
		batch = size;

	memcpy(to, from, batch);
	size -= batch;
	to = (char * )to + batch;
	tcb->t_left += batch;
	tcb->t_beg = (beg + batch ) & ( tcb->t_size - 1 );
	if( size != 0 )
		goto again;

	return 0;
}

/* 这个函数必须是保证 left >= size 才调用！  由 conn_copy() 使用   */
ssize_t tcb_copy(struct tcb * tcb, void * from, size_t size ) {
	void * to;
	size_t beg, end;
	size_t batch;

again:
	beg = tcb->t_beg;
	end = tcb->t_end;
	to = tcb->t_buf + end; /* assume address is consecutive  */

	if( beg <= end )	/* maybe empty */
		batch = tcb->t_size - end;
	else
		batch = beg - end;


	if( batch > size )
		batch = size;
	memcpy(to, from, batch);
	tcb->t_end = (end + batch) & ( tcb->t_size - 1 );
	tcb->t_left -= batch;
	size -= batch;
	from = (char *)from + batch;

	if( size != 0 )
		goto again;

	return 0;
}

void conn_channel_write_cb(struct channel * c );

/**
 *  Buffer there!, save the buf to this connector
 */
static ssize_t conn_save(struct connection * c, void * buf, size_t size ) {
	size_t space;
	struct tcb * t;
	int ret;

again:
	t = &c->c_tcb;
	space = t->t_left;
	if( space >= size ) {
		ret = tcb_copy(t, buf, size);
	} else {
		if( !tcb_grow(t) )
			goto again;
		else
			ERROR("tcb_grow() has encountered a problem!\n");
	}

	return (ssize_t)ret;
}


/**
 * 处理channel, 处理tcb, FIXME: connection 怎么free.
 */
void conn_destroy(struct connection * c ) {
//	channel_destroy(&c->c); no need!
	tcb_destroy(&c->c_tcb);
}

/* CONNECTION API */
void connection_write(struct connection * c, void * buf, size_t size ) {
	int fd;
	ssize_t ret = 0;

	fd = c->c.fd;
//#ifndef CONFIG_DEBUG_TCB
	ret = write(fd, buf, size/6);
//#endif
	if( ret < size  ) {
//		pr_debug("SAVE buffer size: %l \n", size);
 		buf = (char *)buf + ret;
		size -= ret;
		/* now the buf -> buf + size shoule be saved*/
		conn_save(c, buf, size);
		channel_enable_write(&c->c);
	}
}

/* CONNECTION API */
int connection_close(struct connection * conn ) {
	struct channel * ch;
	int fd;

	if( conn == NULL ) {
		return 0;
	}
	ch = &conn->c;
	fd = ch->fd;

	channel_invalidate(ch);
	close(fd);
	conn_destroy(conn);

	return 0;
}

/**
 *  something similar to destroy a connection
 *
 *  FIXED: channel_destroy->call close_cb->HERE->conn_destroy
 *  ->channel_destroy???
 *
 *  conn 开头的意味着都是被动关闭， conn_destroy不会调用channel_destroy
 *  而以后的channel如果被动关闭，会自己关闭自己，close_cb只需要处理自己的事情
 */
static int conn_close(struct connection * c ) {
	int fd;
	struct channel * ch;

	ch = &c->c;
	fd = ch->fd;
	close(fd);
	if( c->close_cb )
		c->close_cb(fd, c);

	/* LOG_HERE */
	conn_destroy(c);

	return 0;
}

static int conn_error(struct connection * c ) {
	int fd;

	fd = c->c.fd;
	if( c->error_cb)
		c->error_cb(fd, c, -1);

	/*    handle ourselves  */
	/*    LOG_HERE 			*/

	conn_close(c);

	return 0;
}


#define to_connection(ptr, member) \
	container_of(ptr, struct connection, member)

/* callback handling functions, AWESOME CODE! */
void conn_channel_write_cb(struct channel * c ) {
	struct connection * conn = NULL;
	struct tcb * tcb;
	int fd;
	void * from;
	size_t beg, end;
	size_t batch,total;
	ssize_t ret;

	fd = c->fd;
	conn = to_connection(c, c);
	tcb = &conn->c_tcb;

	total = tcb->t_size - tcb->t_left;
	beg = tcb->t_beg;
	end = tcb->t_end;
again:
	from = tcb->t_buf + beg;

	if( beg < end )
		batch = end - beg;
	 else
		batch = tcb->t_size - beg;

#ifdef CONFIG_DEBUG_TCB
	printf("started a new transmit\n beg: %ld end: %ld  total: %ld\n", beg, end, total);
	if( batch > 500  )
		batch = 500;
#endif

 	ret = write(fd, from, batch); /* resend */
	pr_debug("%ld bytes have been send!\n", ret);

	if( ret < 0 ) {
		switch( errno ) {
			case EWOULDBLOCK:
				goto done;
				/* FIXME: 	 LOG_HERE  */
				break;
			default:
				/* log! */
				perror("write() error");
				conn_error(conn);
				return;
			/* kill everything! */
		}

	}
	total -= (size_t)ret;

	if( ret  ==  batch) {
		if( total == 0 ) {
			channel_disable_write(c);
			goto done;
		} else {
			beg = ( beg + batch ) & ( tcb->t_size - 1);
#ifdef CONFIG_DEBUG_TCB
			goto done;
#endif
			tcb->t_left += batch;
			goto again;
		}

	} else {
		/* ret < batch, wait to the next time!  */
		beg = ( beg + batch ) & ( tcb->t_size - 1);
	}

done:
	tcb->t_beg = beg;
	tcb->t_left += batch;
}

/**
 * when a channel destroy,
 *
 * channel->close_cb->HERE->close(fd)
 *
 */
void conn_channel_close_cb(struct channel * c ) {
	struct connection * conn = NULL;
	int fd;

	fd = c->fd;
	conn = to_connection(c, c);
	conn_close(conn);

	printf("channel is closed!\n");
}


/**
 * 		call read() and transfer to the user!
 *
 */
void conn_channel_read_cb(struct channel * c ) {
	struct connection * conn = NULL;
	int fd;
	void * buf;
	ssize_t ret;
	char test[50];


	fd = c->fd;
	conn = to_connection(c, c);

	memset(test, 0, 50);
	ret = read(fd, test, 50);
	if( ret < 0 ) {
		perror("connection error");
		conn_error(conn);
	}
	if( ret == 0 ) {
		/*  zero indicates end of file */
//		conn_close(conn);   BUG, we close it deliberately, sp
		connection_close(conn);
	}
}


/**
 * detect error, let user know
 */
void conn_channel_error_cb(struct channel * c ) {
	struct connection * conn = NULL;
	int fd;
	int err = -1;

	fd = c->fd;
	conn = to_connection(c, c);
	conn_error(conn);
}

void conn_init(struct connection * c, struct server * s,  int fd ) {
	struct channel * ch;

	ch = &c->c;
	c->s = s;

	channel_init(ch, s->el, fd, s->el->owner);
	channel_set_write_callback(ch, conn_channel_write_cb);
	channel_set_close_callback(ch, conn_channel_close_cb);
	channel_set_error_callback(ch, conn_channel_error_cb);
	channel_set_read_callback(ch, conn_channel_read_cb);
#ifdef CONFIG_DEBUG_POLLPRI
//	channel_enable_urgent(ch);
#endif
	channel_enable_read(ch);
	c->read_cb = s->read_cb;
	c->close_cb = s->close_cb;
	c->error_cb = s->error_cb;
	tcb_init(&c->c_tcb);
}

/**
 * 2018 6 1 16:59
 * 准备添加服务器监听一个端口的接口，返回一个监听可读的connection
 *
 */
const struct connection *  server_watch(struct server * s,
		const char * port, int famliy ) {
	struct connection * conn = NULL;


	return conn;
}



void server_set_conn_cb(struct server * s, s_conn_cb cb) {
	s->conn_cb = cb;
}

void server_set_close_cb(struct server *s, s_close_cb cb ) {
	s->close_cb = cb;
}

void server_set_read_cb(struct server * s, s_read_cb cb ) {
	s->read_cb = cb;
}

void server_set_errer_cb( struct server * s, s_error_cb cb ) {
	s->error_cb = cb;
}

#define to_server(ptr, member) \
	container_of(ptr, struct server, member)

/**
 *	it means we can connect to client! and call conn_cb;
 */
void server_channel_read_callback(struct channel * c ) {
	struct server * s = NULL;
	int fd;
	struct sockaddr_storage c_addr;
	socklen_t addr_len;
	struct connection * conn;

	s = to_server(c, listener);
	fd = accept4(s->s_fd, (struct sockaddr *)&c_addr,
			&addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if( fd < 0 ) {
		fprintf(stderr,
		       "socket error:: could not connect socket fd : %d\n", s->s_fd);
		perror("errno");
	} else {
		conn = malloc( sizeof(*conn) );
		conn_init(conn, s, fd);	/* gain its callbacks */

		/* new connection so we call back  */
		if( s->conn_cb ) {
			/* LOG_HERE */
			s->conn_cb(fd, conn, (struct sockaddr *)&c_addr, addr_len);
		}
		return;
	}
}

#define LISTEN_QUEUE 128

/**
 * 		listen and enable read!
 * @s: server can be NULL
 * @ch: cannot be NULL( and INITIALIZED! )
 * @queue_length: 0 == default
 */
int server_listen(struct server * s, struct channel * ch, int queue_length) {
	int ret = 0;
	int fd;

	fd = ch->fd;
	if( queue_length <= 0 )
		queue_length = LISTEN_QUEUE;
	ret = listen(fd, queue_length);
	if( ret < 0 ) {
		perror("listen() call error: ");
	}
	channel_set_read_callback(ch, server_channel_read_callback);
	channel_enable_read(ch);

	return ret;
}

/**
 *	Refer to Eva M. Castro<eva@gsyc.escet.urjc.es>
 *	http://long.ccaba.upc.edu/long/045Guidelines/eva/ipv6.html#structures
 */
int server_getfd(struct server * s, const char * port, int family ) {
	struct addrinfo hints, *res, *ressave;
	int n, sockfd;

	memset(&hints, 0, sizeof(struct addrinfo));
		/*
		AI_PASSIVE flag: the resulting address is used to bind
		to a socket for accepting incoming connections.
		So, when the hostname==NULL, getaddrinfo function will
		return one entry per allowed protocol family containing
		the unspecified address for that family.
		*/
	hints.ai_flags    = AI_PASSIVE;
	hints.ai_family   = family;
	hints.ai_socktype = SOCK_STREAM;

	n = getaddrinfo(NULL, port, &hints, &res);

	if (n <0) {
		fprintf(stderr,
				"getaddrinfo error:: [%s]\n",
				gai_strerror(n));
	}
	ressave = res;

	/*
	   Try open socket with each address getaddrinfo returned,
	   until getting a valid listening socket.
	 */
	sockfd = -1;
	while (res) {
		sockfd = socket(res->ai_family,
						res->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
						res->ai_protocol);

		if (!(sockfd < 0)) {
			if ( bind(sockfd, res->ai_addr, res->ai_addrlen) == 0 ) {
#ifdef CONFIG_DEBUG_SERVER
				char straddr[INET6_ADDRSTRLEN] = {0, };
				inet_ntop(res->ai_family, res->ai_addr, straddr, sizeof(straddr));
				printf("addr: %s\n", straddr);
#endif
				break;
			}

			close(sockfd);
			sockfd = -1;
		}
		res = res->ai_next;

	}

	if (sockfd < 0) {
			freeaddrinfo(ressave);
			fprintf(stderr,
					"socket error:: could not open socket\n");
			DIE("socket cannot open");
	} else {
		if( s ) {
			s->s_info = malloc(sizeof(*res));
//			if( ! s->s_info ) FIXME:
//				ERROR("memory exhausted!");
			memcpy(s->s_info, res, sizeof(*res));		/* save address info */
		}
	}

	return sockfd;
}

/**
 *	Refer to Eva M. Castro<eva@gsyc.escet.urjc.es>
 *	http://long.ccaba.upc.edu/long/045Guidelines/eva/ipv6.html#structures
 *
 *	2018 6-1, seperate the { listen, bind } for reuse!
 */
void server_init(struct server * s, const char * port, int family ) {
//	struct addrinfo hints, *res, *ressave;
//	int n, sockfd;
	pthread_t owner;

//	memset(&hints, 0, sizeof(struct addrinfo));
//	/*
//	AI_PASSIVE flag: the resulting address is used to bind
//	to a socket for accepting incoming connections.
//	So, when the hostname==NULL, getaddrinfo function will
//	return one entry per allowed protocol family containing
//	the unspecified address for that family.
//	*/
//	hints.ai_flags    = AI_PASSIVE;
//	hints.ai_family   = family;
//	hints.ai_socktype = SOCK_STREAM;
//
//	n = getaddrinfo(NULL, port, &hints, &res);
//
//	if (n <0) {
//		fprintf(stderr,
//				"getaddrinfo error:: [%s]\n",
//				gai_strerror(n));
//	}
//	ressave = res;
//
//	/*
//	   Try open socket with each address getaddrinfo returned,
//	   until getting a valid listening socket.
//	 */
//	sockfd = -1;
//	while (res) {
//		sockfd = socket(res->ai_family,
//						res->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC,
//						res->ai_protocol);
//
//		if (!(sockfd < 0)) {
//			if ( bind(sockfd, res->ai_addr, res->ai_addrlen) == 0 ) {
//#ifdef CONFIG_DEBUG_SERVER
//				char straddr[INET6_ADDRSTRLEN] = {0, };
//				inet_ntop(res->ai_family, res->ai_addr, straddr, sizeof(straddr));
//				printf("addr: %s\n", straddr);
//#endif
//				break;
//			}
//
//			close(sockfd);
//			sockfd = -1;
//		}
//		res = res->ai_next;
//
//	}
//
//	if (sockfd < 0) {
//	        freeaddrinfo(ressave);
//	        fprintf(stderr,
//	                "socket error:: could not open socket\n");
//	        DIE("socket cannot open");
//	} else {
//		s->s_info = malloc(sizeof(*res));
////		if( ! s->s_info ) FIXME:
////			ERROR("memory exhausted!");
//		memcpy(s->s_info, res, sizeof(*res));		/* save address info */
//	}

	s->s_fd = server_getfd(s, port, family);
	s->el = malloc(sizeof(struct event_loop));
	owner = pthread_self();
	event_loop_init(s->el,owner);
	channel_init(&s->listener, s->el, s->s_fd, owner);
}



/**
 *  Server->channel has no close call back, unlike Connection
 *  so we have to do the close ourself!
 *
 */
void server_destroy(struct server * s) {
	event_loop_destroy(s->el);



	close(s->s_fd);
}

int server_run(struct server * s ) {

	/* server_init 仅仅是bind了Fd，channel没有加入el 而此时真正使channel*/
	server_listen(s, &s->listener, 0);
	event_loop(s->el);

	return 0;
}


/**
 * 1. server_init(server * , struct addr *)

 * 2. server_set_callback_read/close/connect( server *, Callback * )
 * 3. server_add_timer(server *, int interval, int repeat, Callback * )
 * 4. *_write(int fd)
 * 5. *_enable_read(int fd)
 * 6. *_close(int fd)
 * 7. *_start(server *)
 *
 * 8. && server_set_fd_read_callback(int fd); // optional
 */
