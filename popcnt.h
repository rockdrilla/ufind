/* ulist: simple grow-only dynamic array
 *
 * refs:
 * - https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 * - https://gcc.gnu.org/onlinedocs/gcc/x86-Built-in-Functions.html
 * - https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
 *
 * SPDX-License-Identifier: Apache-2.0
 * (c) 2022, Konstantin Demin
 */

#ifndef POPCNT_H
#define POPCNT_H

#include <limits.h>
#include <stdlib.h>

/* ref:
 * - https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
 */
#define _POPCNT_R0(v)     ((v) - (((v) >> 1) & 0x55555555u))
#define _POPCNT_R1(v)     ((_POPCNT_R0(v) & 0x33333333u) + ((_POPCNT_R0(v) >> 2) & 0x33333333u))
#define _POPCNT_R2(v)     (((_POPCNT_R1(v) + ((_POPCNT_R1(v) >> 4) & 0xF0F0F0Fu)) * 0x1010101u) >> 24)
#define _POPCNT_MACRO(v)  _POPCNT_R2(v)

static inline int _popcnt_bithacks(size_t n)
{
	/* ref:
	 * - https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	 */

	static const size_t b0 = ULONG_MAX / 3;
	static const size_t b1 = ULONG_MAX / 15 * 3;
	static const size_t b2 = ULONG_MAX / 255 * 15;
	static const size_t b3 = ULONG_MAX / 255;

	n = n - ((n >> 1) & b0);
	n = (n & b1) + ((n >> 2) & b1);
	n = (n + (n >> 4)) & b2;
	n = (n * b3) >> (sizeof(size_t) - 1) * CHAR_BIT;

	return n;
}

#if __INTPTR_WIDTH__ == 64
  #define _ULIST_BUILTIN_POPCNT(n)  __builtin_popcountll(n)
#else
  #define _ULIST_BUILTIN_POPCNT(n)  __builtin_popcountl(n)
#endif

static int _popcnt(size_t n)
{
	/* ref:
	 * - https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
	 * - https://gcc.gnu.org/onlinedocs/gcc/x86-Built-in-Functions.html
	 */
#if __has_builtin(__builtin_cpu_init) && __has_builtin(__builtin_cpu_supports)
	__builtin_cpu_init();
	if (__builtin_cpu_supports("popcnt") > 0)
		return _ULIST_BUILTIN_POPCNT(n);
#endif

	return _popcnt_bithacks(n);
}

#endif /* POPCNT_H */
