/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#ifdef GIT_SSH

#include <libssh2.h>

#include "common.h"
#include "posix.h"
#include "netops.h"
#include "git2/transport.h"
#include "stream.h"
#include "socket_stream.h"
#include "libssh2_stream.h"

static void ssh_error(LIBSSH2_SESSION *session, const char *errmsg)
{
	char *ssherr;
	libssh2_session_last_error(session, &ssherr, NULL, 0);

	giterr_set(GITERR_SSH, "%s: %s", errmsg, ssherr);
}

int libssh2_connect(git_stream *stream)
{
	git_libssh2_stream *st = (git_libssh2_stream *) stream;
	LIBSSH2_SESSION* s;
	int error;

	if ((error = git_stream_connect((git_stream *)st->socket)) < 0)
		return error;

	s = libssh2_session_init();
	if (!s) {
		giterr_set(GITERR_NET, "failed to initialize SSH session");
		return -1;
	}

	do {
		error = libssh2_session_startup(s, st->socket->s);
	} while (LIBSSH2_ERROR_EAGAIN == error || LIBSSH2_ERROR_TIMEOUT == error);

	if (error != LIBSSH2_ERROR_NONE) {
		ssh_error(s, "Failed to start SSH session");
		libssh2_session_free(s);
		return -1;
	}

	libssh2_session_set_blocking(s, 1);

	st->session = s;
	return 0;
}

int libssh2_certificate(git_cert **out, git_stream *stream)
{
	git_libssh2_stream *st = (git_libssh2_stream *) stream;
	const char *key;

	st->cert.cert_type = GIT_CERT_HOSTKEY_LIBSSH2;

	key = libssh2_hostkey_hash(st->session, LIBSSH2_HOSTKEY_HASH_SHA1);
	if (key != NULL) {
		st->cert.type |= GIT_CERT_SSH_SHA1;
		memcpy(&st->cert.hash_sha1, key, 20);
	}

	key = libssh2_hostkey_hash(st->session, LIBSSH2_HOSTKEY_HASH_MD5);
	if (key != NULL) {
		st->cert.type |= GIT_CERT_SSH_MD5;
		memcpy(&st->cert.hash_md5, key, 16);
	}

	if (st->cert.type == 0) {
		giterr_set(GITERR_SSH, "unable to get the host key");
		return -1;
	}

	*out = (git_cert *) &st->cert;
	return 0;
}

static int send_command(git_libssh2_stream *st)
{
	int error;

	error = libssh2_channel_exec(st->channel, st->cmd);
	if (error < LIBSSH2_ERROR_NONE) {
		ssh_error(st->session, "failed to execute SSH command");
		return -1;
	}

	st->sent_command = 1;
	return 0;
}

ssize_t libssh2_read(git_stream *stream, void *data, size_t len)
{
	git_libssh2_stream *st = (git_libssh2_stream *) stream;
	ssize_t ret;

	if (!st->sent_command && send_command(st) < 0)
		return -1;

	if ((ret = libssh2_channel_read(st->channel, data, len)) < LIBSSH2_ERROR_NONE) {
		ssh_error(st->session, "SSH could not read data");;
		return -1;
	}

	return ret;
}

ssize_t libssh2_write(git_stream *stream, void *data, size_t len, int flags)
{
	git_libssh2_stream *st = (git_libssh2_stream *) stream;
	size_t off = 0;
	int ret;

	GIT_UNUSED(flags);

	if (!st->sent_command && send_command(st) < 0)
		return -1;

	do {
		ret = libssh2_channel_write(st->channel, data + off, len - off);
		if (ret < 0)
			break;

		off += ret;

	} while (off < len);

	if (ret < 0) {
		ssh_error(st->session, "SSH could not write data");
		return -1;
	}

	return len;
}

int libssh2_close(git_stream *stream)
{
	git_libssh2_stream *st = (git_libssh2_stream *) stream;
	int error;

	if (st->channel && (error = libssh2_channel_close(st->channel)) < 0) {
		return error;
	}

	return git_stream_close((git_stream *)st->socket);
}

void libssh2_stream_free(git_stream *stream)
{
	git_libssh2_stream *st = (git_libssh2_stream *) stream;

	if (st->channel) {
		libssh2_channel_free(st->channel);
		st->channel = NULL;
	}

	if (st->session) {
		libssh2_session_free(st->session);
		st->session = NULL;
	}

	git_stream_free((git_stream *)st->socket);
	st->socket = NULL;
	git__free(st->cmd);
	git__free(st);
}

int git_libssh2_stream_new(git_stream **out, const char *host, const char *port, const char *cmd)
{
	git_libssh2_stream *st;

	assert(out && host && cmd);

	st = git__calloc(1, sizeof(git_libssh2_stream));
	GITERR_CHECK_ALLOC(st);

	if (git_socket_stream_new((git_stream **) &st->socket, host, port))
		return -1;

	st->cmd = git__strdup(cmd);
	GITERR_CHECK_ALLOC(st->cmd);
	st->parent.encrypted = 1;
	st->parent.connect = libssh2_connect;
	st->parent.certificate = libssh2_certificate;
	st->parent.read = libssh2_read;
	st->parent.write = libssh2_write;
	st->parent.free = libssh2_stream_free;

	*out = (git_stream *) st;
	return 0;
}

#else

int git_libssh2_stream_new(git_stream **out, const char *host, const char *port)
{
	giterr_set("ssh is not supported in this version");
	return -1;
}


#endif
