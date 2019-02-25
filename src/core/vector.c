/*
 * 	simple vector implementation in C
 *
 *	linux 库定义的帮助宏喜欢一开始是指针，为了使vector看起来像C++
 *	我就定义成local var, 然后默认取址，正在考虑要不要全部换成
 *	linux那种
 *
 *	另外不可能全部定义成static inline的，因为memcpy在，不过好像
 *	也没有什么大问题， 恩，这就是其中的两个问题
 *
 *	重大问题： 没有类型检查！ 必须加上！ FIXME： ！！
 */


#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel.h>
#include <vector.h>



void vector_inc(struct vector * v) {
	v->ptr = realloc(v->ptr, (VECTOR_BATCH + v->count) * v->size);
	/* If you pass a null pointer for ptr, realloc behaves just like malloc (newsize). */
	if( v->ptr == NULL ) {
		perror("memory exhausted!\n");
		exit(-1);
	}
	if( v->count < VECTOR_BATCH )
		v->aval += VECTOR_BATCH - 1; /* FIXME: 最后一个作为交换缓存区 reverse, swap 操作，但是不打算实现。。*/
	else
		v->aval += VECTOR_BATCH;  /* 第(n>1)次填充 */
}

void * vector_index(struct vector * v, int idx ) {
	BUG_ON( idx > v->count );
	if( idx < 0 || !v->count) {
		perror("index lower than 0 or vector empty\n");
		return NULL;
	}
	pr_debug("v->aval = %lu, v->count = %lu\n", v->aval, v->count);
//	pr_debug("index = %d", idx);
	return (void *)((char *)v->ptr + idx * v->size);
}

void vector_pushback(struct vector * v, void * p ) {
	int pos = 0;
	void * ptr;
	if( !v->aval ) {
		vector_inc(v);
	}
	BUG_ON( v->aval <= 0 );
	pos = v->count * v->size;
	ptr = (void *)((char *)v->ptr + pos);
	memcpy(ptr, p, v->size);
	v->count++;
	v->aval--;
}

/* swap the last one and delete */
void vector_del(struct vector * v, int idx ) {
	void * from, * to;
	BUG_ON( idx > v->count || v->count <= 0);
	if( v->count > 1 ) {
		/* 如果是最后一个不用这么麻烦，直接更改计数 */
		from = (void *)( (long)v->ptr + (v->count - 1) * v->size );
		to = (void * )(  (long)v->ptr + idx * v->size);
		memcpy(to, from, v->size);
	}
	v->count--;
	v->aval++;
}

/* Insert 还是不支持了 这个操作根本是浪费CPU circle */

/* may not enough space for vector! but it's rare, just the first time */
 void vector_copy(struct vector * to, struct vector * from) {
	 BUG_ON( to->size != from->size );
	 int objs, aval;

repeat:
	 objs = from->count;
	 aval = to->aval;
	 BUG_ON( to->count != 0 ); /* we have to ensure it's empty */
	 if( objs <= aval ) {
		 memcpy(to->ptr, from->ptr, from->count * from->size);
	 } else {
		 vector_inc(to);
		 goto repeat;
	 }
	 to->count = from->count;
	 to->aval -= to->count;
	 BUG_ON( to->aval < 0 );
}
