#include "clar_libgit2.h"
#include "path.h"
#include "fileops.h"
#include "remote.h"

git_repository *repo;
git_remote *remote;
void test_network_fetch__initialize(void)
{
	cl_git_pass(git_repository_init(&repo, "fetched", false));
}

void test_network_fetch__cleanup(void)
{
	git_remote_free(remote);
	git_repository_free(repo);

	if (git_path_isdir("fetched"))
		git_futils_rmdir_r("fetched", GIT_DIRREMOVAL_FILES_AND_DIRS);
}

static int update_cb(const char *refname, const git_oid *a, const git_oid *b)
{
	const char *action;
	char a_str[GIT_OID_HEXSZ+1], b_str[GIT_OID_HEXSZ+1];

	git_oid_fmt(b_str, b);
	b_str[GIT_OID_HEXSZ] = '\0';

	if (git_oid_iszero(a)) {
		printf("[new]     %.20s %s\n", b_str, refname);
	} else {
		git_oid_fmt(a_str, a);
		a_str[GIT_OID_HEXSZ] = '\0';
		printf("[updated] %.10s..%.10s %s\n", a_str, b_str, refname);
	}

	return 0;
}

void test_network_fetch__can_fetch_the_whole_libgit2_repository_through_http(void)
{
	git_off_t bytes = 0;
	git_indexer_stats stats;
	stats.processed = 0;
	stats.total = 0;

	cl_git_pass(git_remote_new(&remote, repo, "libgit2", "http://github.com/libgit2/libgit2.git", "refs/*:refs/*"));

	cl_git_pass(git_remote_connect(remote, GIT_DIR_FETCH));
	cl_git_pass(git_remote_download(remote, &bytes, &stats));

	git_remote_disconnect(remote);

	cl_git_pass(git_remote_update_tips(remote, update_cb));
}


/*
# *** network::createremotethenload ***
ok 144 - parsing
# *** network::fetch ***
not ok 145 - can_fetch_the_whole_libgit2_repository_through_http
  ---
  message : Function call failed: git_remote_download(remote, &bytes, &stats)
  severity: fail
  suite   : network::fetch
  test    : can_fetch_the_whole_libgit2_repository_through_http
  file    : ..\..\libgit2\tests-clar\network\fetch.c
  line    : 53
  description: Failed to rename lockfile to 'C:/Users/Emeric/AppData/Local/Temp/
clar_tmp_a06660/fetched/.git/objects/pack/pack-e879f765b620a23d2731c285eb81bb57f
949e736.pack': No such file or directory
  ...
# *** network::remotelocal ***
ok 146 - nested_tags_are_completely_peeled
ok 147 - retrieve_advertised_references

*/

/*
ok 142 - sort1
ok 143 - write
# *** network::createremotethenload ***
ok 144 - parsing
# *** network::fetch ***
not ok 145 - can_fetch_the_whole_libgit2_repository_through_http
  ---
  message : Function call failed: git_remote_download(remote, &bytes, &stats)
  severity: fail
  suite   : network::fetch
  test    : can_fetch_the_whole_libgit2_repository_through_http
  file    : ..\..\libgit2\tests-clar\network\fetch.c
  line    : 53
  description: Found invalid hex digit in length
  ...
# *** network::remotelocal ***
ok 146 - nested_tags_are_completely_peeled
ok 147 - retrieve_advertised_references
*/
