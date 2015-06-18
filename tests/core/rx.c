#include "clar_libgit2.h"
#include "rx.h"

static int dummy_val;

static git_subject *subject;
static bool on_next_called;
static bool on_completed_called;
static bool on_error_called;

void test_core_rx__initialize(void)
{
	on_next_called      = false;
	on_completed_called = false;
	on_error_called     = false;
	cl_git_pass(git_subject_new(&subject));
}

void test_core_rx__cleanup(void)
{
	git_subject_free(subject);
}

static int on_next(git_observer *observer, git_message *msg)
{
	GIT_UNUSED(observer);

	cl_assert_equal_i(1, msg->type);
	cl_assert_equal_p(&dummy_val, msg->data);
	on_next_called = true;

	return 0;
}

static int on_completed(git_observer *observer)
{
	GIT_UNUSED(observer);

	on_completed_called = true;

	return 0;
}

static int on_error(git_observer *observer)
{
	GIT_UNUSED(observer);

	on_error_called = true;

	return 0;
}

void test_core_rx__successful_messages(void)
{
	git_observer observer;
	git_observable *observable;
	git_subscription *subscription;
	git_message message;

	observer.on_next      = on_next;
	observer.on_completed = on_completed;
	observer.on_error     = on_error;

	cl_git_pass(git_observable_new(&observable, subject));
	cl_git_pass(git_observable_subscribe(&subscription, observable, &observer));

	message.type = 1;
	message.data = &dummy_val;
	cl_git_pass(git_subject_on_next(subject, &message));

	cl_git_pass(git_subject_on_completed(subject));

	/* Can't send after finishing the stream */
	cl_git_fail(git_subject_on_next(subject, &message));

	cl_assert(on_next_called);
	cl_assert(on_completed_called);
	cl_assert(!on_error_called);

	git_subscription_dispose(subscription);
	git_observable_free(observable);
}

void test_core_rx__on_error(void)
{
	git_observer observer;
	git_observable *observable;
	git_subscription *subscription;
	git_message message;

	observer.on_next      = on_next;
	observer.on_completed = on_completed;
	observer.on_error     = on_error;

	cl_git_pass(git_observable_new(&observable, subject));
	cl_git_pass(git_observable_subscribe(&subscription, observable, &observer));

	message.type = 1;
	message.data = &dummy_val;
	cl_git_pass(git_subject_on_next(subject, &message));

	cl_git_pass(git_subject_on_error(subject));

	/* on_error finishes the stream */
	cl_git_fail(git_subject_on_completed(subject));

	cl_assert(on_next_called);
	cl_assert(!on_completed_called);
	cl_assert(on_error_called);

	git_subscription_dispose(subscription);
	git_observable_free(observable);
}

static int count;
static int on_next_count(git_observer *obs, git_message *msg)
{
	GIT_UNUSED(obs);
	GIT_UNUSED(msg);

	on_next_called = true;
	count++;

	return 0;
}

void test_core_rx__subscribe_unsubscribe(void)
{
	git_observer observer;
	git_observable *observable;
	git_subscription *subscription1, *subscription2;
	git_message message;

	observer.on_next      = on_next_count;
	observer.on_completed = on_completed;
	observer.on_error     = on_error;

	cl_git_pass(git_observable_new(&observable, subject));
	cl_git_pass(git_observable_subscribe(&subscription1, observable, &observer));
	cl_git_pass(git_observable_subscribe(&subscription2, observable, &observer));

	message.type = 1;
	message.data = &dummy_val;

	/* We increment by two, as we've subscribed twice */
	cl_git_pass(git_subject_on_next(subject, &message));

	/* Disposing of this subscription make us receive just one message now */
	git_subscription_dispose(subscription1);

	cl_git_pass(git_subject_on_next(subject, &message));

	git_subscription_dispose(subscription2);

	/* This one plus the completion shouldn't be received by anybody */
	cl_git_pass(git_subject_on_next(subject, &message));
	cl_git_pass(git_subject_on_completed(subject));

	cl_assert(on_next_called);
	cl_assert(!on_completed_called);
	cl_assert(!on_error_called);

	/* The double-message from the first, plus the second makes three */
	cl_assert_equal_i(3, count);

	git_observable_free(observable);
}
