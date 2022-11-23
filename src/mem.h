/*
	The following memory structure is defined:

	+===============+   -->   top (ctx + size)
	+       0       +
	+---------------+
	+      NEXT     +
	+===============+
	+       0       +
	+---------------+
	+  NEXT (fead)  +
	+===============+   -->   there (start of pairs region)
	+ | | | | | | | +
	+---------------+   -->   here (pointer to free contiguous space)
	+x|x|x|x|x|x|x|x+
	+===============+
	+x|x|x|x|x|x|x|x+
	+---------------+
	+x|x|x|x|x|x|x|x+
	+===============+   -->   bottom (end of header)
	+               +
	+     HEADER    +
	+               +
	+===============+   -->   context (address of block)

*/

#include<stdio.h>
#include<stdlib.h>
#include<stddef.h>
#include<stdint.h>

typedef int8_t BYTE;
typedef int64_t CELL;

typedef struct _PAIR {
	struct _PAIR* next;
	union {
		struct _PAIR* prev;
		CELL value;
	};
} PAIR;

typedef struct _CTX {
	// Must ensure that header is a multiple of pair size to maintain
	// pair alignment (2 CELLS, 4 CELLS, 6 CELLS, 8 CELLS...)
	CELL size;
	CELL flags;
	PAIR* dstack;
	PAIR* rstack;
	BYTE* bottom;
	BYTE* here;
	PAIR* there;
	PAIR* top;
} CTX;	

#define UNTYPED(addr)				(PAIR*)(((CELL)addr) & 0xFFFFFFFFFFFFFFF8L)
#define TYPE(pair)					((CELL)(pair->next) & 0x0000000000000007L)
#define SET_TYPE(pair, type)\
	(pair->next = (PAIR*)((((CELL)pair->next) & 0xFFFFFFFFFFFFFFF8L) | type))

#define TYPE_NIL						0
#define AS_NIL(addr)				((PAIR*)((((CELL)addr) & 0xFFFFFFFFFFFFFFF8L) | 0))
#define TYPE_NUMBER					1
#define AS_NUMBER(addr)			((PAIR*)((((CELL)addr) & 0xFFFFFFFFFFFFFFF8L) | 1))
#define TYPE_LIST						2
#define AS_LIST(addr)				((PAIR*)((((CELL)addr) & 0xFFFFFFFFFFFFFFF8L) | 2))
#define TYPE_FREE						7
#define AS_FREE(addr)				((PAIR*)((((CELL)addr) & 0xFFFFFFFFFFFFFFF8L) | 7))

#define NIL(ctx)						(ctx->top)
#define IS_NIL(pair)				(TYPE(pair) == TYPE_NIL)

#define ERR_STACK_UNDERFLOW		1
#define ERR_STACK_OVERFLOW		2

#define ALIGN(addr, bound)	(((CELL)addr + ((CELL)bound-1)) & ~((CELL)bound-1))

CTX* init(CELL size) {
	size = ALIGN(size, sizeof(PAIR));

	CTX* ctx = malloc(size);

	ctx->size = size;

	ctx->bottom = (((BYTE*)ctx) + sizeof(CTX));
	ctx->here = ctx->bottom;

	ctx->there = (PAIR*)ctx->here;
	ctx->top = (PAIR*)((((BYTE*)ctx) + size) - sizeof(PAIR));

	ctx->there->prev = ctx->there + 1;
	ctx->there->next = ctx->top;
	SET_TYPE(ctx->there, TYPE_FREE);

	ctx->top->prev = ctx->there;
	ctx->top->next = ctx->top - 1;
	SET_TYPE(ctx->top, TYPE_NIL);

	for (PAIR* p = ctx->there + 1; p < ctx->top; p++) {
		p->next = p - 1;
		p->prev = p + 1;
		SET_TYPE(p, TYPE_FREE);
	}

	ctx->dstack = ctx->rstack = NIL(ctx);

	return ctx;
}

void deinit(CTX* ctx) {
	free(ctx);
}

CELL length(CTX* ctx, PAIR* list) {
	for (CELL a = 0;; a++) { 
		if (IS_NIL(list)) {
			return a; 
		} else { 
			list = UNTYPED(list->next); 
		} 
	}
}

#define RESERVED(ctx)		(((BYTE*)(ctx->there)) - ctx->here)
#define UNUSED(ctx)			(((BYTE*)(ctx->top)) - ctx->here)
#define FREE_PAIRS(ctx)	(length(ctx, UNTYPED(ctx->top->next)))

void allot(CTX* ctx, CELL bytes) {
	if (bytes > 0) {
		while (RESERVED(ctx) < bytes) {
			PAIR* p = ctx->there;
			if (TYPE(p) == TYPE_FREE) {
				// Remove pair from free pairs list
				if (!IS_NIL(p->prev)) p->prev->next = p->next;
				if (!IS_NIL(p->next)) p->next->prev = p->prev;
				ctx->there++;
			} else {
				// ERROR: Not enough memory
				return;
			}
		}

		ctx->here += bytes;
	} else {
		// TODO: Add pairs to free list?
	}
}

void align(CTX* ctx) {
	BYTE* new_here = (BYTE*)(ALIGN(ctx->here, sizeof(CELL)));
	allot(ctx, new_here - ctx->here);
}

// STACK OPERATIONS

void push(CTX* ctx, CELL value) {
	// This should not be here:
	if (IS_NIL(ctx->top->next)) { 
		ctx->flags |= ERR_STACK_OVERFLOW;
		return;
	}
	PAIR* p = UNTYPED(ctx->top->next);
	ctx->top->next = AS_NIL(p->next);

	p->next = AS_NUMBER(ctx->dstack);
	ctx->dstack = p;
	p->value = value;	
}

CELL pop(CTX* ctx) {
	// This should not be here:
	if (ctx->dstack == NIL(ctx)) {
		ctx->flags |= ERR_STACK_UNDERFLOW;
		return 0;
	}
	PAIR* p = ctx->dstack;
	CELL value = p->value;
	ctx->dstack = UNTYPED(p->next);

	p->next = AS_FREE(ctx->top->next);
	p->prev = NIL(ctx);
	ctx->top->next = AS_NIL(p);

	return value;
}

// TESTING OPERATIONS

void dup(CTX* ctx) {
	CELL X = pop(ctx);
	push(ctx, X); push(ctx, X);
}

void gt(CTX* ctx) {
	CELL X = pop(ctx);
	CELL Y = pop(ctx);
	push(ctx, Y > X);
}

void dec(CTX* ctx) {
	CELL X = pop(ctx);
	push(ctx, X - 1);
}

void swap(CTX* ctx) {
	CELL X = pop(ctx);
	CELL Y = pop(ctx);
	push(ctx, X);
	push(ctx, Y);
}

void add(CTX* ctx) {
	CELL X = pop(ctx);
	CELL Y = pop(ctx);
	push(ctx, Y + X);
}
