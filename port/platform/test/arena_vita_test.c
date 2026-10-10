/*
 * Exercise arena.c's Vita memblock path on the host using a tiny mock of the
 * VitaSDK sysmem API. This checks failure recovery as well as the allocation
 * contract without claiming that the Vita kernel itself was exercised.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include <psp2/kernel/sysmem.h>

static void *mock_block;
static size_t mock_size;
static int fail_alloc_once;
static int fail_base_once;
static int alloc_calls;
static int base_calls;
static int free_calls;

SceUID sceKernelAllocMemBlock(const char *name, int type, SceSize size, const void *options)
{
    (void)name;
    (void)options;
    if (type != SCE_KERNEL_MEMBLOCK_TYPE_USER_RW || size <= 0) return -1;
    alloc_calls++;
    if (fail_alloc_once) {
        fail_alloc_once = 0;
        return -12;
    }
    mock_block = malloc((size_t)size);
    if (mock_block == NULL) return -12;
    mock_size = (size_t)size;
    memset(mock_block, 0xA5, mock_size);
    return 7;
}

int sceKernelGetMemBlockBase(SceUID uid, void **base)
{
    base_calls++;
    if (uid != 7 || fail_base_once) {
        fail_base_once = 0;
        return -1;
    }
    *base = mock_block;
    return 0;
}

int sceKernelFreeMemBlock(SceUID uid)
{
    if (uid != 7 || mock_block == NULL) return -1;
    free(mock_block);
    mock_block = NULL;
    mock_size = 0;
    free_calls++;
    return 0;
}

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

int main(int argc, char **argv)
{
    int failures = 0;
    unsigned char *base;
    size_t i;

    if (argc != 2) {
        fprintf(stderr, "usage: %s success|alloc-fail|base-fail\n", argv[0]);
        return 2;
    }
    CHECK(ico_arena_base() == NULL);
    if (strcmp(argv[1], "alloc-fail") == 0) {
        fail_alloc_once = 1;
        CHECK(ico_arena_init() == -1);
        CHECK(ico_arena_base() == NULL && ico_arena_size() == 0);
        CHECK(alloc_calls == 1 && base_calls == 0 && free_calls == 0);
        CHECK(ico_arena_init() == 0);
        CHECK(alloc_calls == 2 && base_calls == 1);
    } else if (strcmp(argv[1], "base-fail") == 0) {
        fail_base_once = 1;
        CHECK(ico_arena_init() == -1);
        CHECK(ico_arena_base() == NULL && ico_arena_size() == 0);
        CHECK(alloc_calls == 1 && base_calls == 1 && free_calls == 1);
        CHECK(mock_block == NULL);
        CHECK(ico_arena_init() == 0);
        CHECK(alloc_calls == 2 && base_calls == 2);
    } else if (strcmp(argv[1], "success") == 0) {
        CHECK(ico_arena_init() == 0);
        CHECK(ico_arena_init() == 0);
        CHECK(alloc_calls == 1 && base_calls == 1);
    } else {
        fprintf(stderr, "unknown test mode: %s\n", argv[1]);
        return 2;
    }

    base = ico_arena_base();
    CHECK(base != NULL);
    CHECK(((uintptr_t)base & (ICO_ARENA_ALIGN - 1)) == 0);
    CHECK(ico_arena_size() == ICO_ARENA_EE_SIZE + ICO_ARENA_HEADROOM);
    for (i = 0; i < ico_arena_size(); i += 4096) {
        if (base[i] != 0) {
            fprintf(stderr, "FAIL arena not zero at offset %lu\n", (unsigned long)i);
            failures++;
            break;
        }
    }
    CHECK(ico_arena_contains(base, 1));
    CHECK(ico_arena_ee_addr(0x760000) == (uintptr_t)base + 0x760000);
    CHECK(mock_size > ico_arena_size());

    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("Vita arena mock (%s): all checks passed\n", argv[1]);
    return 0;
}
