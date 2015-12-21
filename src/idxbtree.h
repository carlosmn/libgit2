/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_idxbtree_h__
#define INCLUDE_idxbtree_h__

#include <ctype.h>
#include "common.h"
#include "git2/index.h"

#define kmalloc git__malloc
#define kcalloc git__calloc
#define krealloc git__realloc
#define kreallocarray git__reallocarray
#define kfree git__free
#include "kbtree.h"

__KB_TREE_T(idx)
__KB_TREE_T(idxicase)

typedef kbtree_t(idx) git_idxbtree;
typedef kbtree_t(idxicase) git_idxbtree_icase;

GIT_INLINE(int) idxentry_compare(const git_index_entry *a, const git_index_entry *b)
{
	int ret = strcmp(a->path, b->path);
	if (ret == 0)
		ret = GIT_IDXENTRY_STAGE(a) - GIT_IDXENTRY_STAGE(b);

	return ret;
}

GIT_INLINE(int) idxentry_icase_compare(const git_index_entry *a, const git_index_entry *b)
{
	int ret = strcasecmp(a->path, b->path);
	if (ret == 0)
		ret = GIT_IDXENTRY_STAGE(a) - GIT_IDXENTRY_STAGE(b);

	return ret;
}

typedef const git_index_entry *git_index_entry_p;

#define idxentry_compare_wrap(a, b) idxentry_compare((a), (b))
#define idxentry_icase_compare_wrap(a, b) idxentry_icase_compare((a), (b))

#define GIT__USE_IDXBTREE \
	__KB_TREE_IMPL(idx, git_index_entry_p, idxentry_compare_wrap)
#define GIT__USE_IDXBTREE_ICASE \
	__KB_TREE_IMPL(idxicase, git_index_entry_p, idxentry_compare_wrap)

#define GIT_BTREE_DEFAULT_SIZE KB_DEFAULT_SIZE

#define git_idxbtree_alloc(hp, size)						\
	((*(hp) = kb_init(idx, size)) == NULL) ? giterr_set_oom(), -1 : 0

#define git_idxbtree_icase_alloc(hp, size)						\
	((*(hp) = kb_init(idxicase, size)) == NULL) ? giterr_set_oom(), -1 : 0

#define git_idxbtree_free(h) do { kb_destroy(idx, h); h = NULL; } while(0)

#define git_idxbtree_insert(bt, e) kb_put(idx, bt, e)
#define git_idxbtree_icase_insert(bt, e) kb_put(idxicase, bt, e)

#define git_idxbtree_lookup(bt, v) kb_get(idx, bt, v)
#define git_idxbtree_icase_lookup(bt, v) kb_get(idxicase, bt, v)

#define git_idxbtree_delete(bt, v) kb_del(idx, bt, v)
#define git_idxbtree_icase_delete(bt, v) kb_del(idxicase, bt, v)

#endif
