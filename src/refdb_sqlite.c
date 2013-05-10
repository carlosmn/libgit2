#include <git2.h>
#include <string.h>
#include <git2/sys/refdb_backend.h>
#include <git2/sys/refs.h>

#include <sqlite3.h>
#include "refs.h"

#define TABLE_NAME "refs"

typedef struct {
	git_refdb_backend parent;
	sqlite3 *db;
	sqlite3_stmt *st_read;
	sqlite3_stmt *st_write;
	sqlite3_stmt *st_read_all;
	sqlite3_stmt *st_delete;
} refdb_sqlite_backend ;

static int refdb_sqlite__exists(int *exists, git_refdb_backend *_backend, const char *name)
{
	refdb_sqlite_backend *backend = (refdb_sqlite_backend *) _backend;

	sqlite3_reset(backend->st_read);
	if (sqlite3_bind_text(backend->st_read, 1, name, strlen(name), SQLITE_TRANSIENT) == SQLITE_OK) {
		*exists = sqlite3_step(backend->st_read) == SQLITE_ROW;

		return 0;
	}

	return -1;
}

static int refdb_sqlite__lookup(git_reference **out, git_refdb_backend *_backend, const char *name)
{
	refdb_sqlite_backend *backend = (refdb_sqlite_backend *) _backend;
	git_reference *ref;

	sqlite3_reset(backend->st_read);
	if (sqlite3_bind_text(backend->st_read, 1, name, strlen(name), SQLITE_TRANSIENT) == SQLITE_OK) {
		int symbolic;
		const char *target;

		if (sqlite3_step(backend->st_read) != SQLITE_ROW)
			return -1;

		symbolic = sqlite3_column_int(backend->st_read, 0);
		target = (const char *) sqlite3_column_text(backend->st_read, 1);

		if (symbolic) {
			ref = git_reference__alloc_symbolic(name, target);
		} else {
			git_oid oid;

			git_oid_fromstr(&oid, target);
			ref = git_reference__alloc(name, &oid, NULL);
		}

		if (!ref)
			return -1;

		*out = ref;
		return 0;
	}

	return -1;
}

int refdb_sqlite__write(git_refdb_backend *_backend, const git_reference *ref)
{
	refdb_sqlite_backend *backend = (refdb_sqlite_backend *) _backend;
	int symbolic, error;
	const char *name;
	char *target;


	symbolic = ref->type == GIT_REF_SYMBOLIC;
	name = ref->name;
	if (symbolic)
		target = ref->target.symbolic;
	else
		target = git_oid_allocfmt(&ref->target.oid);


	error = SQLITE_ERROR;

	sqlite3_reset(backend->st_write);
	if ((error = sqlite3_bind_text(backend->st_write, 1, name, strlen(name), SQLITE_TRANSIENT)) != SQLITE_OK ||
	    (error = sqlite3_bind_int(backend->st_write, 2, symbolic)) != SQLITE_OK ||
	    (error = sqlite3_bind_text(backend->st_write, 3, target, strlen(target), SQLITE_TRANSIENT)) != SQLITE_OK) {
		git__free(target);
		return -1;
	}

	error = sqlite3_step(backend->st_write);

	error = (error == SQLITE_DONE) ? 0 : -1;

	if (!symbolic)
		git__free(target);

	return error;
}

int refdb_sqlite__delete(git_refdb_backend *_backend, const git_reference *ref)
{
	refdb_sqlite_backend *backend = (refdb_sqlite_backend *) _backend;
	const char *name = ref->name;

	sqlite3_reset(backend->st_delete);
	if (sqlite3_bind_text(backend->st_delete, 1, name, strlen(name), SQLITE_TRANSIENT) != SQLITE_OK)
		return -1;

	if (sqlite3_step(backend->st_delete) != SQLITE_DONE)
		return -1;

	return 0;
}

int refdb_sqlite__foreach(git_refdb_backend *_backend, unsigned int list_flags, git_reference_foreach_cb callback, void *payload)
{
	refdb_sqlite_backend *backend = (refdb_sqlite_backend *) _backend;

	GIT_UNUSED(list_flags);

	sqlite3_reset(backend->st_read_all);
	while (sqlite3_step(backend->st_read_all) == SQLITE_ROW) {
		const char *name;

		name = (const char *) sqlite3_column_text(backend->st_read_all, 0);
		giterr_clear();
		if (callback(name, payload)) {
			return GIT_EUSER;
		}
	}

	return 0;
}


static int create_table(sqlite3 *db)
{
	static const char *sql_creat =
		"CREATE TABLE '" TABLE_NAME "' ("
		"'name' TEXT PRIMARY KEY NOT NULL,"
		"'symbolic' BOOLEAN NOT NULL,"
		"'target' TEXT NOT NULL"
		");";

	if (sqlite3_exec(db, sql_creat, NULL, NULL, NULL) != SQLITE_OK)
		return -1;

	return 0;
}

static int init_statements(refdb_sqlite_backend *backend)
{
	static const char *sql_read =
		"SELECT symbolic, target FROM '" TABLE_NAME "' WHERE name = ?;";

	static const char *sql_read_all =
		"SELECT name, symbolic, target FROM '" TABLE_NAME "';";

	static const char *sql_write =
		"INSERT OR REPLACE INTO '" TABLE_NAME "' VALUES (?, ?, ?);";

	static const char *sql_delete =
		"DELETE FROM '" TABLE_NAME "' WHERE name = ?;";

	if (sqlite3_prepare_v2(backend->db, sql_read, -1, &backend->st_read, NULL) != SQLITE_OK)
		return -1;

	if (sqlite3_prepare_v2(backend->db, sql_read_all, -1, &backend->st_read_all, NULL) != SQLITE_OK)
		return -1;

	if (sqlite3_prepare_v2(backend->db, sql_write, -1, &backend->st_write, NULL) != SQLITE_OK)
		return -1;

	if (sqlite3_prepare_v2(backend->db, sql_delete, -1, &backend->st_delete, NULL) != SQLITE_OK)
		return -1;

		return 0;
}

static int init_db(sqlite3 *db)
{
	static const char *sql_check =
		"SELECT name FROM sqlite_master WHERE type='table' AND name='" TABLE_NAME "';";

	sqlite3_stmt *st_check;
	int error;

	if (sqlite3_prepare_v2(db, sql_check, -1, &st_check, NULL) != SQLITE_OK)
		return GIT_ERROR;

	switch (sqlite3_step(st_check)) {
	case SQLITE_DONE:
		/* the table was not found */
		error = create_table(db);
		break;

	case SQLITE_ROW:
		/* the table was found */
		error = 0;
		break;

	default:
		error = -1;
		break;
	}

	sqlite3_finalize(st_check);
	return error;
}

static void refdb_sqlite__free(git_refdb_backend *_backend)
{
	refdb_sqlite_backend *backend = (refdb_sqlite_backend *) _backend;

	sqlite3_finalize(backend->st_read);
	sqlite3_finalize(backend->st_read_all);
	sqlite3_finalize(backend->st_write);
	sqlite3_finalize(backend->st_delete);
	sqlite3_close(backend->db);
	git__free(backend);
}

int git_refdb_backend_sqlite(git_refdb_backend **out, const char *path)
{
	refdb_sqlite_backend *backend = calloc(1, sizeof(refdb_sqlite_backend));
	int error;

	if (sqlite3_open(path, &backend->db) != SQLITE_OK)
		goto cleanup;

	error = init_db(backend->db);
	if (error < 0)
		goto cleanup;

	error = init_statements(backend);
	if (error < 0)
		goto cleanup;

	backend->parent.lookup = refdb_sqlite__lookup;
	backend->parent.exists = refdb_sqlite__exists;
	backend->parent.write = refdb_sqlite__write;
	backend->parent.delete = refdb_sqlite__delete;
	backend->parent.foreach = refdb_sqlite__foreach;
	backend->parent.free = refdb_sqlite__free;

	*out = (git_refdb_backend *) backend;

	return 0;

cleanup:
	free(backend);

	return -1;
}
