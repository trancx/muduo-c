/*
 * vector.h
 *
 *  Created on: Apr 2, 2018
 *      Author: trance
 */

#ifndef INCLUDE_VECTOR_H_
#define INCLUDE_VECTOR_H_

#include <kernel.h>
#include <malloc.h>

#define VECTOR_BATCH 10


/* 初始化是什么样是什么type 不需要取地址，只是取出来的时候要
 * 从vector里面拿元素出来我们只用指针，这更有效率
 * 如果按照C++那种可以assignment，那么对于大结构就是
 * 浪费时间
 */

#define v_init(name, type) \
		vector_init(&name, sizeof(type))

#define VECTOR_DEFINE(type, name) \
	struct vector name; \
	v_init(name, type)


#define v_empty(name) \
	((&name)->count == 0)

/* NOTE: var must a local or (*pointer) */
#define v_push(name, var) \
		vector_pushback(&name, &var)

#define v_del(name, idx) \
		vector_del(&name, idx)

#define v_size(name) \
		vector_size(&name)

#define v_beg(name) \
		vector_beg(&name)

#define v_end(name) \
		vector_end(&name)

#define v_index(name, idx) \
		vector_index(&name, idx)

#define v_clear(name) \
		vector_clear(&name)

#define v_toarray(name) \
		vector_to_array(&name)

/* NOTE: name(struct vector) must a local var, we cannot free(&local)*/
#define v_free(name) \
		vector_free(&name); \

#define v_cpy(to,from) \
		vector_copy(&to, &from)

#define v_swap(lhs,rhs) \
		vector_swap(&lhs, &rhs)

#define for_each_vector(i,iter,name) \
	for(i=0,iter=vector_beg(&name); i < vector_size(&name); i++, iter++)

struct vector {
	void * ptr;
	size_t size;
	unsigned long count;
	unsigned long aval;
};

static inline void vector_init(struct vector * v, size_t size ) {
	v->ptr = NULL;
	v->size  = size;
	v->count = 0; /* index to the last one and counter */
	v->aval = 0;	/* left objs */
}


/* last index */
static inline void * vector_end(struct vector * v ) {
	size_t offset = 0;
	offset = (v->count -1) * v->size;
	return (void *)( (long)v->ptr + offset );
}

static inline void * vector_beg(struct vector * v ) {
	return v->ptr;
}

/* return vector[idx+1], USELESS.. */
static inline void * vector_next(struct vector * v, int idx ) {
	BUG_ON( idx > v->count );
	if( idx == v->count )
		return NULL;

	return (void *)((long)v->ptr + ++idx * v->size );
}

static inline int vector_size(struct vector * v ) {
	return v->count;
}


static inline void vector_clear(struct vector * v ) {
	v->aval = v->aval + v->count;
	v->count = 0;
}


static inline void vector_free(struct vector * v) {
	if( v->aval)
		free(v->ptr);
}

/* FIXME： add type detection */
static inline void * vector_to_array(struct vector * v ) {
	return v->ptr;
}

/* assumption: size is the same  FIXME: swap efficiency? */
static inline void vector_swap(struct vector * lhs, struct vector * rhs ) {
	unsigned long tmp;

	tmp = (unsigned long )lhs->ptr;
	lhs->ptr = rhs->ptr;
	rhs->ptr = (void *)tmp;

	tmp = lhs->count;
	lhs->count = rhs->count;
	rhs->count = tmp;

	tmp = lhs->aval;
	lhs->aval = rhs->aval;
	rhs->aval = tmp;
}

extern void vector_copy(struct vector * to, struct vector * from);

extern void vector_inc(struct vector * v);

extern void vector_pushback(struct vector * v, void * p );

/* swap the last one and delete */
extern void vector_del(struct vector * v, int idx );

extern void * vector_index(struct vector * v, int idx);

#endif /* INCLUDE_VECTOR_H_ */
