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

#ifndef LOOT_TESTS_API_INTERNALS_METADATA_LIST_TEST
#define LOOT_TESTS_API_INTERNALS_METADATA_LIST_TEST

#include "api/metadata_list.h"

#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class MetadataListTest : public CommonGameTestFixture {
protected:
  MetadataListTest() :
      metadataPath("./testing-metadata/masterlist.yaml"),
      savedMetadataPath("./testing-metadata/saved.masterlist.yaml"),
      missingMetadataPath("./missing-metadata.yaml"),
      invalidMetadataPaths({"./testing-metadata/invalid/non_map_root.yaml",
                            "./testing-metadata/invalid/non_unique.yaml"}) {}

  inline virtual void SetUp() {
    CommonGameTestFixture::SetUp();

    ASSERT_TRUE(boost::filesystem::exists(metadataPath));
    ASSERT_FALSE(boost::filesystem::exists(savedMetadataPath));

    for (const auto& path : invalidMetadataPaths) {
      ASSERT_TRUE(boost::filesystem::exists(path));
    }
  }

  inline virtual void TearDown() {
    CommonGameTestFixture::TearDown();

    ASSERT_TRUE(boost::filesystem::exists(metadataPath));
    ASSERT_NO_THROW(boost::filesystem::remove(savedMetadataPath));

    for (const auto& path : invalidMetadataPaths) {
      ASSERT_TRUE(boost::filesystem::exists(path));
    }
  }

  static std::string PluginMetadataToString(const PluginMetadata& metadata) {
    return metadata.GetName();
  }

  const boost::filesystem::path metadataPath;
  const boost::filesystem::path savedMetadataPath;
  const boost::filesystem::path groupMetadataPath;
  const boost::filesystem::path missingMetadataPath;
  const std::vector<boost::filesystem::path> invalidMetadataPaths;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(, MetadataListTest, ::testing::Values(GameType::tes4));

TEST_P(MetadataListTest, loadShouldLoadGlobalMessages) {
  MetadataList metadataList;

  EXPECT_NO_THROW(metadataList.Load(metadataPath));
  EXPECT_EQ(std::vector<Message>({
                Message(MessageType::say, "A global message."),
            }),
            metadataList.Messages());
}

TEST_P(MetadataListTest, loadShouldLoadPluginMetadata) {
  MetadataList metadataList;

  EXPECT_NO_THROW(metadataList.Load(metadataPath));
  // Non-regex plugins can be outputted in any order, and regex entries can
  // match each other, so convert the list to a set of strings for
  // comparison.
  std::list<PluginMetadata> result(metadataList.Plugins());
  std::set<std::string> names;
  std::transform(
      begin(result),
      end(result),
      std::insert_iterator<std::set<std::string>>(names, begin(names)),
      &MetadataListTest::PluginMetadataToString);

  EXPECT_EQ(std::set<std::string>({
                blankEsm,
                blankEsp,
                "Blank.+\\.esp",
                "Blank.+(Different)?.*\\.esp",
            }),
            names);
}

TEST_P(MetadataListTest, loadShouldLoadBashTags) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  EXPECT_EQ(std::set<std::string>({"C.Climate", "Relev"}),
            metadataList.BashTags());
}

TEST_P(MetadataListTest, loadShouldLoadGroups) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  auto groups = metadataList.Groups();

  EXPECT_EQ(3, groups.size());

  EXPECT_EQ(1, groups.count(Group("default")));
  EXPECT_TRUE(groups.find(Group("default"))->GetAfterGroups().empty());

  EXPECT_EQ(1, groups.count(Group("group1")));
  EXPECT_EQ(std::unordered_set<std::string>({ "group2" }), groups.find(Group("group1"))->GetAfterGroups());

  EXPECT_EQ(1, groups.count(Group("group2")));
  EXPECT_EQ(std::unordered_set<std::string>({ "default" }), groups.find(Group("group2"))->GetAfterGroups());
}

TEST_P(MetadataListTest, loadShouldThrowIfAnInvalidMetadataFileIsGiven) {
  MetadataList ml;
  for (const auto& path : invalidMetadataPaths) {
    EXPECT_THROW(ml.Load(path), FileAccessError);
  }
}

TEST_P(MetadataListTest,
       loadShouldClearExistingDataIfAnInvalidMetadataFileIsGiven) {
  MetadataList metadataList;

  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  EXPECT_THROW(metadataList.Load(blankEsm), FileAccessError);
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_P(MetadataListTest,
       loadShouldClearExistingDataIfAMissingMetadataFileIsGiven) {
  MetadataList metadataList;

  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  EXPECT_THROW(metadataList.Load(missingMetadataPath), FileAccessError);
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_P(MetadataListTest, saveShouldWriteTheLoadedMetadataToTheGivenFilePath) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  EXPECT_NO_THROW(metadataList.Save(savedMetadataPath));

  EXPECT_TRUE(boost::filesystem::exists(savedMetadataPath));

  // Check the new file contains the same metadata.
  EXPECT_NO_THROW(metadataList.Load(savedMetadataPath));

  EXPECT_EQ(std::set<std::string>({"C.Climate", "Relev"}),
            metadataList.BashTags());

  EXPECT_EQ(std::unordered_set<Group>({ Group("default"), Group("group1"), Group("group2") }),
    metadataList.Groups());

  EXPECT_EQ(std::vector<Message>({
                Message(MessageType::say, "A global message."),
            }),
            metadataList.Messages());

  // Non-regex plugins can be outputted in any order, and regex entries can
  // match each other, so convert the list to a set of strings for
  // comparison.
  std::list<PluginMetadata> result(metadataList.Plugins());
  std::set<std::string> names;
  std::transform(
      begin(result),
      end(result),
      std::insert_iterator<std::set<std::string>>(names, begin(names)),
      &MetadataListTest::PluginMetadataToString);
  EXPECT_EQ(std::set<std::string>({
                blankEsm,
                blankEsp,
                "Blank.+\\.esp",
                "Blank.+(Different)?.*\\.esp",
            }),
            names);
}

TEST_P(MetadataListTest, clearShouldClearLoadedData) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_FALSE(metadataList.Messages().empty());
  ASSERT_FALSE(metadataList.Plugins().empty());
  ASSERT_FALSE(metadataList.BashTags().empty());

  metadataList.Clear();
  EXPECT_TRUE(metadataList.Messages().empty());
  EXPECT_TRUE(metadataList.Plugins().empty());
  EXPECT_TRUE(metadataList.BashTags().empty());
}

TEST_P(MetadataListTest, setGroupsShouldReplaceExistingGroups) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  metadataList.SetGroups({
    Group("group4")
  });

  auto groups = metadataList.Groups();

  EXPECT_EQ(2, groups.size());

  EXPECT_EQ(1, groups.count(Group("default")));
  EXPECT_TRUE(groups.find(Group("default"))->GetAfterGroups().empty());

  EXPECT_EQ(1, groups.count(Group("group4")));
  EXPECT_TRUE(groups.find(Group("group4"))->GetAfterGroups().empty());
}

TEST_P(
    MetadataListTest,
    findPluginShouldReturnAnEmptyPluginObjectIfTheGivenPluginIsNotInTheMetadataList) {
  MetadataList metadataList;
  PluginMetadata plugin =
      metadataList.FindPlugin(PluginMetadata(blankDifferentEsm));

  EXPECT_EQ(blankDifferentEsm, plugin.GetName());
  EXPECT_TRUE(plugin.HasNameOnly());
}

TEST_P(
    MetadataListTest,
    findPluginShouldReturnTheMetadataObjectInTheMetadataListIfOneExistsForTheGivenPlugin) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin =
      metadataList.FindPlugin(PluginMetadata(blankDifferentEsp));

  EXPECT_EQ(blankDifferentEsp, plugin.GetName());
  EXPECT_EQ(std::set<File>({
                File(blankEsm),
            }),
            plugin.GetLoadAfterFiles());
  EXPECT_EQ(std::set<File>({
                File(blankEsp),
            }),
            plugin.GetIncompatibilities());
}

TEST_P(MetadataListTest, addPluginShouldStoreGivenSpecificPluginMetadata) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));
  ASSERT_TRUE(
      metadataList.FindPlugin(PluginMetadata(blankDifferentEsm)).HasNameOnly());

  PluginMetadata plugin(blankDifferentEsm);
  plugin.SetGroup("group1");
  metadataList.AddPlugin(plugin);

  plugin = metadataList.FindPlugin(plugin);

  EXPECT_EQ(blankDifferentEsm, plugin.GetName());
  EXPECT_EQ("group1", plugin.GetGroup());
}

TEST_P(MetadataListTest, addPluginShouldStoreGivenRegexPluginMetadata) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin(".+Dependent\\.esp");
  plugin.SetGroup("group1");
  metadataList.AddPlugin(plugin);

  plugin = metadataList.FindPlugin(PluginMetadata(blankPluginDependentEsp));

  EXPECT_EQ("group1", plugin.GetGroup());
}

TEST_P(MetadataListTest, addPluginShouldThrowIfAMatchingPluginAlreadyExists) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(PluginMetadata(blankEsm));
  ASSERT_EQ(blankEsm, plugin.GetName());
  ASSERT_FALSE(plugin.HasNameOnly());

  EXPECT_THROW(metadataList.AddPlugin(PluginMetadata(blankEsm)),
               std::invalid_argument);
}

TEST_P(MetadataListTest,
       erasePluginShouldRemoveStoredMetadataForTheGivenPlugin) {
  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(PluginMetadata(blankEsp));
  ASSERT_EQ(blankEsp, plugin.GetName());
  ASSERT_FALSE(plugin.HasNameOnly());

  metadataList.ErasePlugin(plugin);

  plugin = metadataList.FindPlugin(plugin);
  EXPECT_EQ(blankEsp, plugin.GetName());
  EXPECT_TRUE(plugin.HasNameOnly());
}

TEST_P(
    MetadataListTest,
    evalAllConditionsShouldEvaluateTheConditionsForThePluginsStoredInTeMetadataList) {
  Game game(GetParam(), dataPath.parent_path(), localPath);
  ConditionEvaluator evaluator(game.Type(),
                               game.DataPath(),
                               game.GetCache(),
                               game.GetLoadOrderHandler());

  MetadataList metadataList;
  ASSERT_NO_THROW(metadataList.Load(metadataPath));

  PluginMetadata plugin = metadataList.FindPlugin(PluginMetadata(blankEsm));
  ASSERT_EQ(
      std::vector<Message>({
          Message(MessageType::warn, "This is a warning."),
          Message(MessageType::say,
                  "This message should be removed when evaluating conditions."),
      }),
      plugin.GetMessages());

  plugin = metadataList.FindPlugin(PluginMetadata(blankEsp));
  ASSERT_EQ(blankEsp, plugin.GetName());
  ASSERT_FALSE(plugin.HasNameOnly());

  EXPECT_NO_THROW(metadataList.EvalAllConditions(evaluator));

  plugin = metadataList.FindPlugin(PluginMetadata(blankEsm));
  EXPECT_EQ(std::vector<Message>({
                Message(MessageType::warn, "This is a warning."),
            }),
            plugin.GetMessages());

  plugin = metadataList.FindPlugin(PluginMetadata(blankEsp));
  EXPECT_EQ(blankEsp, plugin.GetName());
  EXPECT_TRUE(plugin.GetDirtyInfo().empty());
}
}
}

#endif
