/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_rx_h__
#define INCLUDE_rx_h__

#include "common.h"
#include "vector.h"
#include "git2/rx.h"

struct git_subject {
	git_refcount rc;

	git_vector observables;
};

struct git_observable {
	git_refcount rc;

	git_vector subscribers;
};

struct git_subscription {
	git_observable *observable;
	git_observer   *observer;
};

#endif










