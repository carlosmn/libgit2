#include "clar_libgit2.h"

#include "buffer.h"
#include "posix.h"
#include "path.h"
#include "refs.h"
#include "refdb_sqlite.h"

static git_repository *repo;

void test_refdb_sqlitedb__initialize(void)
{
	git_buf db_file = GIT_BUF_INIT;
	git_refdb *refdb;
	git_refdb_backend *refdb_backend;

	repo = cl_git_sandbox_init("testrepo.git");

	cl_git_pass(git_buf_printf(&db_file, "%s/refs.db", git_repository_path(repo)));
	cl_git_pass(git_repository_refdb(&refdb, repo));
	cl_git_pass(git_refdb_backend_sqlite(&refdb_backend, git_buf_cstr(&db_file)));
	cl_git_pass(git_refdb_set_backend(refdb, refdb_backend));

	git_buf_free(&db_file);
	git_refdb_free(refdb);
}

void test_refdb_sqlitedb__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

void test_refdb_sqlitedb__add(void)
{
	git_reference *ref;
	git_oid oid;

	cl_git_pass(git_oid_fromstr(&oid, "c47800c7266a2be04c571c04d5a6614691ea99bd"));
	cl_git_pass(git_reference_create(&ref, repo, "refs/heads/master", &oid, 1));
	git_reference_free(ref);

	cl_git_pass(git_reference_create(&ref, repo, "refs/heads/foo", &oid, 1));
	git_reference_free(ref);

	cl_git_pass(git_reference_symbolic_create(&ref, repo, "HEAD", "refs/heads/master", 1));
	git_reference_free(ref);

	cl_git_pass(git_reference_lookup(&ref, repo, "refs/heads/master"));
	git_reference_free(ref);

	cl_git_pass(git_reference_lookup(&ref, repo, "HEAD"));
	git_reference_free(ref);

	cl_git_pass(git_reference_lookup(&ref, repo, "refs/heads/foo"));
	cl_git_pass(git_reference_delete(ref));
	git_reference_free(ref);

	cl_git_fail(git_reference_lookup(&ref, repo, "refs/heads/foo"));
}
