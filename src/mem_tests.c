#include "mem.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

void test_init() {
	CELL size = 1024;
	CTX* ctx = init(size);	

	TEST_ASSERT_EQUAL_PTR(((BYTE*)ctx) + sizeof(CTX), ctx->bottom);
	TEST_ASSERT_EQUAL_PTR(size, ctx->size);
	TEST_ASSERT_EQUAL_PTR(ctx->bottom, ctx->here);
	TEST_ASSERT_EQUAL_PTR(ctx->bottom, ctx->there);
	TEST_ASSERT_EQUAL_PTR((PAIR*)(((BYTE*)ctx) + size - sizeof(PAIR)), ctx->top);

	TEST_ASSERT_EQUAL_INT(
		(size - sizeof(CTX) - sizeof(PAIR)) / sizeof(PAIR),
		FREE_PAIRS(ctx));

	deinit(ctx);
}

void test_types() {
	CELL size = 1024;
	CTX* ctx = init(size);

	TEST_ASSERT_EQUAL_INT(TYPE_NIL, TYPE(ctx->top));
	TEST_ASSERT_EQUAL_INT(TYPE_FREE, TYPE(ctx->top->next));

	ctx->top->next->next = AS_NUMBER(ctx->top->next->next);
	TEST_ASSERT_EQUAL_INT(TYPE_NUMBER, TYPE(ctx->top->next));
	TEST_ASSERT_EQUAL_INT(TYPE_NIL, TYPE(ctx->top));

	ctx->top->next->next = AS_LIST(ctx->top->next->next);
	TEST_ASSERT_EQUAL_INT(TYPE_LIST, TYPE(ctx->top->next));
	TEST_ASSERT_EQUAL_INT(TYPE_NIL, TYPE(ctx->top));

	ctx->top->next->next = AS_FREE(ctx->top->next->next);
	TEST_ASSERT_EQUAL_INT(TYPE_FREE, TYPE(ctx->top->next));
	TEST_ASSERT_EQUAL_INT(TYPE_NIL, TYPE(ctx->top));

	deinit(ctx);
}

void test_allot() {
	CELL size = 128;
	CTX* ctx = init(size);

	TEST_ASSERT_EQUAL_INT(((BYTE*)(ctx->top) - (BYTE*)(ctx->here)), UNUSED(ctx));
	TEST_ASSERT_EQUAL_INT(0, RESERVED(ctx));

	CELL free_pairs = FREE_PAIRS(ctx);

	allot(ctx, 2);

	TEST_ASSERT_EQUAL_INT(14, RESERVED(ctx));
	TEST_ASSERT_EQUAL_INT(free_pairs - 1, FREE_PAIRS(ctx));
	TEST_ASSERT_EQUAL_PTR(ctx->here + 14, (BYTE*)(ctx->there));

	allot(ctx, 10);

	TEST_ASSERT_EQUAL_INT(4, RESERVED(ctx));
	TEST_ASSERT_EQUAL_INT(free_pairs - 1, FREE_PAIRS(ctx));
	TEST_ASSERT_EQUAL_PTR(ctx->here + 4, (BYTE*)(ctx->there));

	allot(ctx, 5);

	TEST_ASSERT_EQUAL_INT(15, RESERVED(ctx));
	TEST_ASSERT_EQUAL_INT(free_pairs - 2, FREE_PAIRS(ctx));
	TEST_ASSERT_EQUAL_PTR(ctx->here + 15, (BYTE*)(ctx->there));

	deinit(ctx);
}

void test_stack() {
	CTX* ctx = init(1024);

	CELL free_pairs = FREE_PAIRS(ctx);

	for (CELL i = 0; i < free_pairs; i++) {
		TEST_ASSERT_EQUAL_INT(free_pairs - i, FREE_PAIRS(ctx));
		push(ctx, i);
		TEST_ASSERT_EQUAL_INT(0, ctx->flags & ERR_STACK_OVERFLOW);
	}

	for (CELL i = free_pairs; i > 0; i--) {
		TEST_ASSERT_EQUAL_INT(free_pairs - i, FREE_PAIRS(ctx));
		pop(ctx);
		TEST_ASSERT_EQUAL_INT(0, ctx->flags & ERR_STACK_UNDERFLOW);
	}

	deinit(ctx);
}

void fib(CTX* ctx) {
	dup(ctx);
	push(ctx, 1);
	gt(ctx);
	if (pop(ctx)) {
		dec(ctx);
		dup(ctx);
		dec(ctx);
		fib(ctx);
		swap(ctx);
		fib(ctx);
		add(ctx);
	}
}

void test_fib() {
	CTX* ctx = init(4096);

	push(ctx, 36);

	fib(ctx);

	TEST_ASSERT_EQUAL_INT(14930352, pop(ctx));

	deinit(ctx);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_init);

	RUN_TEST(test_types);

	RUN_TEST(test_allot);

	RUN_TEST(test_stack);

	RUN_TEST(test_fib);

	return UNITY_END();
}
