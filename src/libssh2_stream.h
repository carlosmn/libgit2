/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_libssh2_stream_h__
#define INCLUDE_libssh2_stream_h__

#include "netops.h"
#include "socket_stream.h"

typedef struct {
	git_stream parent;
	git_socket_stream *socket;
	LIBSSH2_SESSION *session;
	LIBSSH2_CHANNEL *channel;
	char *cmd;
	int sent_command;
	git_cert_hostkey cert;
} git_libssh2_stream;

extern int git_libssh2_stream_new(git_stream **out, const char *host, const char *port, const char *cmd);

#endif
