/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_rx_h__
#define INCLUDE_git_rx_h__

#include "common.h"

struct git_observer;

/**
 * A message sent to an observer
 *
 * Consumers should generally not assume that the contents will be
 * valid beyond the on_next callback.
 */
typedef struct {
	/**
	 * The type of message
	 */
	int type;
	/**
	 * A pointer to the data
	 */
	void *data;
}git_message;

typedef int (*git_on_next_fn)(struct git_observer *obs, git_message *msg);
typedef int (*git_on_error_fn)(struct git_observer *obs);
typedef int (*git_on_completed_fn)(struct git_observer *obs);

/**
 * An observer receives messages
 *
 * Subscribe to an observable in order to start receiving messages.
 */
typedef struct git_observer {
	git_on_next_fn      on_next;
	git_on_error_fn     on_error;
	git_on_completed_fn on_completed;
} git_observer;

/**
 * On observable sends messages
 */
typedef struct git_observable git_observable;

/**
 * A subject is an observer and observable at the same time
 *
 * You can ask it for an observable and (optionally) perform filtering
 * on it before subscribing.
 */
typedef struct git_subject git_subject;

/**
 * The relationship between an observer and observable
 */
typedef struct git_subscription git_subscription;

/**
 * Create a new subject
 */
GIT_EXTERN(int) git_subject_new(git_subject **out);

/**
 * Send a message to a subject
 */
GIT_EXTERN(int) git_subject_on_next(git_subject *subj, git_message *msg);

/**
 * Signal a channel as completed
 */
GIT_EXTERN(int) git_subject_on_completed(git_subject *subj);

/**
 * Signal an error
 */
GIT_EXTERN(int) git_subject_on_error(git_subject *subj);

/**
 * Free a subject
 *
 * Subjects are reference-counted. When the last observable is freed,
 * the subject will also be freed.
 */
GIT_EXTERN(void) git_subject_free(git_subject *subj);

/**
 * Create a new observable out of a subject
 *
 * You can then perform filtering/throttling on this observable.
 */
GIT_EXTERN(int) git_observable_new(git_observable **out, git_subject *subj);

/**
 * Subscribe to an observable
 */
GIT_EXTERN(int) git_observable_subscribe(git_subscription **out, git_observable *observable, git_observer *observer);

/**
 * Free an observable
 *
 * Observables are reference-counted, when the last subscription is
 * disposed, the observable will be freed.
 */
GIT_EXTERN(void) git_observable_free(git_observable *obs);

/**
 * Dispose of a subscription
 *
 * The observer will no longer receive any messages.
 */
GIT_EXTERN(void) git_subscription_dispose(git_subscription *sub);

#endif


















