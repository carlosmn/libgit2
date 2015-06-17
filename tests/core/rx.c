#include "clar_libgit2.h"
#include "rx.h"

static int dummy_val;

static git_subject *subject;

void test_core_rx__initialize(void)
{
	cl_git_pass(git_subject_new(&subject));
}

void test_core_rx__cleanup(void)
{
	git_subject_free(subject);
}

static bool on_next_called;
static int on_next(git_observer *observer, git_message *msg)
{
	GIT_UNUSED(observer);

	cl_assert_equal_i(1, msg->type);
	cl_assert_equal_p(&dummy_val, msg->data);
	on_next_called = true;

	return 0;
}

static bool on_completed_called;
static int on_completed(git_observer *observer)
{
	GIT_UNUSED(observer);

	on_completed_called = true;

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

	cl_git_pass(git_observable_new(&observable, subject));
	cl_git_pass(git_observable_subscribe(&subscription, observable, &observer));

	message.type = 1;
	message.data = &dummy_val;
	cl_git_pass(git_subject_on_next(subject, &message));

	cl_git_pass(git_subject_on_completed(subject));

	cl_assert(on_next_called);
	cl_assert(on_completed_called);

	git_subscription_dispose(subscription);
	git_observable_free(observable);
}







