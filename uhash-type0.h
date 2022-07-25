/* uhash: simple grow-only hash
 *
 * - uhash "type 0": key is "scalar", no value
 *
 * refs:
 * - https://github.com/etherealvisage/avl
 * - https://github.com/DanielGibson/Snippets
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * (c) 2022, Konstantin Demin
 */

#ifndef UHASH_TYPE0_H
#define UHASH_TYPE0_H

#include "uhash-common.h"

#define _UHASH_TYPEPROC_NODE_VISITOR__TYPE0(user_t, key_t) \
	typedef int (* UHASH_T(user_t, node_proc) ) (key_t key);

#define _UHASH_TYPE_NODE__TYPE0(user_t, key_t) \
	typedef struct UHASH_T(user_t, node) { \
		uhash_idx_t  left, right; \
		int          depth; \
		key_t        key; \
	} UHASH_T(user_t, node);

#define _UHASH_TYPEIMPL__TYPE0(user_t) \
	ulist_t                   nodes; \
	uhash_idx_t               tree_root; \
	UHASH_T(user_t, key_cmp)  key_comparator;


#define _UHASH_PROC_KEY__TYPE0(user_t, key_t) \
	static inline key_t \
	UHASH_P(user_t, _key) (const user_t * hash, const UHASH_T(user_t, node) * node) { \
		return node->key; \
	} \
	\
	static const key_t * \
	UHASH_P(user_t, key) (const user_t * hash, uhash_idx_t node_index) { \
		const UHASH_T(user_t, node) * node = UHASH_C(user_t, cnode, hash, node_index); \
		if (node == NULL) \
			return NULL; \
		return &(node->key); \
	} \
	\
	static inline void \
	UHASH_P(user_t, _set_key) (user_t * hash, UHASH_T(user_t, node) * node, key_t key) { \
		node->key = key; \
	} \
	\
	static void \
	UHASH_P(user_t, set_key) (user_t * hash, uhash_idx_t node_index, key_t key) { \
		UHASH_T(user_t, node) * node = UHASH_C(user_t, node, hash, node_index); \
		if (node == NULL) \
			return; \
		UHASH_C(user_t, _set_key, hash, node, key); \
	}


#define _UHASH_PROC__INIT_NODE__TYPE0(user_t, key_t) \
	static inline void \
	UHASH_P(user_t, _init_node_real) (user_t * hash, UHASH_T(user_t, node) * node, key_t key) { \
		node->depth = 1; \
		UHASH_C(user_t, _set_key, hash, node, key); \
	} \
	\
	static inline void \
	UHASH_P(user_t, _init_node) (user_t * hash, UHASH_T(user_t, node) * node, key_t key, void * value) { \
		UHASH_C(user_t, _init_node_real, hash, node, key); \
	}

#define _UHASH_PROC__NODE_VISITOR__TYPE0(user_t) \
	static inline int \
	UHASH_P(user_t, _node_visitor) (const user_t * hash, const UHASH_T(user_t, node) * node, UHASH_T(user_t, node_proc) visitor) { \
		return visitor( \
			UHASH_C(user_t, _key, hash, node) \
		); \
	}

#define _UHASH_PROCIMPL_INIT__TYPE0(user_t) \
	{ \
	memset(hash, 0, sizeof(user_t)); \
	ulist_init(&hash->nodes, sizeof(UHASH_T(user_t, node))); \
	}

#define _UHASH_PROC_INIT__TYPE0(user_t) \
	static void \
	UHASH_P(user_t, init) (user_t * hash) \
		_UHASH_PROCIMPL_INIT__TYPE0(user_t)

#define _UHASH_PROCIMPL_FREE__TYPE0(user_t) \
	{ \
	ulist_free(&hash->nodes); \
	memset(hash, 0, sizeof(user_t)); \
	}

#define _UHASH_PROC_FREE__TYPE0(user_t) \
	static void \
	UHASH_P(user_t, free) (user_t * hash) \
		_UHASH_PROCIMPL_FREE__TYPE0(user_t)


#define _UHASH_PROC_SEARCH__TYPE0(user_t, key_t) \
	static uhash_idx_t \
	UHASH_P(user_t, search) (user_t * hash, key_t key) \
		_UHASH_PROCIMPL_SEARCH(user_t)

#define _UHASH_PROC_INSERT__TYPE0(user_t, key_t) \
	static uhash_idx_t \
	UHASH_P(user_t, insert) (user_t * hash, key_t key) { \
		void * value = NULL; \
		_UHASH_PROCIMPL_INSERT(user_t) \
	}


#define UHASH_DEFINE__TYPE0(user_t, key_t) \
	_UHASH_TYPEPROC_NODE_VISITOR__TYPE0(user_t, key_t) \
	_UHASH_TYPEPROC_KEY_VISITOR(user_t, key_t) \
	_UHASH_TYPEPROC_CMP_KEY_PLAIN(user_t, key_t) \
	\
	_UHASH_TYPE_NODE__TYPE0(user_t, key_t) \
	typedef struct { \
		_UHASH_TYPEIMPL__TYPE0(user_t) \
	} user_t; \
	\
	_UHASH_PROC_NODE(user_t) \
	_UHASH_PROC_KEY__TYPE0(user_t, key_t) \
	\
	_UHASH_PROC__INIT_NODE__TYPE0(user_t, key_t) \
	_UHASH_PROC__NODE_VISITOR__TYPE0(user_t) \
	_UHASH_PROC_INIT__TYPE0(user_t) \
	_UHASH_PROC_FREE__TYPE0(user_t) \
	\
	_UHASH_PROC_RELA_INDEX(user_t) \
	_UHASH_PROC_RELA_NODE(user_t) \
	_UHASH_PROC_LEFT(user_t) \
	_UHASH_PROC_RIGHT(user_t) \
	_UHASH_PROC_DEPTH(user_t) \
	_UHASH_PROC_TREE_DEPTH(user_t) \
	_UHASH_PROC_BALANCE_FACTOR(user_t) \
	_UHASH_PROC_UPDATE_DEPTH(user_t) \
	_UHASH_PROC_ROTATE_LEFT(user_t) \
	_UHASH_PROC_ROTATE_RIGHT(user_t) \
	_UHASH_PROC_ROTATE(user_t) \
	_UHASH_PROC_REBALANCE(user_t) \
	_UHASH_PROC_WALK(user_t) \
	\
	_UHASH_PROC_SEARCH__TYPE0(user_t, key_t) \
	_UHASH_PROC_INSERT__TYPE0(user_t, key_t) \


#endif /* UHASH_TYPE0_H */
