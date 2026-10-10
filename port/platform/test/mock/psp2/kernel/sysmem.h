#ifndef ICO_TEST_PSP2_KERNEL_SYSMEM_H
#define ICO_TEST_PSP2_KERNEL_SYSMEM_H

#include <stddef.h>

typedef int SceUID;
typedef int SceSize;

#define SCE_KERNEL_MEMBLOCK_TYPE_USER_RW 0x0c20

SceUID sceKernelAllocMemBlock(const char *name, int type, SceSize size, const void *options);
int sceKernelGetMemBlockBase(SceUID uid, void **base);
int sceKernelFreeMemBlock(SceUID uid);

#endif
