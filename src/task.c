/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "task.h"

static int seterr_nothreads(void)
{
	giterr_set(GITERR_THREAD, "libgit2 was built without thread support");
	return -1;
}

int git_task_new(git_task **out, git_task_entrypoint entry,  git_task_finished_cb cb, void *payload)
{
	git_task *task;

	assert(out && entry);

	task = git__calloc(1, sizeof(git_task));
	GITERR_CHECK_ALLOC(task);

	task->entry = entry;
	task->finished_cb = cb;
	task->payload = payload;

	*out = task;
	return 0;
}

static void *wrap_task_entry(void *payload)
{
	git_task *task = payload;

	task->exit_code = task->entry(task);
	task->finished_cb(task->payload);

	return NULL;
}

int git_task_start(git_task *task)
{
#ifdef GIT_THREADS
	int error;

	if ((error = git_thread_create(&task->thread, NULL, wrap_task_entry, task)) < 0)
		giterr_set(GITERR_OS, "failed to create thread");

	return error;
#else
	GIT_UNUSED(task);
	return seterr_nothreads();
#endif
}

int git_task_finished(git_task *task)
{
	return task->finished;
}

int git_task_join(int *exit_code, git_task *task)
{
#ifdef GIT_THREADS
	int error;

	if ((error = git_thread_join(&task->thread, NULL)) < 0)
		return error;

	*exit_code = task->exit_code;

	return 0;
#else
	GIT_UNUSED(exit_code);
	GIT_UNUSED(task);

	return seterr_nothreads();
#endif
}

void git_task_free(git_task *task)
{
	task->on_free(task);
	git__free(task);
}
