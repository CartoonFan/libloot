/*  LOOT

A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
Fallout: New Vegas.

Copyright (C) 2014-2016    WrinklyNinja

This file is part of LOOT.

LOOT is free software: you can redistribute
it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

LOOT is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LOOT.  If not, see
<https://www.gnu.org/licenses/>.
*/

#ifndef LOOT_TESTS_API_INTERNALS_HELPERS_GIT_HELPER_TEST
#define LOOT_TESTS_API_INTERNALS_HELPERS_GIT_HELPER_TEST

#include "api/helpers/git_helper.h"

#include <gtest/gtest.h>

#include "loot/exception/git_state_error.h"

namespace loot {
namespace test {
class GitHelperTest : public ::testing::Test {
protected:
  GitHelperTest() :
    parentRepoRoot(GetRepoRoot()) {}

  inline void SetUp() {
    ASSERT_TRUE(boost::filesystem::exists(parentRepoRoot / "README.md"));

    // Create a backup of CONTRIBUTING.md.
    ASSERT_TRUE(boost::filesystem::exists(parentRepoRoot / "CONTRIBUTING.md"));
    ASSERT_FALSE(boost::filesystem::exists(parentRepoRoot / "CONTRIBUTING.md.copy"));
    ASSERT_NO_THROW(boost::filesystem::copy(parentRepoRoot / "CONTRIBUTING.md", parentRepoRoot / "CONTRIBUTING.md.copy"));
    ASSERT_TRUE(boost::filesystem::exists(parentRepoRoot / "CONTRIBUTING.md.copy"));

    // Edit CONTRIBUTING.md
    boost::filesystem::ofstream out(parentRepoRoot / "CONTRIBUTING.md");
    out.close();
  }

  inline void TearDown() {
      // Restore original CONTRIBUTING.md
    ASSERT_NO_THROW(boost::filesystem::remove(parentRepoRoot / "CONTRIBUTING.md"));
    ASSERT_NO_THROW(boost::filesystem::rename(parentRepoRoot / "CONTRIBUTING.md.copy", parentRepoRoot / "CONTRIBUTING.md"));
    ASSERT_TRUE(boost::filesystem::exists(parentRepoRoot / "CONTRIBUTING.md"));
    ASSERT_FALSE(boost::filesystem::exists(parentRepoRoot / "CONTRIBUTING.md.copy"));
  }

  GitHelper git_;

  const boost::filesystem::path parentRepoRoot;

private:
  inline static boost::filesystem::path GetRepoRoot() {
    boost::filesystem::path dir = boost::filesystem::current_path();
    while (!boost::filesystem::exists(dir / ".git")) {
      dir = dir.parent_path();
    }

    return dir;
  }
};

TEST_F(GitHelperTest, repoShouldInitialiseAsANullPointer) {
  EXPECT_EQ(nullptr, git_.GetData().repo);
}

TEST_F(GitHelperTest, destructorShouldCallLibgit2CleanupFunction) {
  ASSERT_EQ(2, git_libgit2_init());

  GitHelper * gitPointer = new GitHelper();
  ASSERT_EQ(4, git_libgit2_init());

  delete gitPointer;
  EXPECT_EQ(2, git_libgit2_shutdown());
}

TEST_F(GitHelperTest, callShouldNotThrowIfPassedAZeroValue) {
  EXPECT_NO_THROW(git_.Call(0));
}

TEST_F(GitHelperTest, callShouldThrowIfPassedANonZeroValue) {
  EXPECT_THROW(git_.Call(1), std::system_error);
  EXPECT_THROW(git_.Call(-1), std::system_error);
}

TEST_F(GitHelperTest, isRepositoryShouldReturnTrueForARepositoryRoot) {
  EXPECT_TRUE(GitHelper::IsRepository(parentRepoRoot));
}

TEST_F(GitHelperTest, isRepositoryShouldReturnFalseForRepositorySubdirectory) {
  EXPECT_FALSE(GitHelper::IsRepository(boost::filesystem::current_path()));
}

TEST_F(GitHelperTest, isFileDifferentShouldThrowIfGivenANonRepositoryPath) {
  EXPECT_THROW(GitHelper::IsFileDifferent(boost::filesystem::current_path(), "README.md"), GitStateError);
}

TEST_F(GitHelperTest, isFileDifferentShouldReturnFalseForAnUntrackedFile) {
    // New files not in the index are not tracked by Git, so aren't considered
    // different.
  EXPECT_FALSE(GitHelper::IsFileDifferent(parentRepoRoot, "CONTRIBUTING.md.copy"));
}

TEST_F(GitHelperTest, isFileDifferentShouldReturnFalseForAnUnchangedTrackedFile) {
  EXPECT_FALSE(GitHelper::IsFileDifferent(parentRepoRoot, "README.md"));
}

TEST_F(GitHelperTest, isFileDifferentShouldReturnTrueForAChangedTrackedFile) {
  EXPECT_TRUE(GitHelper::IsFileDifferent(parentRepoRoot, "CONTRIBUTING.md"));
}
}
}

#endif
