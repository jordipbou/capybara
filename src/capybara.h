#include<stdlib.h>
#include<string.h>
#include<stddef.h>

#ifdef __linux__
#include<sys/mman.h>
#include<unistd.h>
#elif _WIN32
#include<windows.h>
#include<memoryapi.h>
#include<stdint.h>
#endif

// DATATYPES ------------------------------------------------------------------

#ifdef __linux__
typedef int8_t BYTE;
#endif
typedef int32_t HALF;
typedef int64_t CELL;

typedef struct _CTX CTX;
typedef void (*FUNC)(CTX*);

typedef struct _CTX {
	CELL dsize, csize;	// Data space size and code space size
	BYTE* dhere;				// Pointer to start of free memory on data segment
	BYTE* chere;				// Pointer to start of free memory on code segment
	BYTE* bottom;				// Lower address of data space free region (above header)
	FUNC* Fx;						// Virtual register with address of function to call from C 
	CELL Lx;						// Virtual register for passing literal values to C
	BYTE* code;					// Pointer to code space
} CTX;

// CONTEXT CREATION AND DESTRUCTION -------------------------------------------

#define ALIGN(addr, bound)	((CELL)addr + ((CELL)bound-1)) & ~((CELL)bound-1)

CELL pagesize() {
#if __linux__
	CELL PAGESIZE = sysconf(_SC_PAGESIZE);
#elif _WIN32
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	CELL PAGESIZE = si.dwPageSize;
#endif
	return PAGESIZE;
}

// Initializes two memory blocks, one as read-write data space and another
// one as read-exec for code space.
// Returns a pointer to a context structure with access to both blocks.
// Both blocks will be pagesize aligned.
// Free data space pointer will be aligned to first 2*sizeof(CELL) address
// above context address + sizeof(CTX);

CTX* init(CELL dsize, CELL csize) {
	CELL PAGESIZE = pagesize();

	dsize = ALIGN(dsize, PAGESIZE);
	csize = ALIGN(csize, PAGESIZE);

#if __linux__
	CTX* ctx = mmap(NULL, dsize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ctx == (void*)-1) return NULL;
#elif _WIN32
	CTX *ctx = VirtualAlloc(NULL, dsize, MEM_COMMIT, PAGE_READWRITE);
	if (!ctx) return NULL;
#endif

	ctx->dsize = dsize;
	ctx->csize = csize;
	ctx->Fx = NULL;
	ctx->Lx = 0;

#if __linux__
	ctx->code = mmap(NULL, csize, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ctx->code == (void*)-1) {
		free(ctx);
		return NULL;
	}
#elif _WIN32
	ctx->code = VirtualAlloc(NULL, csize, MEM_COMMIT, PAGE_EXECUTE_READ);
	if (!ctx->code) {
		free(ctx);
		return NULL;
	}
#endif

	// Start of free memory on data space is cell aligned
	ctx->dhere = (BYTE*)((CELL)ctx + ALIGN(sizeof(CTX), 2*sizeof(CELL)));
	ctx->bottom = ctx->dhere;
	ctx->chere = ctx->code;

	return ctx;
}

void deinit(CTX* ctx) {
#if __linux__
	munmap(ctx->code, ctx->csize);
	munmap(ctx, ctx->dsize);
#elif _WIN32
	VirtualFree(ctx->code, 0, MEM_RELEASE);
	VirtualFree(ctx, 0, MEM_RELEASE);
#endif
}

// COMPILATION ----------------------------------------------------------------

// Sets code space protection to WRITE to allow compilation.

BYTE* unprotect(CTX* ctx) {
#if __linux__
	if (!mprotect(ctx->code, ctx->csize, PROT_WRITE)) 
		return ctx->chere;
	else 
		return NULL; 
#elif _WIN32
	DWORD oldprot;
	if (VirtualProtect(ctx->code, ctx->csize, PAGE_READWRITE, &oldprot)) 
		return ctx->chere;
	else 
		return NULL;
#endif
}

// Sets code space protection back to READ_EXEC for execution.

BYTE* protect(CTX* ctx, BYTE* old_chere) {
#if __linux__
	if (!mprotect(ctx->code, ctx->csize, PROT_READ|PROT_EXEC)) 
		return ctx->chere;
	else {
		ctx->chere = old_chere;
		return NULL;
	}
#elif _WIN32
	DWORD oldprot;
	if (VirtualProtect(ctx->code, ctx->csize, PAGE_EXECUTE_READ, &oldprot)) 
		return ctx->chere;
	else {
		ctx->chere = old_chere;
		return NULL;
	}
#endif
}

// Compile helpers

CELL compile_byte(CTX* ctx, BYTE lit) {
	*(ctx->chere) = lit;
	ctx->chere++;
	return 1;
}

CELL compile_half(CTX* ctx, HALF lit) {
	*((HALF*)(ctx->chere)) = lit;
	ctx->chere += sizeof(HALF);
	return sizeof(HALF);
}

CELL compile_cell(CTX* ctx, CELL lit) {
	*((CELL*)(ctx->chere)) = lit;
	ctx->chere += sizeof(CELL);
	return sizeof(CELL);
}

CELL compile_bytes(CTX* ctx, BYTE* bytes, CELL len) {
	memcpy(ctx->chere, bytes, len);
	ctx->chere += len;
	return len;
}

CELL compile_next(CTX* ctx) {
	// 0:  48 8d 81 <H:Address after ret>	lea    rax,[rcx + <address after ret>]
	// 7:  c3															ret
	// 8 bytes
	CELL bytes = compile_bytes(ctx, "\x48\x8D\x81", 3);
	bytes += compile_half(ctx, ctx->chere - ctx->code + 5);
	bytes += compile_byte(ctx, 0xC3);
	return bytes;
}

CELL compile_reg(CTX* ctx, CELL lit, BYTE offset) {
	// 0:  49 ba <C:Address of cfunc>			movabs r10,0xff00ff11ff22ff33
	// a:  4c 89 52 <B:Fx offset>					mov    Q_WORD PTR [rdx+<Fx offset>],r10
	// 14 bytes
	CELL bytes = compile_bytes(ctx, "\x49\xBA", 2);
	bytes += compile_cell(ctx, lit);
	bytes += compile_bytes(ctx, "\x4C\x89\x52", 3);
	bytes += compile_byte(ctx, offset);
	return bytes;
}

#define compile_cfunc(ctx, cfunc)	compile_reg(ctx, (CELL)cfunc, offsetof(CTX, Fx))
#define compile_push(ctx, lit)		compile_reg(ctx, lit, offsetof(CTX, Lx))

// RDI: Not used as windows only accepts 4 parameters on registers
// RSI: Not used as windows only accepts 4 parameters on registers
// RDX: Pointer to context
// RCX: Pointer to code space
// R8: Reserved
// R9: Reserved
#ifdef __linux__
#define CALL(f, ctx)\
	((BYTE* (*)(void*, void*, CTX*, BYTE*, void*, void*))(f))\
		(NULL, NULL, ctx, ctx->code, NULL, NULL)
#elif _WIN32
#define CALL(f, ctx)\
	((BYTE* (*)(BYTE*, CTX*, void*, void*))(f))\
		(ctx->code, ctx, NULL, NULL)
#endif

// MEMORY MANAGEMENT ----------------------------------------------------------

BYTE* top(CTX* ctx) { return ((BYTE*)ctx) + ctx->dsize; }
CELL available(CTX* ctx) { return top(ctx) - ctx->dhere; }

void allot(CTX* ctx, CELL bytes) {
	if (bytes < 0) {
		if ((ctx->dhere + bytes) > ctx->bottom) ctx->dhere += bytes;
		else ctx->dhere = ctx->bottom;
	} else if (bytes > 0) {
		if (ctx->dhere + bytes < top(ctx)) ctx->dhere += bytes;
		else { /* Not enough memory error */ }
	}
}
