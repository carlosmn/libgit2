/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_git_task_h__
#define INCLUDE_git_task_h__

/**
 * Represents an asynchronous task
 */
typedef struct git_task git_task;

/**
 * The type for the callback used for completion notification.
 */
typedef void (*git_task_finished_cb)(void *payload);

/**
 * Check if the task has finished;
 *
 * @param task the task to query
 * @return 1 if the task has finished, 0 otherwise
 */
GIT_EXTERN(int) git_task_finished(git_task *task);

/**
 * Free a task
 */
GIT_EXTERN(void) git_task_free(git_task *task);

#endif
