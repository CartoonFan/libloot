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

#ifndef LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_GRAPH_TEST
#define LOOT_TESTS_API_INTERNALS_SORTING_PLUGIN_GRAPH_TEST

#include <gtest/gtest.h>

#include "api/sorting/plugin_graph.h"

namespace loot {
namespace test {
TEST(PluginGraph, topologicalSortWithNoLoadedPluginsShouldReturnAnEmptyList) {
  PluginGraph graph;
  std::vector<vertex_t> sorted = graph.TopologicalSort();

  EXPECT_TRUE(sorted.empty());
}
}
}

#endif
