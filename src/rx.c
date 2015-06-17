/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "util.h"
#include "rx.h"

int git_subject_new(git_subject **out)
{
	git_subject *subj;

	subj = git__calloc(1, sizeof(git_subject));
	GITERR_CHECK_ALLOC(subj);

	if ((git_vector_init(&subj->observables, 0, NULL)) < 0) {
		git__free(subj);
		return -1;
	}

	GIT_REFCOUNT_INC(subj);

	*out = subj;
	return 0;
}

static void subject_free(git_subject *subj)
{
	git_vector_free(&subj->observables);
	git__free(subj);
}

void git_subject_free(git_subject *subj)
{
	GIT_REFCOUNT_DEC(subj, subject_free);
}

int git_subject_on_next(git_subject *subj, git_message *msg)
{
	size_t i, j;
	git_observable *observable;
	git_subscription *subscription;

	git_vector_foreach(&subj->observables, i, observable) {
		git_vector_foreach(&observable->subscribers, j, subscription) {
			subscription->observer->on_next(subscription->observer, msg);
		}
	}

	return 0;
}

int git_subject_on_error(git_subject *subj)
{
	size_t i, j;
	git_observable *observable;
	git_subscription *subscription;

	git_vector_foreach(&subj->observables, i, observable) {
		git_vector_foreach(&observable->subscribers, j, subscription) {
			subscription->observer->on_error(subscription->observer);
		}
	}

	return 0;
}

int git_subject_on_completed(git_subject *subj)
{
	size_t i, j;
	git_observable *observable;
	git_subscription *subscription;

	git_vector_foreach(&subj->observables, i, observable) {
		git_vector_foreach(&observable->subscribers, j, subscription) {
			subscription->observer->on_completed(subscription->observer);
		}
	}

	return 0;
}

int git_observable_new(git_observable **out, git_subject *subj)
{
	git_observable *obs;

	obs = git__calloc(1, sizeof(git_observable));
	GITERR_CHECK_ALLOC(obs);

	if (git_vector_init(&obs->subscribers, 0, NULL) < 0) {
		git__free(obs);
		return -1;
	}

	if (git_vector_insert(&subj->observables, obs) < 0) {
		git_vector_free(&obs->subscribers);
		git__free(obs);
	}

	GIT_REFCOUNT_INC(obs);

	*out = obs;
	return 0;
}

static int add_subscription(git_observable *observable, git_subscription *sub)
{
	int error;

	if ((error = git_vector_insert(&observable->subscribers, sub)) < 0)
		return error;

	return 0;
}

int git_observable_subscribe(git_subscription **out, git_observable *observable, git_observer *observer)
{
	int error;
	git_subscription *sub;

	sub = git__calloc(1, sizeof(git_subscription));
	GITERR_CHECK_ALLOC(sub);

	if ((error = add_subscription(observable, sub)) < 0) {
		git__free(sub);
		return error;
	}

	GIT_REFCOUNT_INC(observable);
	sub->observable = observable;
	sub->observer   = observer;

	*out = sub;
	return 0;
}

static void observable_free(git_observable *obs)
{
	git_vector_free(&obs->subscribers);
	git__free(obs);
}

void git_observable_free(git_observable *obs)
{
	GIT_REFCOUNT_DEC(obs, observable_free);
}

void git_subscription_dispose(git_subscription *sub)
{
	git_observable_free(sub->observable);
	git__free(sub);
}
