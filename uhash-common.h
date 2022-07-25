/* uhash: simple grow-only hash
 *
 * - common definitions
 *
 * refs:
 * - https://github.com/etherealvisage/avl
 * - https://github.com/DanielGibson/Snippets
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * (c) 2022, Konstantin Demin
 */

#ifndef UHASH_COMMON_H
#define UHASH_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "popcnt.h"

#ifdef UHASH_IDX_T
  #undef UHASH_IDX_T
#endif
#ifndef UHASH_FULL_WIDTH_INDICES
  #ifdef UHASH_SHORT_INDICES
	#define UHASH_IDX_T  uint16_t
  #else
	#define UHASH_IDX_T  uint32_t
  #endif
#else
  #define UHASH_IDX_T  size_t
#endif
typedef UHASH_IDX_T uhash_idx_t;

#define ULIST_IDX_T  UHASH_IDX_T

#include "ulist.h"

#define UHASH_TYPE(type, kind) uhash_ ## kind ## __ ## type
#define UHASH_PROC(type, proc) type ## __ ## proc
#define UHASH_CALL(type, proc, ...) UHASH_PROC(type, proc) (__VA_ARGS__)

// shorthands
#define UHASH_T(type, kind) UHASH_TYPE(type, kind)
#define UHASH_P(type, proc) UHASH_PROC(type, proc)
#define UHASH_C(type, proc, ...) UHASH_CALL(type, proc, __VA_ARGS__)

#define UHASH_DEFINE_DEFAULT_KEY_COMPARATOR(key_t) \
	static int \
	UHASH_T(key_t, cmp) (key_t k1, key_t k2) { \
		if (k1 == k2) return 0; \
		return (k1 > k2) ? 1 : -1; \
	}

#define UHASH_SET_KEY_COMPARATOR(hash, key_Cmp) \
	{ \
	(hash)->key_comparator = key_Cmp; \
	}

#define UHASH_SET_DEFAULT_KEY_COMPARATOR(hash, key_t) \
	UHASH_SET_KEY_COMPARATOR(hash, UHASH_T(key_t, cmp))

#define UHASH_SET_VALUE_HANDLERS(hash, value_Ctor, value_Dtor) \
	{ \
	(hash)->value_constructor = value_Ctor; \
	(hash)->value_destructor  = value_Dtor; \
	}

static const int _uhash_avl_left  = 0;
static const int _uhash_avl_right = 1;

static inline uhash_idx_t _uhash_idx_int(uhash_idx_t idx) { return (idx - 1); }
static inline uhash_idx_t _uhash_idx_pub(uhash_idx_t idx) { return (idx + 1); }

#define _UHASH_IDX_T__WIDTH          (_ULIST_POPCNT_MACRO(((ULIST_IDX_T) ~0)))
#define _UHASH_IDX_T__SELECTOR_BITS  2
#define _UHASH_IDX_T__TRUINDEX_BITS  (_UHASH_IDX_T__WIDTH - _UHASH_IDX_T__SELECTOR_BITS)

#define UHASH_IDX_T_MAX  ((1 << _UHASH_IDX_T__TRUINDEX_BITS) - 1)

#define _UHASH_IDX_T__TRUINDEX_MASK  (UHASH_IDX_T_MAX)

#define UHASH_NODE_SELECTOR_SELF   0
#define UHASH_NODE_SELECTOR_LEFT   1
#define UHASH_NODE_SELECTOR_RIGHT  2
#define UHASH_NODE_SELECTOR_ROOT   3
#define _UHASH_NODE_SELECTOR_MASK  3

static inline uhash_idx_t _uhash_idx_truindex(uhash_idx_t idx) { return idx &  _UHASH_IDX_T__TRUINDEX_MASK; }
static inline uhash_idx_t _uhash_idx_selector(uhash_idx_t idx) { return idx >> _UHASH_IDX_T__TRUINDEX_BITS; }

static inline uhash_idx_t uhash_node_rela_index(uhash_idx_t selector, uhash_idx_t truindex) {
	return _uhash_idx_truindex(truindex) | ((selector & _UHASH_NODE_SELECTOR_MASK) << _UHASH_IDX_T__TRUINDEX_BITS);
}

#define _UHASH_TYPEPROC_KEY_VISITOR(user_t, key_t) \
	typedef int (* UHASH_T(user_t, key_proc) ) (key_t * key);

#define _UHASH_TYPEPROC_VALUE_VISITOR(user_t, value_t) \
	typedef int (* UHASH_T(user_t, value_proc) ) (value_t * value);

#define _UHASH_TYPEPROC_CMP_KEY_PLAIN(user_t, key_t) \
	typedef int (* UHASH_T(user_t, key_cmp) ) (key_t key1, key_t key2);

#define _UHASH_TYPEPROC_CMP_KEY_PTR(user_t, key_t) \
	typedef int (* UHASH_T(user_t, key_cmp) ) (const key_t * key1, const key_t * key2);

#define _UHASH_PROC__NODE(user_t) \
	static inline UHASH_T(user_t, node) * \
	UHASH_P(user_t, _node) (user_t * hash, uhash_idx_t index) { \
		return ulist_get(&hash->nodes, _uhash_idx_int(index)); \
	}

#define _UHASH_PROC_NODE(user_t) \
	static UHASH_T(user_t, node) * \
	UHASH_P(user_t, node) (user_t * hash, uhash_idx_t node_index) { \
		return (node_index == 0) ? NULL : UHASH_C(user_t, _node, hash, node_index); \
	}

#define _UHASH_PROC_RELA_INDEX(user_t) \
	static uhash_idx_t * \
	UHASH_P(user_t, rela_index) (user_t * hash, uhash_idx_t index) { \
		uhash_idx_t selector = _uhash_idx_selector(index); \
		switch (selector) { \
		case UHASH_NODE_SELECTOR_ROOT: \
			return &(hash->tree_root); \
		} \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, _uhash_idx_truindex(index)); \
		if (node == NULL) \
			return NULL; \
		switch (selector) { \
		case UHASH_NODE_SELECTOR_LEFT: \
			return &(node->left); \
		case UHASH_NODE_SELECTOR_RIGHT: \
			return &(node->right); \
		} \
		return NULL; \
	}

#define _UHASH_PROC_RELA_NODE(user_t) \
	static UHASH_T(user_t, node) * \
	UHASH_P(user_t, rela_node) (user_t * hash, uhash_idx_t index) { \
		uhash_idx_t selector = _uhash_idx_selector(index); \
		switch (selector) { \
		case UHASH_NODE_SELECTOR_ROOT: \
			return UHASH_C(user_t, node, hash, hash->tree_root); \
		} \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, _uhash_idx_truindex(index)); \
		if (node == NULL) \
			return NULL; \
		switch (selector) { \
		case UHASH_NODE_SELECTOR_LEFT: \
			return UHASH_C(user_t, node, hash, node->left); \
		case UHASH_NODE_SELECTOR_RIGHT: \
			return UHASH_C(user_t, node, hash, node->right); \
		} \
		return node; \
	}

#define _UHASH_PROC_LEFT(user_t) \
	static uhash_idx_t \
	UHASH_P(user_t, left) (user_t * hash, uhash_idx_t node_index) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		return (node == NULL) ? 0 : node->left; \
	}

#define _UHASH_PROC_RIGHT(user_t) \
	static uhash_idx_t \
	UHASH_P(user_t, right) (user_t * hash, uhash_idx_t node_index) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		return (node == NULL) ? 0 : node->right; \
	}

#define _UHASH_PROC_DEPTH(user_t) \
	static int \
	UHASH_P(user_t, depth) (user_t * hash, uhash_idx_t node_index) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		return (node == NULL) ? 0 : node->depth; \
	}

#define _UHASH_PROC_TREE_DEPTH(user_t) \
	static int \
	UHASH_P(user_t, tree_depth) (user_t * hash) { \
		return UHASH_C(user_t, depth, hash, hash->tree_root); \
	}

#define _UHASH_PROC_BALANCE_FACTOR(user_t) \
	static int \
	UHASH_P(user_t, balance_factor) (user_t * hash, uhash_idx_t node_index) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		if (node == NULL) \
			return 0; \
		int d_left  = UHASH_C(user_t, depth, hash, node->left); \
		int d_right = UHASH_C(user_t, depth, hash, node->right); \
		return (d_left - d_right); \
	}

#define _UHASH_PROC_UPDATE_DEPTH(user_t) \
	static void \
	UHASH_P(user_t, update_depth) (user_t * hash, uhash_idx_t node_index) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		if (node == NULL) \
			return; \
		int d_left  = UHASH_C(user_t, depth, hash, node->left); \
		int d_right = UHASH_C(user_t, depth, hash, node->right); \
		if (d_left != 0) \
			node->depth = d_left; \
		if (d_right != 0) \
			if (node->depth < d_right) \
				node->depth = d_right; \
		node->depth++; \
	}

#define _UHASH_PROC_ROTATE_LEFT(user_t) \
	static void \
	UHASH_P(user_t, rotate_left) (user_t * hash, uhash_idx_t node_index) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		UHASH_T(user_t, node) * left = UHASH_C(user_t, node, hash, node->left); \
		uhash_idx_t i = left->right; \
		UHASH_T(user_t, node) * x = UHASH_C(user_t, node, hash, i); \
		left->right = x->left; \
		x->left = node->left; \
		UHASH_C(user_t, update_depth, hash, node->left); \
		node->left = i; \
		UHASH_C(user_t, update_depth, hash, node->left); \
	}

#define _UHASH_PROC_ROTATE_RIGHT(user_t) \
	static void \
	UHASH_P(user_t, rotate_right) (user_t * hash, uhash_idx_t node_index) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		UHASH_T(user_t, node) * right = UHASH_C(user_t, node, hash, node->right); \
		uhash_idx_t i = right->left; \
		UHASH_T(user_t, node) * x = UHASH_C(user_t, node, hash, i); \
		right->left = x->right; \
		x->right = node->right; \
		UHASH_C(user_t, update_depth, hash, node->right); \
		node->right = i; \
		UHASH_C(user_t, update_depth, hash, node->right); \
	}

#define _UHASH_PROC_ROTATE(user_t) \
	static void \
	UHASH_P(user_t, rotate) (user_t * hash, uhash_idx_t * node_index_ptr, int direction) { \
		if (node_index_ptr == NULL) \
			return; \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, *node_index_ptr); \
		if (node == NULL) \
			return; \
		uhash_idx_t i; \
		UHASH_T(user_t, node) * x; \
		if (direction == _uhash_avl_left) { \
			i = node->right; \
			x = UHASH_C(user_t, node, hash, i); \
			node->right = x->left; \
			x->left = *node_index_ptr; \
		} else \
		if (direction == _uhash_avl_right) { \
			i = node->left; \
			x = UHASH_C(user_t, node, hash, i); \
			node->left = x->right; \
			x->right = *node_index_ptr; \
		} else \
			return; \
		UHASH_C(user_t, update_depth, hash, *node_index_ptr); \
		UHASH_C(user_t, update_depth, hash, i); \
		*node_index_ptr = i; \
	}

#define _UHASH_PROC_REBALANCE(user_t) \
	static void \
	UHASH_P(user_t, rebalance) (user_t * hash, uhash_idx_t * node_index_ptr) { \
		if (node_index_ptr == NULL) \
			return; \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, *node_index_ptr); \
		if (node == NULL) \
			return; \
		int delta = UHASH_C(user_t, balance_factor, hash, *node_index_ptr); \
		switch (delta) { \
		case 2: \
			if (UHASH_C(user_t, balance_factor, hash, node->left) < 0) \
				UHASH_C(user_t, rotate_left, hash, *node_index_ptr); \
			\
			UHASH_C(user_t, rotate, hash, node_index_ptr, _uhash_avl_right); \
			break; \
		case -2: \
			if (UHASH_C(user_t, balance_factor, hash, node->right) > 0) \
				UHASH_C(user_t, rotate_right, hash, *node_index_ptr); \
			\
			UHASH_C(user_t, rotate, hash, node_index_ptr, _uhash_avl_left); \
			break; \
		} \
	}


#define _UHASH_PROCIMPL_SEARCH(user_t) \
	{ \
		uhash_idx_t idx = hash->tree_root; \
		while (idx != 0) { \
			UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, idx); \
			if (node == NULL) \
				return 0; \
			int cmp = hash->key_comparator(key, UHASH_C(user_t, _key, hash, node)); \
			if (cmp == 0) \
				return idx; \
			if (cmp > 0) \
				idx = node->right; \
			else \
				idx = node->left; \
		} \
		return 0; \
	}

#define _UHASH_PROCIMPL_INSERT(user_t) \
	{ \
		if (hash->nodes.used == UHASH_IDX_T_MAX) \
			return 0; \
		uhash_idx_t idx_rela = uhash_node_rela_index(UHASH_NODE_SELECTOR_ROOT, 0); \
		uhash_idx_t * node_index_ptr; \
		UHASH_T(user_t, node) * node; \
		int cmp; \
		ulist_t branch; \
		ulist_init(&branch, sizeof(uhash_idx_t)); \
		while (1) { \
			node_index_ptr = UHASH_C(user_t, rela_index, hash, idx_rela); \
			if (*node_index_ptr == 0) { \
				uhash_idx_t i = ulist_append(&hash->nodes, NULL); \
				node = ulist_get(&hash->nodes, i); \
				UHASH_C(user_t, _init_node, hash, node, key, value); \
				node_index_ptr = UHASH_C(user_t, rela_index, hash, idx_rela); \
				*node_index_ptr = _uhash_idx_pub(i); \
				break; \
			} \
			node = UHASH_C(user_t, node, hash, *node_index_ptr); \
			cmp = hash->key_comparator(key, UHASH_C(user_t, _key, hash, node)); \
			if (cmp == 0) \
				break; \
			ulist_append(&branch, &idx_rela); \
			if (cmp > 0) \
				idx_rela = uhash_node_rela_index(UHASH_NODE_SELECTOR_RIGHT, *node_index_ptr); \
			else \
				idx_rela = uhash_node_rela_index(UHASH_NODE_SELECTOR_LEFT, *node_index_ptr); \
		} \
		uhash_idx_t * a; \
		for (uhash_idx_t i = branch.used; i != 0; i--) { \
			a = ulist_get(&branch, i - 1); \
			node_index_ptr = UHASH_C(user_t, rela_index, hash, *a); \
			UHASH_C(user_t, rebalance, hash, node_index_ptr); \
			UHASH_C(user_t, update_depth, hash, *node_index_ptr); \
		} \
		ulist_free(&branch); \
		return *node_index_ptr; \
	}

#define _UHASH_PROC_WALK(user_t) \
	static void \
	UHASH_P(user_t, walk) (const user_t * hash, UHASH_T(user_t, node_proc) visitor) { \
		for (uhash_idx_t i = 0; i < hash->nodes.used; i++) { \
			UHASH_C(user_t, _node_visitor, hash, ulist_get(&hash->nodes, i), visitor); \
		} \
	}

#endif /* UHASH_COMMON_H */
