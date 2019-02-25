/*
 * server.h
 *
 *  Created on: Apr 29, 2018
 *      Author: trance
 */

#ifndef SRC_SERVER_H_
#define SRC_SERVER_H_

#include <muduo.h>
#include <sys/socket.h>

/* transmit controll block */
struct tcb {
	char * t_buf;
//	char * t_pos;  == buf + beg, so deprecated

	size_t t_beg;
	size_t t_end;

	size_t t_size;
	size_t t_left;
};

struct connection;

typedef void (*c_close_cb)(int fd, struct connection * c);
typedef void (*c_read_cb)(int fd, struct connection * c, void * buf, size_t len);
typedef void (*c_error_cb)(int fd, struct connection * c, int error );
//typedef void (*c_write_cb)(int fd, struct connection * c);

/* keep track of the buffer */
struct connection {
	struct server * s;
	struct channel c;
	c_read_cb read_cb;
	c_close_cb close_cb;
	c_error_cb error_cb;

	struct tcb c_tcb;

	void * private;	 /* reserve for user, anything */
};

extern void connnection_write(struct connection * c, void * buf, size_t size );
extern void connnection_close(struct connection * conn);

//typedef int  (*server_cb_t)(int fd, void *);
typedef void (*s_close_cb)(int fd, struct connection * c);
typedef void (*s_read_cb)(int fd, struct connection * c, void * buf, size_t len);
typedef void (*s_error_cb)(int fd, struct connection * c, int error );
typedef void (*s_conn_cb)(int fd, struct connection * c, struct sockaddr * sa, socklen_t addrlen );

struct server {
	int s_fd;
//	struct addr s_addr;
	struct event_loop * el;
	struct channel listener;

	/* back_up */
	s_read_cb read_cb;
	s_close_cb close_cb;
	s_error_cb error_cb;

	s_conn_cb conn_cb;
	struct addrinfo * s_info;
};

extern void server_init(struct server * s, const char * port, int family );
extern void server_set_conn_cb(struct server * s, s_conn_cb cb);

extern void server_set_conn_cb(struct server * s, s_conn_cb cb);
extern void server_set_close_cb(struct server *s, s_close_cb cb );
extern void server_set_read_cb(struct server * s, s_read_cb cb );
extern void server_set_errer_cb( struct server * s, s_error_cb cb );

extern int server_run(struct server * s);


//struct sockaddr_storage;

typedef struct ipaddr {
	sa_family_t family;
} addr_t;


#endif /* SRC_SERVER_H_ */
