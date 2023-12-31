#include "clar_libgit2.h"
#include "refs.h"
#include "config/config_helpers.h"

static git_repository *repo;

void test_refs_branches_move__initialize(void)
{
	repo = cl_git_sandbox_init("testrepo.git");
}

void test_refs_branches_move__cleanup(void)
{
	cl_git_sandbox_cleanup();
}

#define NEW_BRANCH_NAME "new-branch-on-the-block"

void test_refs_branches_move__can_move_a_local_branch(void)
{
	git_reference *original_ref, *new_ref;

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

	cl_git_pass(git_branch_move(&new_ref, original_ref, NEW_BRANCH_NAME, 0));
	cl_assert_equal_s(GIT_REFS_HEADS_DIR NEW_BRANCH_NAME, git_reference_name(new_ref));

	git_reference_free(original_ref);
	git_reference_free(new_ref);
}

void test_refs_branches_move__can_move_a_local_branch_to_a_different_namespace(void)
{
	git_reference *original_ref, *new_ref, *newer_ref;

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

	/* Downward */
	cl_git_pass(git_branch_move(&new_ref, original_ref, "somewhere/" NEW_BRANCH_NAME, 0));
	git_reference_free(original_ref);

	/* Upward */
	cl_git_pass(git_branch_move(&newer_ref, new_ref, "br2", 0));
	git_reference_free(new_ref);

	git_reference_free(newer_ref);
}

void test_refs_branches_move__can_move_a_local_branch_to_a_partially_colliding_namespace(void)
{
	git_reference *original_ref, *new_ref, *newer_ref;

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

	/* Downward */
	cl_git_pass(git_branch_move(&new_ref, original_ref, "br2/" NEW_BRANCH_NAME, 0));
	git_reference_free(original_ref);

	/* Upward */
	cl_git_pass(git_branch_move(&newer_ref, new_ref, "br2", 0));
	git_reference_free(new_ref);

	git_reference_free(newer_ref);
}

void test_refs_branches_move__can_not_move_a_branch_if_its_destination_name_collide_with_an_existing_one(void)
{
	git_reference *original_ref, *new_ref;
	git_config *config;
	git_buf original_remote = GIT_BUF_INIT,
	        original_merge = GIT_BUF_INIT;
	const char *str;

	cl_git_pass(git_repository_config_snapshot(&config, repo));

	cl_git_pass(git_config_get_string_buf(&original_remote, config, "branch.master.remote"));
	cl_git_pass(git_config_get_string_buf(&original_merge, config, "branch.master.merge"));
	git_config_free(config);

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

	cl_assert_equal_i(GIT_EEXISTS,
		git_branch_move(&new_ref, original_ref, "master", 0));

	cl_assert(git_error_last()->message != NULL);

	cl_git_pass(git_repository_config_snapshot(&config, repo));
	cl_git_pass(git_config_get_string(&str, config, "branch.master.remote"));
	cl_assert_equal_s(original_remote.ptr, str);
	cl_git_pass(git_config_get_string(&str, config, "branch.master.merge"));
	cl_assert_equal_s(original_merge.ptr, str);
	git_config_free(config);

	cl_assert_equal_i(GIT_EEXISTS,
		git_branch_move(&new_ref, original_ref, "cannot-fetch", 0));

	cl_assert(git_error_last()->message != NULL);

	cl_git_pass(git_repository_config_snapshot(&config, repo));
	cl_git_pass(git_config_get_string(&str, config, "branch.master.remote"));
	cl_assert_equal_s(original_remote.ptr, str);
	cl_git_pass(git_config_get_string(&str, config, "branch.master.merge"));
	cl_assert_equal_s(original_merge.ptr, str);
	git_config_free(config);

	git_reference_free(original_ref);
	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/track-local"));

	cl_assert_equal_i(GIT_EEXISTS,
		git_branch_move(&new_ref, original_ref, "master", 0));

	cl_assert(git_error_last()->message != NULL);

	cl_git_pass(git_repository_config_snapshot(&config, repo));
	cl_git_pass(git_config_get_string(&str, config, "branch.master.remote"));
	cl_assert_equal_s(original_remote.ptr, str);
	cl_git_pass(git_config_get_string(&str, config, "branch.master.merge"));
	cl_assert_equal_s(original_merge.ptr, str);

	git_buf_dispose(&original_remote);
	git_buf_dispose(&original_merge);
	git_reference_free(original_ref);
	git_config_free(config);
}

void test_refs_branches_move__moving_a_branch_with_an_invalid_name_returns_EINVALIDSPEC(void)
{
	git_reference *original_ref, *new_ref;

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

	cl_assert_equal_i(GIT_EINVALIDSPEC, git_branch_move(&new_ref, original_ref, "Inv@{id", 0));

	git_reference_free(original_ref);
}

void test_refs_branches_move__can_not_move_a_non_branch(void)
{
	git_reference *tag, *new_ref;

	cl_git_pass(git_reference_lookup(&tag, repo, "refs/tags/e90810b"));
	cl_git_fail(git_branch_move(&new_ref, tag, NEW_BRANCH_NAME, 0));

	git_reference_free(tag);
}

void test_refs_branches_move__can_force_move_over_an_existing_branch(void)
{
	git_reference *original_ref, *new_ref;

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

	cl_git_pass(git_branch_move(&new_ref, original_ref, "master", 1));

	git_reference_free(original_ref);
	git_reference_free(new_ref);
}

void test_refs_branches_move__moving_a_branch_moves_related_configuration_data(void)
{
	git_reference *branch;
	git_reference *new_branch;

	cl_git_pass(git_branch_lookup(&branch, repo, "track-local", GIT_BRANCH_LOCAL));

	assert_config_entry_existence(repo, "branch.track-local.remote", true);
	assert_config_entry_existence(repo, "branch.track-local.merge", true);
	assert_config_entry_existence(repo, "branch.moved.remote", false);
	assert_config_entry_existence(repo, "branch.moved.merge", false);

	cl_git_pass(git_branch_move(&new_branch, branch, "moved", 0));
	git_reference_free(branch);

	assert_config_entry_existence(repo, "branch.track-local.remote", false);
	assert_config_entry_existence(repo, "branch.track-local.merge", false);
	assert_config_entry_existence(repo, "branch.moved.remote", true);
	assert_config_entry_existence(repo, "branch.moved.merge", true);

	git_reference_free(new_branch);
}

void test_refs_branches_move__moving_the_branch_pointed_at_by_HEAD_updates_HEAD(void)
{
	git_reference *branch;
	git_reference *new_branch;

	cl_git_pass(git_reference_lookup(&branch, repo, "refs/heads/master"));
	cl_git_pass(git_branch_move(&new_branch, branch, "master2", 0));
	git_reference_free(branch);
	git_reference_free(new_branch);

	cl_git_pass(git_repository_head(&branch, repo));
	cl_assert_equal_s("refs/heads/master2", git_reference_name(branch));
	git_reference_free(branch);
}

void test_refs_branches_move__can_move_with_unicode(void)
{
	git_reference *original_ref, *new_ref;
	const char *new_branch_name = "\x41\xCC\x8A\x73\x74\x72\x6F\xCC\x88\x6D";

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));
	cl_git_pass(git_branch_move(&new_ref, original_ref, new_branch_name, 0));

	if (cl_repo_get_bool(repo, "core.precomposeunicode"))
		cl_assert_equal_s(GIT_REFS_HEADS_DIR "\xC3\x85\x73\x74\x72\xC3\xB6\x6D", git_reference_name(new_ref));
	else
		cl_assert_equal_s(GIT_REFS_HEADS_DIR "\x41\xCC\x8A\x73\x74\x72\x6F\xCC\x88\x6D", git_reference_name(new_ref));

	git_reference_free(original_ref);
	git_reference_free(new_ref);
}

void test_refs_branches_move__moves_reflog_correctly(void)
{
	git_reference *original_ref, *new_ref;
    git_reflog *original_reflog, *new_reflog;

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

    cl_git_pass(git_reflog_read(&original_reflog, repo, "refs/heads/br2"));
    cl_assert_equal_i(2, git_reflog_entrycount(original_reflog));

	cl_git_pass(git_branch_move(&new_ref, original_ref, NEW_BRANCH_NAME, 0));
	cl_assert_equal_s(GIT_REFS_HEADS_DIR NEW_BRANCH_NAME, git_reference_name(new_ref));

    cl_git_pass(git_reflog_read(&new_reflog, repo, GIT_REFS_HEADS_DIR NEW_BRANCH_NAME));
    cl_assert_equal_i(3, git_reflog_entrycount(new_reflog));

    git_reference_free(original_ref);
	git_reference_free(new_ref);
    git_reflog_free(original_reflog);
    git_reflog_free(new_reflog);
}

void test_refs_branches_move__failed_move_restores_reflog(void)
{
	git_reference *original_ref, *new_ref;
	git_reflog *recovered_reflog;

	cl_git_pass(git_reference_lookup(&original_ref, repo, "refs/heads/br2"));

	cl_assert_equal_i(GIT_EINVALIDSPEC, git_branch_move(&new_ref, original_ref, "Inv@{id", 0));

	cl_git_pass(git_reflog_read(&recovered_reflog, repo, "refs/heads/br2"));
	cl_assert_equal_i(2, git_reflog_entrycount(recovered_reflog));

	git_reference_free(original_ref);
	git_reflog_free(recovered_reflog);
}
