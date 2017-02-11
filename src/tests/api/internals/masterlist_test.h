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

#ifndef LOOT_TESTS_API_INTERNALS_MASTERLIST_TEST
#define LOOT_TESTS_API_INTERNALS_MASTERLIST_TEST

#include "api/masterlist.h"

#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class MasterlistTest : public CommonGameTestFixture {
protected:
  MasterlistTest() :
    repoBranch("master"),
    oldBranch("old-branch"),
    repoUrl("https://github.com/loot/testing-metadata.git"),
    masterlistPath(localPath / "masterlist.yaml") {}

  void SetUp() {
    CommonGameTestFixture::SetUp();

    ASSERT_FALSE(boost::filesystem::exists(masterlistPath));
    ASSERT_FALSE(boost::filesystem::exists(localPath / ".git"));
  }

  void TearDown() {
    CommonGameTestFixture::TearDown();

    ASSERT_NO_THROW(boost::filesystem::remove(masterlistPath));
  }

  const std::string repoUrl;
  const std::string repoBranch;
  const std::string oldBranch;

  const boost::filesystem::path masterlistPath;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        MasterlistTest,
                        ::testing::Values(
                          GameType::tes4,
                          GameType::tes5,
                          GameType::fo3,
                          GameType::fonv,
                          GameType::fo4,
                          GameType::tes5se));

TEST_P(MasterlistTest, updateWithSeparateParametersShouldThrowIfAnInvalidPathIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update(";//\?", repoUrl, repoBranch), boost::filesystem::filesystem_error);
}

TEST_P(MasterlistTest, updateWithSeparateParametersShouldThrowIfABlankPathIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update("", repoUrl, repoBranch), boost::filesystem::filesystem_error);
}

TEST_P(MasterlistTest, updateWithSeparateParametersShouldThrowIfABranchThatDoesNotExistIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update(masterlistPath,
                                 repoUrl,
                                 "missing-branch"), std::system_error);
}

TEST_P(MasterlistTest, updateWithSeparateParametersShouldThrowIfABlankBranchIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update(masterlistPath, repoUrl, ""), std::invalid_argument);
}

TEST_P(MasterlistTest, updateWithSeparateParametersShouldThrowIfAUrlThatDoesNotExistIsGiven) {
  Masterlist masterlist;

  EXPECT_THROW(masterlist.Update(masterlistPath,
                                 "https://github.com/loot/does-not-exist.git",
                                 repoBranch), std::system_error);
}

TEST_P(MasterlistTest, updateWithSeparateParametersShouldThrowIfABlankUrlIsGiven) {
  Masterlist masterlist;
  EXPECT_THROW(masterlist.Update(masterlistPath, "", repoBranch), std::invalid_argument);
}

TEST_P(MasterlistTest, updateWithSeparateParametersShouldReturnTrueIfNoMasterlistExists) {
  Masterlist masterlist;
  EXPECT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                repoBranch));
}

TEST_P(MasterlistTest, updateWithSeparateParametersShouldReturnFalseIfAnUpToDateMasterlistExists) {
  Masterlist masterlist;

  EXPECT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                repoBranch));

  EXPECT_FALSE(masterlist.Update(masterlistPath,
                                 repoUrl,
                                 repoBranch));
}

TEST_P(MasterlistTest, getInfoShouldThrowIfNoMasterlistExistsAtTheGivenPath) {
  Masterlist masterlist;
  EXPECT_THROW(masterlist.GetInfo(masterlistPath, false), FileAccessError);
}

TEST_P(MasterlistTest, getInfoShouldThrowIfTheGivenPathDoesNotBelongToAGitRepository) {
  ASSERT_NO_THROW(boost::filesystem::copy("./testing-metadata/masterlist.yaml", masterlistPath));

  Masterlist masterlist;
  EXPECT_THROW(masterlist.GetInfo(masterlistPath, false), GitStateError);
}

TEST_P(MasterlistTest, getInfoShouldReturnRevisionAndDateStringsOfTheCorrectLengthsWhenRequestingALongId) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                repoBranch));

  MasterlistInfo info = masterlist.GetInfo(masterlistPath, false);
  EXPECT_EQ(40, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(MasterlistTest, getInfoShouldReturnRevisionAndDateStringsOfTheCorrectLengthsWhenRequestingAShortId) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                repoBranch));

  MasterlistInfo info = masterlist.GetInfo(masterlistPath, true);
  EXPECT_GE((unsigned)40, info.revision_id.length());
  EXPECT_LE((unsigned)7, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_FALSE(info.is_modified);
}

TEST_P(MasterlistTest, getInfoShouldAppendSuffixesToReturnedStringsIfTheMasterlistHasBeenEdited) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                repoBranch));
  boost::filesystem::ofstream out(masterlistPath);
  out.close();

  MasterlistInfo info = masterlist.GetInfo(masterlistPath, false);
  EXPECT_EQ(40, info.revision_id.length());
  EXPECT_EQ(10, info.revision_date.length());
  EXPECT_TRUE(info.is_modified);
}

TEST_P(MasterlistTest, isLatestShouldThrowIfTheGivenPathDoesNotBelongToAGitRepository) {
  ASSERT_NO_THROW(boost::filesystem::copy("./testing-metadata/masterlist.yaml", masterlistPath));

  EXPECT_THROW(Masterlist::IsLatest(masterlistPath, repoBranch), GitStateError);
}

TEST_P(MasterlistTest, isLatestShouldThrowIfTheGivenBranchIsAnEmptyString) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                repoBranch));

  EXPECT_THROW(Masterlist::IsLatest(masterlistPath, ""), std::invalid_argument);
}

TEST_P(MasterlistTest, isLatestShouldReturnFalseIfTheCurrentRevisionIsNotTheLatestRevisionInTheGivenBranch) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                oldBranch));

  EXPECT_FALSE(Masterlist::IsLatest(masterlistPath, repoBranch));
}

TEST_P(MasterlistTest, isLatestShouldReturnTrueIfTheCurrentRevisionIsTheLatestRevisioninTheGivenBranch) {
  Masterlist masterlist;
  ASSERT_TRUE(masterlist.Update(masterlistPath,
                                repoUrl,
                                repoBranch));

  EXPECT_TRUE(Masterlist::IsLatest(masterlistPath, repoBranch));
}
}
}

#endif
