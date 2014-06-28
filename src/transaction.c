/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "repository.h"
#include "strmap.h"
#include "refdb.h"
#include "pool.h"
#include "git2/signature.h"
#include "git2/sys/refs.h"

GIT__USE_STRMAP;

typedef struct {
	const char *name;
	void *payload;

	git_ref_t ref_type;
	union {
		git_oid id;
		char *symbolic;
	} target;

	const char *message;
	git_signature *sig;

	unsigned int set_target :1,
		set_reflog :1,
		committed :1;
} transaction_node;

struct git_transaction {
	git_repository *repo;
	git_refdb *db;

	git_strmap *locks;
	git_pool pool;
};

int git_transaction_new(git_transaction **out, git_repository *repo)
{
	int error;
	git_pool pool;
	git_transaction *tx = NULL;

	assert(out && repo);

	if ((error = git_pool_init(&pool, 1, 0)) < 0)
		return error;

	tx = git_pool_mallocz(&pool, sizeof(git_transaction));
	if (!tx) {
		error = -1;
		goto on_error;
	}

	if ((error = git_strmap_alloc(&tx->locks)) < 0) {
		error = -1;
		goto on_error;
	}

	if ((error = git_repository_refdb(&tx->db, repo)) < 0)
		goto on_error;

	memcpy(&tx->pool, &pool, sizeof(git_pool));
	tx->repo = repo;
	*out = tx;
	return 0;

on_error:
	git_pool_clear(&pool);
	return error;
}

int git_transaction_lock(git_transaction *tx, const char *refname)
{
	int error;
	transaction_node *node;

	assert(tx && refname);

	node = git_pool_mallocz(&tx->pool, sizeof(transaction_node));
	GITERR_CHECK_ALLOC(node);

	node->name = git_pool_strdup(&tx->pool, refname);
	GITERR_CHECK_ALLOC(node->name);

	if ((error = git_refdb_lock(&node->payload, tx->db, refname)) < 0)
		return error;

	git_strmap_insert(tx->locks, node->name, node, error);
	if (error < 0) 
		goto cleanup;

	return 0;

cleanup:
	git_refdb_unlock(tx->db, node->payload, false, NULL, NULL, NULL);

	return error;
}

int git_transaction_set_target(git_transaction *tx, const char *refname, git_oid *target, const git_signature *sig, const char *msg)
{
	git_strmap_iter pos;
	transaction_node *node;

	assert(tx && refname && target);

	pos = git_strmap_lookup_index(tx->locks, refname);
	if (!git_strmap_valid_index(tx->locks, pos)) {
		giterr_set(GITERR_REFERENCE, "the specified reference is not locked");
		return GIT_ENOTFOUND;
	}

	node = git_strmap_value_at(tx->locks, pos);
	if (sig && git_signature_dup(&node->sig, sig) < 0)
		return -1;

	if (!node->sig && git_reference__log_signature(&node->sig, tx->repo) < 0)
		return -1;

	if (msg) {
		node->message = git_pool_strdup(&tx->pool, msg);
		GITERR_CHECK_ALLOC(node->message);
	}

	git_oid_cpy(&node->target.id, target);
	node->ref_type = GIT_REF_OID;
	node->set_target = 1;

	return 0;
}

int git_transaction_set_symbolic_target(git_transaction *tx, const char *refname, const char *target, const git_signature *sig, const char *msg)
{
	git_strmap_iter pos;
	transaction_node *node;

	assert(tx && refname && target);

	pos = git_strmap_lookup_index(tx->locks, refname);
	if (!git_strmap_valid_index(tx->locks, pos)) {
		giterr_set(GITERR_REFERENCE, "the specified reference is not locked");
		return GIT_ENOTFOUND;
	}

	node = git_strmap_value_at(tx->locks, pos);
	if (sig && git_signature_dup(&node->sig, sig) < 0)
		return -1;

	if (msg) {
		node->message = git_pool_strdup(&tx->pool, msg);
		GITERR_CHECK_ALLOC(node->message);
	}

	node->target.symbolic = git_pool_strdup(&tx->pool, target);
	GITERR_CHECK_ALLOC(node->target.symbolic);
	node->ref_type = GIT_REF_SYMBOLIC;
	node->set_target = 1;

	return 0;
}

int git_transaction_commit(git_transaction *tx)
{
	transaction_node *node;
	git_strmap_iter pos;
	git_reference *ref;
	int error;

	assert(tx);

	for (pos = kh_begin(tx->locks); pos < kh_end(tx->locks); pos++) {
		if (!git_strmap_has_data(tx->locks, pos))
			continue;

		node = git_strmap_value_at(tx->locks, pos);
		if (!node->set_target) {
			node->committed = true;
			if ((error = git_refdb_unlock(tx->db, node->payload, false, NULL, NULL, NULL)) < 0)
				return error;

			continue;
		}

		if (node->ref_type == GIT_REF_OID) {
			ref = git_reference__alloc(node->name, &node->target.id, NULL);
		} else if (node->ref_type == GIT_REF_SYMBOLIC) {
			ref = git_reference__alloc_symbolic(node->name, node->target.symbolic);
		} else {
			assert(0);
		}

		GITERR_CHECK_ALLOC(ref);

		if (node->ref_type == GIT_REF_OID) {
			error = git_refdb_unlock(tx->db, node->payload, true, ref, node->sig, node->message);
		} else if (node->ref_type == GIT_REF_SYMBOLIC) {
			error = git_refdb_unlock(tx->db, node->payload, true, ref, node->sig, node->message);
		} else {
			assert(0);
		}

		git_reference_free(ref);
		node->committed = true;

		if (error < 0)
			return error;
	}

	return 0;
}

void git_transaction_free(git_transaction *tx)
{
	transaction_node *node;
	git_pool pool;
	git_strmap_iter pos;

	assert(tx);

	/* start by unlocking the ones we've left hanging, if any */
	for (pos = kh_begin(tx->locks); pos < kh_end(tx->locks); pos++) {
		if (!git_strmap_has_data(tx->locks, pos))
			continue;

		node = git_strmap_value_at(tx->locks, pos);
		if (node->committed)
			continue;

		git_refdb_unlock(tx->db, node->payload, false, NULL, NULL, NULL);
	}

	git_refdb_free(tx->db);
	git_strmap_foreach_value(tx->locks, node, {
		git_signature_free(node->sig);
	});
	git_strmap_free(tx->locks);

	/* tx is inside the pool, so we need to extract the data */
	memcpy(&pool, &tx->pool, sizeof(git_pool));
	git_pool_clear(&pool);
}
