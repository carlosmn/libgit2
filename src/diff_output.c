#include "git2/submodule.h"
#include "diff_output.h"
#define KNOWN_BINARY_FLAGS (GIT_DIFF_FILE_BINARY|GIT_DIFF_FILE_NOT_BINARY)
#define NOT_BINARY_FLAGS   (GIT_DIFF_FILE_NOT_BINARY|GIT_DIFF_FILE_NO_DATA)
static int diff_delta_is_binary_by_attr(
	diff_context *ctxt, git_diff_patch *patch)
	git_diff_delta *delta = patch->delta;
	if (ctxt->opts && (ctxt->opts->flags & GIT_DIFF_FORCE_TEXT) != 0) {
		delta->new_file.flags |= (delta->old_file.flags & KNOWN_BINARY_FLAGS);
	diff_context *ctxt, git_diff_delta *delta, git_diff_file *file, git_map *map)
	GIT_UNUSED(ctxt);

	if ((file->flags & KNOWN_BINARY_FLAGS) == 0) {
	update_delta_is_binary(delta);
	diff_context *ctxt, git_diff_delta *delta, git_diff_file *file)
	if ((file->flags & KNOWN_BINARY_FLAGS) != 0)
	update_delta_is_binary(delta);
	const git_diff_options *opts, xdemitconf_t *cfg, xpparam_t *param)

	diff_context *ctxt,
	git_diff_delta *delta,
	if (file->mode == GIT_FILEMODE_COMMIT)
	{
		char oidstr[GIT_OID_HEXSZ+1];
		git_buf content = GIT_BUF_INIT;

		git_oid_fmt(oidstr, &file->oid);
		oidstr[GIT_OID_HEXSZ] = 0;
		git_buf_printf(&content, "Subproject commit %s\n", oidstr );

		map->data = git_buf_detach(&content);
		map->len = strlen(map->data);

		file->flags |= GIT_DIFF_FILE_FREE_DATA;
		return 0;
	}

	if ((error = diff_delta_is_binary_by_size(ctxt, delta, file)) < 0)
	if (delta->binary == 1)
	return diff_delta_is_binary_by_content(ctxt, delta, file, map);
}

static int get_workdir_sm_content(
	diff_context *ctxt,
	git_diff_file *file,
	git_map *map)
{
	int error = 0;
	git_buf content = GIT_BUF_INIT;
	git_submodule* sm = NULL;
	unsigned int sm_status = 0;
	const char* sm_status_text = "";
	char oidstr[GIT_OID_HEXSZ+1];

	if ((error = git_submodule_lookup(&sm, ctxt->repo, file->path)) < 0 ||
		(error = git_submodule_status(&sm_status, sm)) < 0)
		return error;

	/* update OID if we didn't have it previously */
	if ((file->flags & GIT_DIFF_FILE_VALID_OID) == 0) {
		const git_oid* sm_head;

		if ((sm_head = git_submodule_wd_oid(sm)) != NULL ||
			(sm_head = git_submodule_head_oid(sm)) != NULL)
		{
			git_oid_cpy(&file->oid, sm_head);
			file->flags |= GIT_DIFF_FILE_VALID_OID;
		}
	}

	git_oid_fmt(oidstr, &file->oid);
	oidstr[GIT_OID_HEXSZ] = '\0';

	if (GIT_SUBMODULE_STATUS_IS_WD_DIRTY(sm_status))
		sm_status_text = "-dirty";

	git_buf_printf(&content, "Subproject commit %s%s\n",
				   oidstr, sm_status_text);

	map->data = git_buf_detach(&content);
	map->len = strlen(map->data);

	file->flags |= GIT_DIFF_FILE_FREE_DATA;

	return 0;
	diff_context *ctxt,
	git_diff_delta *delta,
	if (S_ISGITLINK(file->mode))
		return get_workdir_sm_content(ctxt, file, map);

	if (S_ISDIR(file->mode))
		return 0;

		if ((error = diff_delta_is_binary_by_size(ctxt, delta, file)) < 0 ||
			delta->binary == 1)
		error = diff_delta_is_binary_by_content(ctxt, delta, file, map);

static void diff_context_init(
	diff_context *ctxt,
	git_diff_list *diff,
	git_repository *repo,
	const git_diff_options *opts,
	void *data,
	git_diff_file_fn file_cb,
	git_diff_hunk_fn hunk_cb,
	git_diff_data_fn data_cb)
	memset(ctxt, 0, sizeof(diff_context));
	ctxt->diff = diff;
	ctxt->file_cb = file_cb;
	ctxt->hunk_cb = hunk_cb;
	ctxt->data_cb = data_cb;
	ctxt->cb_data = data;
	ctxt->cb_error = 0;
	setup_xdiff_options(ctxt->opts, &ctxt->xdiff_config, &ctxt->xdiff_params);
static int diff_delta_file_callback(
	diff_context *ctxt, git_diff_delta *delta, size_t idx)
	float progress;

	if (!ctxt->file_cb)
		return 0;

	progress = ctxt->diff ? ((float)idx / ctxt->diff->deltas.length) : 1.0f;

	if (ctxt->file_cb(ctxt->cb_data, delta, progress) != 0)
		ctxt->cb_error = GIT_EUSER;

	return ctxt->cb_error;
static void diff_patch_init(
	diff_context *ctxt, git_diff_patch *patch)
	memset(patch, 0, sizeof(git_diff_patch));
	patch->diff = ctxt->diff;
	patch->ctxt = ctxt;
	if (patch->diff) {
		patch->old_src = patch->diff->old_src;
		patch->new_src = patch->diff->new_src;
	} else {
		patch->old_src = patch->new_src = GIT_ITERATOR_TREE;
	}
static git_diff_patch *diff_patch_alloc(
	diff_context *ctxt, git_diff_delta *delta)
	git_diff_patch *patch = git__malloc(sizeof(git_diff_patch));
	if (!patch)
		return NULL;
	diff_patch_init(ctxt, patch);
	git_diff_list_addref(patch->diff);
	GIT_REFCOUNT_INC(patch);
	patch->delta = delta;
	patch->flags = GIT_DIFF_PATCH_ALLOCATED;

	return patch;
static int diff_patch_load(
	diff_context *ctxt, git_diff_patch *patch)
	git_diff_delta *delta = patch->delta;
	if ((patch->flags & GIT_DIFF_PATCH_LOADED) != 0)
	error = diff_delta_is_binary_by_attr(ctxt, patch);
	patch->old_data.data = "";
	patch->old_data.len  = 0;
	patch->old_blob      = NULL;
	patch->new_data.data = "";
	patch->new_data.len  = 0;
	patch->new_blob      = NULL;
	if (!ctxt->hunk_cb &&
		!ctxt->data_cb &&
		(ctxt->opts->flags & GIT_DIFF_SKIP_BINARY_CHECK) != 0)
		goto cleanup;

	case GIT_DELTA_UNTRACKED:
		delta->old_file.flags |= GIT_DIFF_FILE_NO_DATA;
		if ((ctxt->opts->flags & GIT_DIFF_INCLUDE_UNTRACKED_CONTENT) == 0)
			delta->new_file.flags |= GIT_DIFF_FILE_NO_DATA;
		break;
		patch->old_src == GIT_ITERATOR_WORKDIR) {
				ctxt, delta, &delta->old_file, &patch->old_data)) < 0)
		patch->new_src == GIT_ITERATOR_WORKDIR) {
				ctxt, delta, &delta->new_file, &patch->new_data)) < 0)
		patch->old_src != GIT_ITERATOR_WORKDIR) {
				ctxt, delta, &delta->old_file,
				&patch->old_data, &patch->old_blob)) < 0)
		patch->new_src != GIT_ITERATOR_WORKDIR) {
				ctxt, delta, &delta->new_file,
				&patch->new_data, &patch->new_blob)) < 0)
	if (!error) {
		patch->flags |= GIT_DIFF_PATCH_LOADED;
		if (delta->binary != 1 &&
			delta->status != GIT_DELTA_UNMODIFIED &&
			(patch->old_data.len || patch->new_data.len) &&
			!git_oid_equal(&delta->old_file.oid, &delta->new_file.oid))
			patch->flags |= GIT_DIFF_PATCH_DIFFABLE;
	}
static int diff_patch_cb(void *priv, mmbuffer_t *bufs, int len)
	git_diff_patch *patch = priv;
	diff_context   *ctxt  = patch->ctxt;
		ctxt->cb_error = parse_hunk_header(&ctxt->cb_range, bufs[0].ptr);
		if (ctxt->cb_error < 0)
		if (ctxt->hunk_cb != NULL &&
			ctxt->hunk_cb(ctxt->cb_data, patch->delta, &ctxt->cb_range,
		if (ctxt->data_cb != NULL &&
			ctxt->data_cb(ctxt->cb_data, patch->delta, &ctxt->cb_range,
				origin, bufs[1].ptr, bufs[1].size))
		if (ctxt->data_cb != NULL &&
			ctxt->data_cb(ctxt->cb_data, patch->delta, &ctxt->cb_range,
				origin, bufs[2].ptr, bufs[2].size))
static int diff_patch_generate(
	diff_context *ctxt, git_diff_patch *patch)
	if ((patch->flags & GIT_DIFF_PATCH_DIFFED) != 0)
	if ((patch->flags & GIT_DIFF_PATCH_LOADED) == 0)
		if ((error = diff_patch_load(ctxt, patch)) < 0)
			return error;
	if ((patch->flags & GIT_DIFF_PATCH_DIFFABLE) == 0)
	if (!ctxt->file_cb && !ctxt->hunk_cb)
		return 0;

	patch->ctxt = ctxt;
	xdiff_callback.outf = diff_patch_cb;
	xdiff_callback.priv = patch;
	old_xdiff_data.ptr  = patch->old_data.data;
	old_xdiff_data.size = patch->old_data.len;
	new_xdiff_data.ptr  = patch->new_data.data;
	new_xdiff_data.size = patch->new_data.len;
	if (!error)
		patch->flags |= GIT_DIFF_PATCH_DIFFED;
static void diff_patch_unload(git_diff_patch *patch)
{
	if ((patch->flags & GIT_DIFF_PATCH_DIFFED) != 0) {
		patch->flags = (patch->flags & ~GIT_DIFF_PATCH_DIFFED);

		patch->hunks_size = 0;
		patch->lines_size = 0;
	}

	if ((patch->flags & GIT_DIFF_PATCH_LOADED) != 0) {
		patch->flags = (patch->flags & ~GIT_DIFF_PATCH_LOADED);

		release_content(
			&patch->delta->old_file, &patch->old_data, patch->old_blob);
		release_content(
			&patch->delta->new_file, &patch->new_data, patch->new_blob);
	}
}

static void diff_patch_free(git_diff_patch *patch)
{
	diff_patch_unload(patch);

	git__free(patch->lines);
	patch->lines = NULL;
	patch->lines_asize = 0;

	git__free(patch->hunks);
	patch->hunks = NULL;
	patch->hunks_asize = 0;

	if (!(patch->flags & GIT_DIFF_PATCH_ALLOCATED))
		return;

	patch->flags = 0;

	git_diff_list_free(patch->diff); /* decrements refcount */

	git__free(patch);
}

#define MAX_HUNK_STEP 128
#define MIN_HUNK_STEP 8
#define MAX_LINE_STEP 256
#define MIN_LINE_STEP 8

static int diff_patch_hunk_cb(
	void *cb_data,
	const git_diff_delta *delta,
	const git_diff_range *range,
	const char *header,
	size_t header_len)
{
	git_diff_patch *patch = cb_data;
	diff_patch_hunk *hunk;

	GIT_UNUSED(delta);

	if (patch->hunks_size >= patch->hunks_asize) {
		size_t new_size;
		diff_patch_hunk *new_hunks;

		if (patch->hunks_asize > MAX_HUNK_STEP)
			new_size = patch->hunks_asize + MAX_HUNK_STEP;
		else
			new_size = patch->hunks_asize * 2;
		if (new_size < MIN_HUNK_STEP)
			new_size = MIN_HUNK_STEP;

		new_hunks = git__realloc(
			patch->hunks, new_size * sizeof(diff_patch_hunk));
		if (!new_hunks)
			return -1;

		patch->hunks = new_hunks;
		patch->hunks_asize = new_size;
	}

	hunk = &patch->hunks[patch->hunks_size++];

	memcpy(&hunk->range, range, sizeof(hunk->range));

	assert(header_len + 1 < sizeof(hunk->header));
	memcpy(&hunk->header, header, header_len);
	hunk->header[header_len] = '\0';
	hunk->header_len = header_len;

	hunk->line_start = patch->lines_size;
	hunk->line_count = 0;

	return 0;
}

static int diff_patch_line_cb(
	void *cb_data,
	const git_diff_delta *delta,
	const git_diff_range *range,
	char line_origin,
	const char *content,
	size_t content_len)
{
	git_diff_patch *patch = cb_data;
	diff_patch_hunk *hunk;
	diff_patch_line *last, *line;

	GIT_UNUSED(delta);
	GIT_UNUSED(range);

	assert(patch->hunks_size > 0);
	assert(patch->hunks != NULL);

	hunk = &patch->hunks[patch->hunks_size - 1];

	if (patch->lines_size >= patch->lines_asize) {
		size_t new_size;
		diff_patch_line *new_lines;

		if (patch->lines_asize > MAX_LINE_STEP)
			new_size = patch->lines_asize + MAX_LINE_STEP;
		else
			new_size = patch->lines_asize * 2;
		if (new_size < MIN_LINE_STEP)
			new_size = MIN_LINE_STEP;

		new_lines = git__realloc(
			patch->lines, new_size * sizeof(diff_patch_line));
		if (!new_lines)
			return -1;

		patch->lines = new_lines;
		patch->lines_asize = new_size;
	}

	last = (patch->lines_size > 0) ?
		&patch->lines[patch->lines_size - 1] : NULL;
	line = &patch->lines[patch->lines_size++];

	line->ptr = content;
	line->len = content_len;
	line->origin = line_origin;

	/* do some bookkeeping so we can provide old/new line numbers */

	for (line->lines = 0; content_len > 0; --content_len) {
		if (*content++ == '\n')
			++line->lines;
	}

	if (!last) {
		line->oldno = hunk->range.old_start;
		line->newno = hunk->range.new_start;
	} else {
		switch (last->origin) {
		case GIT_DIFF_LINE_ADDITION:
			line->oldno = last->oldno;
			line->newno = last->newno + last->lines;
			break;
		case GIT_DIFF_LINE_DELETION:
			line->oldno = last->oldno + last->lines;
			line->newno = last->newno;
			break;
		default:
			line->oldno = last->oldno + last->lines;
			line->newno = last->newno + last->lines;
			break;
		}
	}

	hunk->line_count++;

	return 0;
}


	void *cb_data,
	git_diff_data_fn data_cb)
	diff_context ctxt;
	git_diff_patch patch;

	diff_context_init(
		&ctxt, diff, diff->repo, &diff->opts,
		cb_data, file_cb, hunk_cb, data_cb);
	diff_patch_init(&ctxt, &patch);
	git_vector_foreach(&diff->deltas, idx, patch.delta) {
		/* check flags against patch status */
		if (git_diff_delta__should_skip(ctxt.opts, patch.delta))
		if (!(error = diff_patch_load(&ctxt, &patch))) {
			/* invoke file callback */
			error = diff_delta_file_callback(&ctxt, patch.delta, idx);
			/* generate diffs and invoke hunk and line callbacks */
			if (!error)
				error = diff_patch_generate(&ctxt, &patch);
			diff_patch_unload(&patch);
		}
		giterr_clear(); /* don't let error message leak */

char git_diff_status_char(git_delta_t status)
{
	char code;

	switch (status) {
	case GIT_DELTA_ADDED:     code = 'A'; break;
	case GIT_DELTA_DELETED:   code = 'D'; break;
	case GIT_DELTA_MODIFIED:  code = 'M'; break;
	case GIT_DELTA_RENAMED:   code = 'R'; break;
	case GIT_DELTA_COPIED:    code = 'C'; break;
	case GIT_DELTA_IGNORED:   code = 'I'; break;
	case GIT_DELTA_UNTRACKED: code = '?'; break;
	default:                  code = ' '; break;
	}

	return code;
}

static int print_compact(
	void *data, const git_diff_delta *delta, float progress)
	char old_suffix, new_suffix, code = git_diff_status_char(delta->status);
	if (code == ' ')
static int print_oid_range(diff_print_info *pi, const git_diff_delta *delta)
static int print_patch_file(
	void *data, const git_diff_delta *delta, float progress)
	if (S_ISDIR(delta->new_file.mode))
		return 0;

	if (pi->print_cb(pi->cb_data, delta, NULL, GIT_DIFF_LINE_FILE_HDR, git_buf_cstr(pi->buf), git_buf_len(pi->buf)))
	if (delta->binary != 1)
		return 0;
	const git_diff_delta *d,
	const git_diff_range *r,
	if (S_ISDIR(d->new_file.mode))
		return 0;

	const git_diff_delta *delta,
	const git_diff_range *range,
	if (S_ISDIR(delta->new_file.mode))
		return 0;

		file->mode = 0644;
	const git_diff_options *options,
	git_diff_data_fn data_cb)
	diff_context ctxt;
	git_diff_delta delta;
	git_diff_patch patch;
		git_blob *swap = old_blob;
		old_blob = new_blob;
		new_blob = swap;
	if (new_blob)
		repo = git_object_owner((git_object *)new_blob);
	else if (old_blob)
		repo = git_object_owner((git_object *)old_blob);
	diff_context_init(
		&ctxt, NULL, repo, options,
		cb_data, file_cb, hunk_cb, data_cb);

	diff_patch_init(&ctxt, &patch);
	/* create a fake delta record and simulate diff_patch_load */
	delta.binary = -1;
	set_data_from_blob(old_blob, &patch.old_data, &delta.old_file);
	set_data_from_blob(new_blob, &patch.new_data, &delta.new_file);
	delta.status = new_blob ?
		(old_blob ? GIT_DELTA_MODIFIED : GIT_DELTA_ADDED) :
		(old_blob ? GIT_DELTA_DELETED : GIT_DELTA_UNTRACKED);
	patch.delta = &delta;
	if ((error = diff_delta_is_binary_by_content(
			 &ctxt, &delta, &delta.old_file, &patch.old_data)) < 0 ||
		(error = diff_delta_is_binary_by_content(
			&ctxt, &delta, &delta.new_file, &patch.new_data)) < 0)
	patch.flags |= GIT_DIFF_PATCH_LOADED;
	if (delta.binary != 1 && delta.status != GIT_DELTA_UNMODIFIED)
		patch.flags |= GIT_DIFF_PATCH_DIFFABLE;
	if (!(error = diff_delta_file_callback(&ctxt, patch.delta, 1)))
		error = diff_patch_generate(&ctxt, &patch);
	diff_patch_unload(&patch);

size_t git_diff_num_deltas(git_diff_list *diff)
	assert(diff);
	return (size_t)diff->deltas.length;
size_t git_diff_num_deltas_of_type(git_diff_list *diff, git_delta_t type)
	size_t i, count = 0;
	git_diff_delta *delta;
	assert(diff);
	git_vector_foreach(&diff->deltas, i, delta) {
		count += (delta->status == type);
	return count;
int git_diff_get_patch(
	git_diff_patch **patch_ptr,
	const git_diff_delta **delta_ptr,
	git_diff_list *diff,
	size_t idx)
	diff_context ctxt;
	git_diff_delta *delta;
	git_diff_patch *patch;
	if (patch_ptr)
		*patch_ptr = NULL;
	delta = git_vector_get(&diff->deltas, idx);
	if (!delta) {
		if (delta_ptr)
			*delta_ptr = NULL;
		giterr_set(GITERR_INVALID, "Index out of range for delta in diff");
		return GIT_ENOTFOUND;
	if (delta_ptr)
		*delta_ptr = delta;
	if (!patch_ptr &&
		(delta->binary != -1 ||
		 (diff->opts.flags & GIT_DIFF_SKIP_BINARY_CHECK) != 0))
		return 0;
	diff_context_init(
		&ctxt, diff, diff->repo, &diff->opts,
		NULL, NULL, diff_patch_hunk_cb, diff_patch_line_cb);
	if (git_diff_delta__should_skip(ctxt.opts, delta))
		return 0;
	patch = diff_patch_alloc(&ctxt, delta);
	if (!patch)
		return -1;
	if (!(error = diff_patch_load(&ctxt, patch))) {
		ctxt.cb_data = patch;
		error = diff_patch_generate(&ctxt, patch);
		if (error == GIT_EUSER)
			error = ctxt.cb_error;
	}
	if (error)
		git_diff_patch_free(patch);
	else if (patch_ptr)
		*patch_ptr = patch;
	return error;
void git_diff_patch_free(git_diff_patch *patch)
	if (patch)
		GIT_REFCOUNT_DEC(patch, diff_patch_free);
const git_diff_delta *git_diff_patch_delta(git_diff_patch *patch)
	assert(patch);
	return patch->delta;
size_t git_diff_patch_num_hunks(git_diff_patch *patch)
	assert(patch);
	return patch->hunks_size;
int git_diff_patch_get_hunk(
	const git_diff_range **range,
	size_t *lines_in_hunk,
	git_diff_patch *patch,
	size_t hunk_idx)
	diff_patch_hunk *hunk;
	assert(patch);
	if (hunk_idx >= patch->hunks_size) {
		if (range) *range = NULL;
		if (header) *header = NULL;
		if (header_len) *header_len = 0;
		if (lines_in_hunk) *lines_in_hunk = 0;
		return GIT_ENOTFOUND;
	hunk = &patch->hunks[hunk_idx];
	if (range) *range = &hunk->range;
	if (header) *header = hunk->header;
	if (header_len) *header_len = hunk->header_len;
	if (lines_in_hunk) *lines_in_hunk = hunk->line_count;
	return 0;
}
int git_diff_patch_num_lines_in_hunk(
	git_diff_patch *patch,
	size_t hunk_idx)
{
	assert(patch);
	if (hunk_idx >= patch->hunks_size)
		return GIT_ENOTFOUND;
	else
		return (int)patch->hunks[hunk_idx].line_count;
int git_diff_patch_get_line_in_hunk(
	char *line_origin,
	const char **content,
	int *old_lineno,
	int *new_lineno,
	git_diff_patch *patch,
	size_t hunk_idx,
	size_t line_of_hunk)
	diff_patch_hunk *hunk;
	diff_patch_line *line;
	assert(patch);
	if (hunk_idx >= patch->hunks_size)
		goto notfound;
	hunk = &patch->hunks[hunk_idx];
	if (line_of_hunk >= hunk->line_count)
		goto notfound;
	line = &patch->lines[hunk->line_start + line_of_hunk];
	if (line_origin) *line_origin = line->origin;
	if (content) *content = line->ptr;
	if (content_len) *content_len = line->len;
	if (old_lineno) *old_lineno = line->oldno;
	if (new_lineno) *new_lineno = line->newno;
	return 0;

notfound:
	if (line_origin) *line_origin = GIT_DIFF_LINE_CONTEXT;
	if (content) *content = NULL;
	if (content_len) *content_len = 0;
	if (old_lineno) *old_lineno = -1;
	if (new_lineno) *new_lineno = -1;

	return GIT_ENOTFOUND;
