#include "clar_libgit2.h"
#include "git2/sys/odb_backend.h"

static git_odb *g_odb;
static git_repository *g_repo;
static git_odb_backend *g_odb_backend;

/* define our very own tracing odb backend */
typedef struct {
	git_odb_backend parent;
	size_t nwrites;
} tracing_backend;

static int tracing_backend__write(git_odb_backend *_backend, const git_oid *id, const void *data, size_t len, git_otype type)
{
	tracing_backend *backend = (tracing_backend *) _backend;

	GIT_UNUSED(id);
	GIT_UNUSED(data);
	GIT_UNUSED(len);
	GIT_UNUSED(type);

	backend->nwrites++;

	return GIT_PASSTHROUGH;
}

static int tracing_backend__writestream(git_odb_stream **out, git_odb_backend *_backend, size_t len, git_otype type)
{
	tracing_backend *backend = (tracing_backend *) _backend;

	GIT_UNUSED(out);
	GIT_UNUSED(len);
	GIT_UNUSED(type);

	backend->nwrites++;

	return GIT_PASSTHROUGH;
}

static int tracing_backend_new(git_odb_backend **out)
{
	tracing_backend *backend;

	backend = git__calloc(1, sizeof(tracing_backend));
	GITERR_CHECK_ALLOC(backend);

	backend->parent.version = 1;
	backend->parent.write = tracing_backend__write;
	backend->parent.writestream = tracing_backend__writestream;

	*out = (git_odb_backend *) backend;

	return 0;
}

void test_odb_tracing__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
	cl_git_pass(git_repository_odb(&g_odb, g_repo));
	cl_git_pass(tracing_backend_new(&g_odb_backend));
	cl_git_pass(git_odb_add_backend(g_odb, g_odb_backend, 3));
}

void test_odb_tracing__cleanup(void)
{
	git_odb_free(g_odb);
	cl_git_sandbox_cleanup();
}

static void assert_writes(size_t expected)
{
	tracing_backend *backend = (tracing_backend *) g_odb_backend;

	cl_assert_equal_i(expected, backend->nwrites);
}

void test_odb_tracing__write(void)
{
	const char *data = "some data\n";
	const size_t data_len = strlen(data);
	git_oid id;
	char id_str[GIT_OID_HEXSZ + 1] = {0};

	cl_git_pass(git_odb_write(&id, g_odb, data, data_len, GIT_OBJ_BLOB));
	git_oid_fmt(id_str, &id);

	cl_assert_equal_s("426863280eedd08aa600ac034e6a9933ba372944", id_str);
	assert_writes(1);
}

void test_odb_tracing__writestream(void)
{
	const char *data = "some data\n";
	const size_t data_len = strlen(data);
	git_odb_stream *stream;
	git_oid id;
	char id_str[GIT_OID_HEXSZ + 1] = {0};

	cl_git_pass(git_odb_open_wstream(&stream, g_odb, data_len, GIT_OBJ_BLOB));
	cl_git_pass(git_odb_stream_write(stream, data, data_len));
	cl_git_pass(git_odb_stream_finalize_write(&id, stream));

	git_oid_fmt(id_str, &id);

	cl_assert_equal_s("426863280eedd08aa600ac034e6a9933ba372944", id_str);
	assert_writes(1);

	git_odb_stream_free(stream);
}
