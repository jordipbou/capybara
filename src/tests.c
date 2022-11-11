#include <stdlib.h>

#include "unity.h"
#include "capybara.h"

void setUp(void) {}

void tearDown(void) {}

void test_block_initialization(void) {
	CELL data_size = 1024;
	CELL code_size = 1024;

	CTX* ctx = init(data_size, code_size);
	TEST_ASSERT_NOT_NULL(ctx);

	TEST_ASSERT_GREATER_OR_EQUAL_INT(data_size, ctx->dsize);
	TEST_ASSERT_GREATER_OR_EQUAL_INT(code_size, ctx->csize);

	TEST_ASSERT_EQUAL_INT(0, ctx->Fx);
	TEST_ASSERT_EQUAL_INT(0, ctx->Lx);

	TEST_ASSERT_NOT_NULL(ctx->code);

	TEST_ASSERT_EQUAL_PTR(ctx, ALIGN(ctx, pagesize()));
	TEST_ASSERT_EQUAL_PTR(ctx->code, ALIGN(ctx->code, pagesize()));

	TEST_ASSERT_EQUAL_PTR(ctx->dhere, ALIGN(ctx->dhere, 2*sizeof(CELL)));
	
	deinit(ctx);
}

void test_compile_byte(void) {
	CTX* ctx = init(1024, 1024);

	TEST_ASSERT_EQUAL_INT(ctx->code, ctx->chere);

	BYTE* chere = unprotect(ctx);
	CELL bytes = compile_byte(ctx, 0xC3);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT8(0xC3, *(ctx->code));

	chere = unprotect(ctx);
	bytes += compile_byte(ctx, 0xB8);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT8(0xC3, *(ctx->code));
	TEST_ASSERT_EQUAL_INT8(0xB8, *(ctx->code + 1));

	deinit(ctx);
}

void test_compile_bytes(void) {
	CTX* ctx = init(1024, 1024);

	TEST_ASSERT_EQUAL_INT(ctx->code, ctx->chere);

	BYTE* chere = unprotect(ctx);
	CELL bytes = compile_bytes(ctx, "\xB8\x11\xFF\x00\xFF", 5);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT8(0xB8, *(ctx->code));
	TEST_ASSERT_EQUAL_INT8(0x11, *(ctx->code + 1));
	TEST_ASSERT_EQUAL_INT8(0xFF, *(ctx->code + 2));
	TEST_ASSERT_EQUAL_INT8(0x00, *(ctx->code + 3));
	TEST_ASSERT_EQUAL_INT8(0xFF, *(ctx->code + 4));

	unprotect(ctx);
	bytes += compile_bytes(ctx, "\xC7\x47", 2);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT8(0xC7, *(ctx->code + 5));
	TEST_ASSERT_EQUAL_INT8(0x47, *(ctx->code + 6));

	deinit(ctx);
}

void test_compile_literal(void) {
	CTX* ctx = init(1024, 1024);

	TEST_ASSERT_EQUAL_INT(ctx->code, ctx->chere);

	BYTE* chere = unprotect(ctx);
	CELL bytes = compile_cell(ctx, 41378);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT64(41378, *((CELL*)(ctx->code)));

	chere = unprotect(ctx);
	bytes += compile_cell(ctx, 13);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT64(13, *((CELL*)(ctx->code + sizeof(CELL))));

	chere = unprotect(ctx);
	bytes += compile_half(ctx, 19923);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT32(19923, *((HALF*)(ctx->code + 2*sizeof(CELL))));

	chere = unprotect(ctx);
	bytes += compile_half(ctx, -367);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	TEST_ASSERT_EQUAL_INT32(-367, *((HALF*)(ctx->code + 2*sizeof(CELL) + sizeof(HALF))));

	deinit(ctx);
}

void test_compile_next(void) {
	CTX* ctx = init(1024, 1024);
	
	TEST_ASSERT_EQUAL_INT(ctx->code, ctx->chere);

	BYTE* chere = unprotect(ctx);
	CELL bytes = compile_next(ctx);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	BYTE* NEXT = CALL(ctx->code, ctx);

	TEST_ASSERT_EQUAL_PTR(ctx->code + 8, NEXT);

	chere = unprotect(ctx);
	bytes += compile_next(ctx);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	NEXT = CALL(NEXT, ctx);

	TEST_ASSERT_EQUAL_PTR(ctx->code + 16, NEXT);

	deinit(ctx);
}

void generic_cfunc(CTX* ctx) {
}

void test_compile_cfunc(void) {
	CTX* ctx = init(1024, 1024);

	TEST_ASSERT_EQUAL_INT(ctx->code, ctx->chere);

	BYTE* chere = unprotect(ctx);
	CELL bytes = compile_cfunc(ctx, &generic_cfunc);
	bytes += compile_next(ctx);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	BYTE* NEXT = CALL(ctx->code, ctx);

	TEST_ASSERT_EQUAL_PTR(&generic_cfunc, ctx->Fx);
	TEST_ASSERT_EQUAL_INT(ctx->code + 22, NEXT);

	deinit(ctx);
}

void test_compile_push(void) {
	CTX* ctx = init(1024, 1024);

	TEST_ASSERT_EQUAL_INT(ctx->code, ctx->chere);

	TEST_ASSERT_EQUAL_INT(0, ctx->Lx);

	BYTE* chere = unprotect(ctx);
	CELL bytes = compile_push(ctx, 13);
	bytes += compile_next(ctx);
	protect(ctx, chere);

	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, ctx->chere);

	BYTE* NEXT = CALL(ctx->code, ctx);

	TEST_ASSERT_EQUAL_INT(13, ctx->Lx);
	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, NEXT);

	chere = unprotect(ctx);
	bytes += compile_push(ctx, 17);
	bytes += compile_next(ctx);
	protect(ctx, chere);

	NEXT = CALL(NEXT, ctx);

	TEST_ASSERT_EQUAL_INT(17, ctx->Lx);
	TEST_ASSERT_EQUAL_INT(ctx->code + bytes, NEXT);

	deinit(ctx);
}

void test_allot(void) {
	CTX* ctx = init(1024, 1024);

	TEST_ASSERT_EQUAL_PTR(ctx->bottom, ctx->dhere);

	CELL free = available(ctx);

	allot(ctx, 10);

	TEST_ASSERT_EQUAL_INT(free - 10, available(ctx));

	allot(ctx, -5);

	TEST_ASSERT_EQUAL_INT(free - 5, available(ctx));

	allot(ctx, -15);

	TEST_ASSERT_EQUAL_PTR(ctx->bottom, ctx->dhere);

	deinit(ctx);
}

void test_align(void) {
	CTX* ctx = init(1024, 1024);

	align(ctx);

	TEST_ASSERT_EQUAL_PTR(ctx->dhere, ALIGN(ctx->dhere, sizeof(CELL)));

	allot(ctx, 1);
	TEST_ASSERT_EQUAL_PTR(ctx->dhere + 7, ALIGN(ctx->dhere, sizeof(CELL)));

	align(ctx);
	TEST_ASSERT_EQUAL_PTR(ctx->dhere, ALIGN(ctx->dhere, sizeof(CELL)));
	
	deinit(ctx);
}

int main(void) {
	UNITY_BEGIN();

	RUN_TEST(test_block_initialization);

	RUN_TEST(test_compile_byte);
	RUN_TEST(test_compile_bytes);
	RUN_TEST(test_compile_literal);

	RUN_TEST(test_compile_next);
	RUN_TEST(test_compile_cfunc);
	RUN_TEST(test_compile_push);

	RUN_TEST(test_allot);
	RUN_TEST(test_align);

	return UNITY_END();
}
