/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_task_h__
#define INCLUDE_task_h__

#include "common.h"
#include "thread-utils.h"
#include "git2/task.h"

typedef int (*git_task_entrypoint)(git_task *task);

struct git_task {
	git_task_entrypoint entry;
	git_thread thread;

	git_task_finished_cb finished_cb;
	void *payload;
	
	int exit_code;
	void *exit_value;
	unsigned int finished;

	void (*on_free)(git_task *task);
};

/**
 * Allocate and initialize a task
 *
 * The new task won't be scheduled yet.
 *
 * @param out the task
 * @param entry the entry-point for this task
 * @param cb will be called when the entry point returns
 * @param payload data to keep around
 */
int git_task_new(git_task **out, git_task_entrypoint entry, git_task_finished_cb cb, void *payload);

/**
 * Schedule the task for execution
 */
int git_task_start(git_task *task);

/**
 * Join the task's thread
 *
 * Wait for the thread which is running the task to finish.
 *
 * @param the task's exit code will be written into this variable
 * @param task the task to wait for
 * @return 0 on success or an error code
 */
int git_task_join(int *exit_code, git_task *task);

#endif
