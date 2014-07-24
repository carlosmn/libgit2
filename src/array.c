/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "array.h"

typedef git_array_t(char) git_array_generic_t;

/* use a generic array for growth so this can return the new item */
void *git_array_grow(void *_a, size_t item_size)
{
	git_array_generic_t *a = _a;
	uint32_t new_size = (a->size < 8) ? 8 : a->asize * 3 / 2;
	char *new_array = git__realloc(a->ptr, new_size * item_size);
	if (!new_array) {
		git_array_clear(*a);
		return NULL;
	} else {
		a->ptr = new_array; a->asize = new_size; a->size++;
		return a->ptr + (a->size - 1) * item_size;
	}
}
