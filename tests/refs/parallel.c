#include "clar_libgit2.h"
#include "repository.h"

git_thread _iterator;
git_thread _creator;
git_thread _packer;
int _stop;
git_oid _id;

void *iterator_thread(void *data)
{
	git_repository *repo;
	const char *path = (const char *) data;

	cl_git_pass(git_repository_open(&repo, path));


	while (!_stop) {
		git_reference_iterator *iter;
		const char *name;
		int error;

		cl_git_pass(git_reference_iterator_new(&iter, repo));

		while ((error = git_reference_next_name(&name, iter)) == 0) {
			/* just spin */
		}

		git_reference_iterator_free(iter);
	}

	git_repository_free(repo);

	return NULL;
}

void *packer_thread(void *data)
{
	git_repository *repo;
	const char *path = (const char *) data;
	git_refdb *db;
	int error;

	cl_git_pass(git_repository_open(&repo, path));
	cl_git_pass(git_repository_refdb(&db, repo));

	while (!_stop) {
		do {
			error = git_refdb_compress(db);
		} while (error == GIT_ELOCKED);

		cl_git_pass(error);
	}

	git_refdb_free(db);
	git_repository_free(repo);

	return NULL;
}

static size_t n = 0;
void *creator_thread(void *data)
{
	git_repository *repo;
	const char *path = (const char *) data;
	git_buf name = GIT_BUF_INIT, rename = GIT_BUF_INIT;

	cl_git_pass(git_repository_open(&repo, path));

	while (!_stop) {
		git_reference *ref;
		git_buf_clear(&name);
		cl_git_pass(git_buf_printf(&name, "refs/something%" PRIuZ "/ref%" PRIuZ, n, n));
		cl_git_pass(git_reference_create(&ref, repo, name.ptr, &_id, 0, "create one reference"));
		git_reference_free(ref);

		if (n > 0) {
			git_reference *ref, *ref2;
			int error;

			git_buf_clear(&name);
			cl_git_pass(git_buf_printf(&name, "refs/something%" PRIuZ "/ref%" PRIuZ, n-1, n-1));

			cl_git_pass(git_reference_lookup(&ref, repo, name.ptr));
			git_buf_clear(&rename);
			cl_git_pass(git_buf_printf(&rename, "refs/something%" PRIuZ, n-1));
			do {
				error = git_reference_rename(&ref2, ref, rename.ptr, 0, "rename so we delete a dir");
			} while (error == GIT_ELOCKED);

			cl_git_pass(error);

			git_reference_free(ref);
			git_reference_free(ref2);
		}

		n++;
	}

	git_repository_free(repo);

	return NULL;
}

/*
 * Test that we handle concurrent operations (at the fileystem level)
 * well. We spawn three threads, one keeps creating references,
 * another keeps iterating over the refs and a third wants to pack them up
 */
void test_refs_parallel__cleaning_up(void)
{
	const char *path;
	git_repository *repo;
	git_odb *db;
	long start;
	size_t i;

#ifndef GIT_THREADS
	cl_skip():
#endif
	path = "parallel_repo.git";
	cl_git_pass(git_repository_init(&repo, path, true));
	cl_git_pass(git_repository_odb__weakptr(&db, repo));
	cl_git_pass(git_odb_write(&_id, db, "foo\n", strlen("foo\n"), GIT_OBJ_BLOB));

	git_repository_free(repo);

	cl_git_pass(git_thread_create(&_iterator, NULL, iterator_thread, (void *) path));
	cl_git_pass(git_thread_create(&_iterator, NULL, creator_thread, (void *) path));
	cl_git_pass(git_thread_create(&_iterator, NULL, packer_thread, (void *) path));

	start = git__timer();

	while (git__timer() - start < 10) {
		sleep(1);
	}

	_stop = 1;
	sleep(2);

	/* Now we check that we have all the references we want */
	packer_thread((void *) path);

	printf("n is at %zu\n", n);

	cl_git_pass(git_repository_open(&repo, path));


	{
		git_reference_iterator *iter;
		const char *name;
		size_t nrefs = 0;
		int error;

		cl_git_pass(git_reference_iterator_new(&iter, repo));

		while ((error = git_reference_next_name(&name, iter)) == 0) {
			nrefs++;
		}

		printf("There were %zu refs\n", nrefs);
		git_reference_iterator_free(iter);
	}

	git_repository_free(repo);

	
	for (i = 0; i < n - 1; i++) {
		git_reference *ref;
		git_buf buf = GIT_BUF_INIT;

		cl_git_pass(git_buf_printf(&buf, "refs/something%" PRIuZ, i));
		cl_git_pass(git_reference_lookup(&ref, repo, buf.ptr));
	}
}
