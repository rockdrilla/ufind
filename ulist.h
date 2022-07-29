/* ulist: simple grow-only dynamic array
 *
 * refs:
 * - https://github.com/DanielGibson/Snippets
 *
 * SPDX-License-Identifier: Apache-2.0
 * (c) 2022, Konstantin Demin
 */

#ifndef ULIST_H
#define ULIST_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "popcnt.h"

#ifndef ULIST_IDX_T
  #define ULIST_IDX_T  uint32_t
#endif
typedef ULIST_IDX_T ulist_idx_t;

#ifdef _ULIST_MEMBLOCK_DEFAULT
  #undef _ULIST_MEMBLOCK_DEFAULT
#endif
#define _ULIST_MEMBLOCK_DEFAULT  65536
#ifndef ULIST_MEMBLOCK
  #define ULIST_MEMBLOCK  _ULIST_MEMBLOCK_DEFAULT
#endif

#ifdef _ULIST_GROWTH_FACTOR_DEFAULT
  #undef _ULIST_GROWTH_FACTOR_DEFAULT
#endif
#define _ULIST_GROWTH_FACTOR_DEFAULT  8
#ifndef ULIST_GROWTH_FACTOR
  #define ULIST_GROWTH_FACTOR  _ULIST_GROWTH_FACTOR_DEFAULT
#endif

typedef struct {
	void         * ptr;
	uint           item, growth;
	ulist_idx_t    used, allocated;
} ulist_t;

typedef void (*ulist_item_visitor) (const void * value, ulist_idx_t index);

static const size_t ulist_memblock
	= (_POPCNT_MACRO(ULIST_MEMBLOCK) == 1)
	? (ULIST_MEMBLOCK)
	: _ULIST_MEMBLOCK_DEFAULT;

static const uint8_t ulist_growth_factor
	= (((ULIST_GROWTH_FACTOR) > 1) && ((ULIST_GROWTH_FACTOR) < (__INTPTR_WIDTH__ / 2)))
	? (ULIST_GROWTH_FACTOR)
	: _ULIST_GROWTH_FACTOR_DEFAULT;

static size_t _ulist_align_alloc(size_t length, size_t align)
{
	if (align == 0)
		return 0;
	if (_popcnt(align) == 1) {
		size_t mask = align - 1;
		return (length & ~mask) + (((length & mask) != 0) ? align : 0);
	}
	size_t rem = length % align;
	return length + ((rem != 0) ? (align - rem) : 0);
}

static ulist_idx_t _ulist_calc_growth(size_t item_size)
{
	static const size_t watermark = ulist_memblock >> ulist_growth_factor;
	if (item_size > watermark)
		return _ulist_align_alloc(item_size << ulist_growth_factor, ulist_memblock);
	return ulist_memblock;
}

static void _ulist_free(void * ptr, size_t len)
{
	if (ptr == NULL)
		return;
	if (len != 0)
		memset(ptr, 0, len);
	free(ptr);
}

static inline void * _ulist_item_ptr(const ulist_t * list, ulist_idx_t index)
{
	return (void *)((size_t) list->ptr + list->item * index);
}

static void _ulist_grow_by_size(ulist_t * list, size_t length)
{
	if (length == 0)
		return;

	size_t old_alloc = (size_t) list->item * (size_t) list->allocated;
	size_t new_alloc = _ulist_align_alloc(old_alloc + length, ulist_memblock);
	old_alloc = _ulist_align_alloc(old_alloc, ulist_memblock);
	if (new_alloc <= old_alloc)
		return;

	void * new_ptr = realloc(list->ptr, new_alloc);
	if (new_ptr == NULL)
		return;

	list->ptr = new_ptr;
	list->allocated = new_alloc / (size_t) list->item;
	memset(_ulist_item_ptr(list, list->used), 0, list->item * (list->allocated - list->used));
}

static inline void _ulist_grow_by_item_count(ulist_t * list, ulist_idx_t count)
{
	if (count == 0)
		return;
	_ulist_grow_by_size(list, list->item * count);
}

static inline void _ulist_autogrow(ulist_t * list)
{
	_ulist_grow_by_size(list, list->growth);
}

static void _ulist_set(ulist_t * list, ulist_idx_t index, void * item)
{
	void * dst = _ulist_item_ptr(list, index);
	if (item != NULL)
		memcpy(dst, item, list->item);
	else
		memset(dst, 0, list->item);
}

static void ulist_init(ulist_t * list, ulist_idx_t item_size)
{
	memset(list, 0, sizeof(ulist_t));
	list->item = item_size;
	if (item_size == 0)
		return;
	list->growth = _ulist_calc_growth(item_size);
}

static void ulist_free(ulist_t * list)
{
	_ulist_free(list->ptr, list->item * list->used);
	memset(list, 0, sizeof(ulist_t));
}

static void * ulist_get(const ulist_t * list, ulist_idx_t index)
{
	return (index >= list->used) ? NULL : _ulist_item_ptr(list, index);
}

static void ulist_set(ulist_t * list, ulist_idx_t index, void * item)
{
	if (index >= list->used)
		return;
	_ulist_set(list, index, item);
}

static ulist_idx_t ulist_append(ulist_t * list, void * item)
{
	if (list->used == list->allocated)
		_ulist_autogrow(list);
	_ulist_set(list, list->used, item);
	return (list->used++);
}

static void ulist_walk(const ulist_t * list, ulist_item_visitor visitor)
{
	for (ulist_idx_t i = 0; i < list->used; i++) {
		visitor(_ulist_item_ptr(list, i), i);
	}
}

static void ulist_rwalk(const ulist_t * list, ulist_item_visitor visitor)
{
	for (ulist_idx_t i = list->used; (i--) != 0; ) {
		visitor(_ulist_item_ptr(list, i), i);
	}
}

#endif /* ULIST_H */
