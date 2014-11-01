/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_socket_stream_h__
#define INCLUDE_socket_stream_h__

#include "git/sys/stream.h"

extern int git_openssl_stream_new(git_stream **out, const char *host, const char *port);

#endif
