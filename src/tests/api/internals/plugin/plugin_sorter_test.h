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

#ifndef LOOT_TESTS_API_INTERNALS_PLUGIN_PLUGIN_SORTER_TEST
#define LOOT_TESTS_API_INTERNALS_PLUGIN_PLUGIN_SORTER_TEST

#include "api/plugin/plugin_sorter.h"

#include "loot/exception/cyclic_interaction_error.h"
#include "tests/common_game_test_fixture.h"

namespace loot {
namespace test {
class PluginSorterTest : public CommonGameTestFixture {
protected:
  PluginSorterTest() : game_(GetParam(), dataPath.parent_path(), localPath) {}

  void loadInstalledPlugins(Game& game_, bool headersOnly) {
    const std::vector<std::string> plugins({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
    });
    game_.IdentifyMainMasterFile(masterFile);
    game_.LoadPlugins(plugins, headersOnly);
  }

  Game game_;
};

// Pass an empty first argument, as it's a prefix for the test instantation,
// but we only have the one so no prefix is necessary.
INSTANTIATE_TEST_CASE_P(,
                        PluginSorterTest,
                        ::testing::Values(
                          GameType::tes4));

TEST_P(PluginSorterTest, sortingWithNoLoadedPluginsShouldReturnAnEmptyList) {
  PluginSorter sorter;
  std::vector<std::string> sorted = sorter.Sort(game_);

  EXPECT_TRUE(sorted.empty());
}

TEST_P(PluginSorterTest, sortingShouldNotMakeUnnecessaryChangesToAnExistingLoadOrder) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder = getLoadOrder();

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));

  // Check stability.
  sorted = ps.Sort(game_);
  EXPECT_TRUE(std::equal(begin(sorted), end(sorted), begin(expectedSortedOrder)));
}

TEST_P(PluginSorterTest, sortingShouldEvaluateRelativeGlobalPriorities) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankDifferentMasterDependentEsp);
  plugin.SetGlobalPriority(Priority(-100));
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentMasterDependentEsp,
      blankEsp,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankPluginDependentEsp,
      blankDifferentPluginDependentEsp,
  });

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest, sortingWithGlobalPrioritiesShouldInheritRecursivelyRegardlessOfEvaluationOrder) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));

  // Set Blank.esp's priority.
  PluginMetadata plugin(blankEsp);
  plugin.SetGlobalPriority(Priority(2));
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  // Load Blank - Master Dependent.esp after Blank.esp so that it
  // inherits Blank.esp's priority.
  plugin = PluginMetadata(blankMasterDependentEsp);
  plugin.SetLoadAfterFiles({
    File(blankEsp),
  });
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  // Load Blank - Different.esp after Blank - Master Dependent.esp, so
  // that it inherits its inherited priority.
  plugin = PluginMetadata(blankDifferentEsp);
  plugin.SetLoadAfterFiles({
    File(blankMasterDependentEsp),
  });
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  // Set Blank - Different Master Dependent.esp to have a higher priority
  // than 0 but lower than Blank.esp. Need to also make it a global priority
  // because it doesn't otherwise conflict with the other plugins.
  plugin = PluginMetadata(blankDifferentMasterDependentEsp);
  plugin.SetGlobalPriority(Priority(1));
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
    masterFile,
    blankEsm,
    blankDifferentEsm,
    blankMasterDependentEsm,
    blankDifferentMasterDependentEsm,
    blankDifferentMasterDependentEsp,
    blankEsp,
    blankMasterDependentEsp,
    blankDifferentEsp,
    blankPluginDependentEsp,
    blankDifferentPluginDependentEsp,
  });

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest, sortingShouldUseLoadAfterMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsp);
  plugin.SetLoadAfterFiles({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankDifferentPluginDependentEsp,
      blankEsp,
      blankPluginDependentEsp,
  });

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest, sortingShouldUseRequirementMetadataWhenDecidingRelativePluginPositions) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsp);
  plugin.SetRequirements({
      File(blankDifferentEsp),
      File(blankDifferentPluginDependentEsp),
  });
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  std::vector<std::string> expectedSortedOrder({
      masterFile,
      blankEsm,
      blankDifferentEsm,
      blankMasterDependentEsm,
      blankDifferentMasterDependentEsm,
      blankDifferentEsp,
      blankMasterDependentEsp,
      blankDifferentMasterDependentEsp,
      blankDifferentPluginDependentEsp,
      blankEsp,
      blankPluginDependentEsp,
  });

  std::vector<std::string> sorted = ps.Sort(game_);
  EXPECT_EQ(expectedSortedOrder, sorted);
}

TEST_P(PluginSorterTest, sortingShouldThrowIfACyclicInteractionIsEncountered) {
  ASSERT_NO_THROW(loadInstalledPlugins(game_, false));
  PluginMetadata plugin(blankEsm);
  plugin.SetLoadAfterFiles({File(blankMasterDependentEsm)});
  game_.GetDatabase()->SetPluginUserMetadata(plugin);

  PluginSorter ps;
  EXPECT_THROW(ps.Sort(game_), CyclicInteractionError);
}
}
}

#endif
