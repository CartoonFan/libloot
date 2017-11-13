/*  LOOT

    A load order optimisation tool for Oblivion, Skyrim, Fallout 3 and
    Fallout: New Vegas.

    Copyright (C) 2012-2016    WrinklyNinja

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

#include "plugin_sorter.h"

#include <cstdlib>

#include <boost/algorithm/string.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/iteration_macros.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/locale.hpp>

#include "api/game/game.h"
#include "api/helpers/logging.h"
#include "api/metadata/condition_evaluator.h"
#include "loot/exception/cyclic_interaction_error.h"

using std::list;
using std::string;
using std::vector;

namespace loot {
PluginSortingData::PluginSortingData(const Plugin& plugin,
                                     const PluginMetadata&& metadata) :
    plugin_(plugin),
    PluginMetadata(metadata) {}

std::string PluginSortingData::GetName() const { return plugin_.GetName(); }

bool PluginSortingData::IsMaster() const {
  return plugin_.IsMaster() || plugin_.IsLightMaster();
}

bool PluginSortingData::LoadsArchive() const { return plugin_.LoadsArchive(); }

std::vector<std::string> PluginSortingData::GetMasters() const {
  return plugin_.GetMasters();
}

size_t PluginSortingData::NumOverrideFormIDs() const {
  return plugin_.NumOverrideFormIDs();
}

bool PluginSortingData::DoFormIDsOverlap(
    const PluginSortingData& plugin) const {
  return plugin_.DoFormIDsOverlap(plugin.plugin_);
}

typedef boost::graph_traits<PluginGraph>::vertex_iterator vertex_it;
typedef boost::graph_traits<PluginGraph>::edge_descriptor edge_t;
typedef boost::graph_traits<PluginGraph>::edge_iterator edge_it;

class PathFoundException : public std::exception {};

class CycleDetector : public boost::dfs_visitor<> {
public:
  void tree_edge(edge_t edge, const PluginGraph& graph) {
    const vertex_t source = boost::source(edge, graph);
    const string name = graph[source].GetName();

    // Check if the plugin already exists in the recorded trail.
    auto it = find(begin(trail), end(trail), name);

    if (it != end(trail)) {
      // Erase everything from this position onwards, as it doesn't
      // contribute to a forward-cycle.
      trail.erase(it, end(trail));
    }

    trail.push_back(name);
  }

  void back_edge(edge_t edge, const PluginGraph& graph) {
    vertex_t source = boost::source(edge, graph);
    vertex_t target = boost::target(edge, graph);

    trail.push_back(graph[source].GetName());
    string backCycle;
    auto it = find(begin(trail), end(trail), graph[target].GetName());
    for (it; it != end(trail); ++it) {
      backCycle += *it + ", ";
    }
    backCycle.erase(backCycle.length() - 2);

    throw CyclicInteractionError(
        graph[source].GetName(), graph[target].GetName(), backCycle);
  }

private:
  list<string> trail;
};

class PathDetector : public boost::bfs_visitor<> {
public:
  PathDetector(vertex_t vertex) : target(vertex) {}

  inline void discover_vertex(vertex_t vertex, const PluginGraph& graph) {
    if (vertex == target)
      throw PathFoundException();
  }

private:
  vertex_t target;
};

std::vector<std::string> PluginSorter::Sort(Game& game) {
  logger_ = getLogger();

  // Clear existing data.
  graph_.clear();
  indexMap_.clear();
  oldLoadOrder_.clear();

  AddPluginVertices(game);

  // If there aren't any vertices, exit early, because sorting assumes
  // there is at least one plugin.
  if (boost::num_vertices(graph_) == 0)
    return vector<std::string>();

  // Get the existing load order.
  oldLoadOrder_ = game.GetLoadOrder();
  if (logger_) {
    logger_->info("Fetched existing load order: ");
    for (const auto& plugin : oldLoadOrder_) {
      logger_->info("\t\t{}", plugin);
    }
  }

  // Now add the interactions between plugins to the graph as edges.
  if (logger_) {
    logger_->info("Adding edges to plugin graph.");
    logger_->debug("Adding non-overlap edges.");
  }
  AddSpecificEdges();

  PropagatePriorities();

  if (logger_) {
    logger_->debug("Adding priority edges.");
  }
  AddPriorityEdges();

  if (logger_) {
    logger_->debug("Adding overlap edges.");
  }
  AddOverlapEdges();

  if (logger_) {
    logger_->debug("Adding tie-break edges.");
  }
  AddTieBreakEdges();

  if (logger_) {
    logger_->debug("Checking to see if the graph is cyclic.");
  }
  CheckForCycles();

  // Now we can sort.
  if (logger_) {
    logger_->debug("Performing a topological sort.");
  }
  list<vertex_t> sortedVertices;
  boost::topological_sort(graph_,
                          std::front_inserter(sortedVertices),
                          boost::vertex_index_map(vertexIndexMap_));

  // Check that the sorted path is Hamiltonian (ie. unique).
  for (auto it = sortedVertices.begin(); it != sortedVertices.end(); ++it) {
    if (next(it) != sortedVertices.end() &&
        !boost::edge(*it, *next(it), graph_).second && logger_) {
      logger_->error(
          "The calculated load order is not unique. No edge exists between {} "
          "and {}.",
          graph_[*it].GetName(),
          graph_[*next(it)].GetName());
    }
  }

  // Output a plugin list using the sorted vertices.
  if (logger_) {
    logger_->info("Calculated order: ");
  }
  vector<std::string> plugins;
  for (const auto& vertex : sortedVertices) {
    plugins.push_back(graph_[vertex].GetName());
    if (logger_) {
      logger_->info("\t{}", plugins.back());
    }
  }

  return plugins;
}

void PluginSorter::AddPluginVertices(Game& game) {
  if (logger_) {
    logger_->info(
        "Merging masterlist, userlist into plugin list, evaluating conditions "
        "and checking for install validity.");
  }

  // The resolution of tie-breaks in the plugin graph may be dependent
  // on the order in which vertices are iterated over, as an earlier tie
  // break resolution may cause a potential later tie break to instead
  // cause a cycle. Vertices are stored in a std::list and added to the
  // list using push_back().
  // Plugins are stored in an unordered map, so simply iterating over
  // its elements is not guarunteed to produce a consistent vertex order.
  // MSVC 2013 and GCC 5.0 have been shown to produce consistent
  // iteration orders that differ, and while MSVC 2013's order seems to
  // be independent on the order in which the unordered map was filled
  // (being lexicographical), GCC 5.0's unordered map iteration order is
  // dependent on its insertion order.
  // Given that, the order of vertex creation should be made consistent
  // in order to produce consistent sorting results. While MSVC 2013
  // doesn't strictly need this, there is no guaruntee that this
  // unspecified behaviour will remain in future compiler updates, so
  // implement it generally.

  // Using a set of plugin names followed by finding the matching key
  // in the unordered map, as it's probably faster than copying the
  // full plugin objects then sorting them.
  for (const auto& plugin : game.GetCache()->GetPlugins()) {
    if (logger_) {
      logger_->trace("Getting and evaluating metadata for plugin {}",
                     plugin->GetName());
    }

    auto metadata =
        game.GetDatabase()->GetPluginMetadata(plugin->GetName(), true, true);

    if (logger_) {
      logger_->trace("Getting and evaluating metadata for plugin \"{}\"",
                     plugin->GetName());
    }

    vertex_t v = boost::add_vertex(
        PluginSortingData(*plugin, std::move(metadata)), graph_);
  }

  // Prebuild an index map, which std::list-based VertexList graphs don't have.
  vertexIndexMap_ = vertex_map_t(indexMap_);
  size_t i = 0;
  BGL_FORALL_VERTICES(v, graph_, PluginGraph)
  put(vertexIndexMap_, v, i++);
}

bool PluginSorter::GetVertexByName(const std::string& name,
                                   vertex_t& vertexOut) const {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (boost::iequals(graph_[vertex].GetName(), name)) {
      vertexOut = vertex;
      return true;
    }
  }

  return false;
}

void PluginSorter::CheckForCycles() const {
  boost::depth_first_search(
      graph_, visitor(CycleDetector()).vertex_index_map(vertexIndexMap_));
}

bool PluginSorter::EdgeCreatesCycle(const vertex_t& fromVertex,
                                    const vertex_t& toVertex) const {
  try {
    boost::breadth_first_search(
        graph_,
        toVertex,
        visitor(PathDetector(fromVertex)).vertex_index_map(vertexIndexMap_));
  } catch (PathFoundException&) {
    return true;
  }
  return false;
}

void PluginSorter::PropagatePriorities() {
  /* If a plugin has a priority value > 0, that value should be
     inherited by all plugins that have edges coming from that
     plugin, ie. those that load after it, unless the plugin being
     compared itself has a larger value. */

  // Find all vertices with priorities > 0.
  std::vector<vertex_t> positivePriorityVertices;
  vertex_it vit, vitend;
  tie(vit, vitend) = boost::vertices(graph_);
  std::copy_if(vit,
               vitend,
               std::back_inserter(positivePriorityVertices),
               [&](const vertex_t& vertex) {
                 return graph_[vertex].GetLocalPriority() > 0 ||
                        graph_[vertex].GetGlobalPriority() > 0;
               });

  // To reduce the number of priorities that will need setting,
  // sort the vertices in order of decreasing priority.
  std::sort(begin(positivePriorityVertices),
            end(positivePriorityVertices),
            [&](const vertex_t& lhs, const vertex_t& rhs) {
              return graph_[lhs].GetLocalPriority() >
                         graph_[rhs].GetLocalPriority() ||
                     graph_[lhs].GetGlobalPriority() >
                         graph_[rhs].GetGlobalPriority();
            });

  // Create a color map.
  std::vector<boost::default_color_type> colorVec(num_vertices(graph_));
  boost::iterator_property_map<boost::default_color_type*, vertex_map_t>
      colorMap(&colorVec.front(), vertexIndexMap_);

  // Now loop over the vertices. For each one, do a depth-first
  // search, setting priorities until an equal or larger value is
  // encountered.
  for (const vertex_t& vertex : positivePriorityVertices) {
    if (logger_) {
      logger_->trace(
          "Doing DFS for {} which has local priority {} and global priority {}",
          graph_[vertex].GetName(),
          graph_[vertex].GetLocalPriority().GetValue(),
          graph_[vertex].GetGlobalPriority().GetValue());
    }
    boost::dfs_visitor<> visitor;
    boost::depth_first_visit(
        graph_,
        vertex,
        visitor,
        colorMap,
        [&](const vertex_t& currentVertex, const PluginGraph& graph) {
          // depth_first_search takes a const graph, so cast it if modifying a
          // vertex.
          if (graph[currentVertex].GetLocalPriority() <
              graph[vertex].GetLocalPriority()) {
            if (logger_) {
              logger_->trace("Overriding local priority for {} from {} to {}",
                             graph[currentVertex].GetName(),
                             graph[currentVertex].GetLocalPriority().GetValue(),
                             graph[vertex].GetLocalPriority().GetValue());
            }
            const_cast<PluginGraph&>(graph)[currentVertex].SetLocalPriority(
                graph[vertex].GetLocalPriority());

            return false;
          }

          if (graph[currentVertex].GetGlobalPriority() <
              graph[vertex].GetGlobalPriority()) {
            if (logger_) {
              logger_->trace(
                  "Overriding global priority for {} from {} to {}",
                  graph[currentVertex].GetName(),
                  graph[currentVertex].GetGlobalPriority().GetValue(),
                  graph[vertex].GetGlobalPriority().GetValue());
            }
            const_cast<PluginGraph&>(graph)[currentVertex].SetGlobalPriority(
                graph[vertex].GetGlobalPriority());

            return false;
          }

          return currentVertex != vertex &&
                 graph[currentVertex].GetLocalPriority() >=
                     graph[vertex].GetLocalPriority() &&
                 graph[currentVertex].GetGlobalPriority() >=
                     graph[vertex].GetGlobalPriority();
        });
  }
}

void PluginSorter::AddEdge(const vertex_t& fromVertex,
                           const vertex_t& toVertex) {
  if (!boost::edge(fromVertex, toVertex, graph_).second) {
    if (logger_) {
      logger_->trace("Adding edge from \"{}\" to \"{}\".",
                     graph_[fromVertex].GetName(),
                     graph_[toVertex].GetName());
    }

    boost::add_edge(fromVertex, toVertex, graph_);
  }
}

void PluginSorter::AddSpecificEdges() {
  // Add edges for all relationships that aren't overlaps or priority
  // differences.
  vertex_it vit, vitend;
  for (tie(vit, vitend) = boost::vertices(graph_); vit != vitend; ++vit) {
    if (logger_) {
      logger_->trace("Adding specific edges to vertex for \"{}\".",
                     graph_[*vit].GetName());
      logger_->trace("Adding edges for master flag differences.");
    }

    for (vertex_it vit2 = vit; vit2 != vitend; ++vit2) {
      if (graph_[*vit].IsMaster() == graph_[*vit2].IsMaster())
        continue;

      vertex_t vertex, parentVertex;
      if (graph_[*vit2].IsMaster()) {
        parentVertex = *vit2;
        vertex = *vit;
      } else {
        parentVertex = *vit;
        vertex = *vit2;
      }

      AddEdge(parentVertex, vertex);
    }

    vertex_t parentVertex;
    if (logger_) {
      logger_->trace("Adding in-edges for masters.");
    }
    for (const auto& master : graph_[*vit].GetMasters()) {
      if (GetVertexByName(master, parentVertex))
        AddEdge(parentVertex, *vit);
    }

    if (logger_) {
      logger_->trace("Adding in-edges for requirements.");
    }
    for (const auto& file : graph_[*vit].GetRequirements()) {
      if (GetVertexByName(file.GetName(), parentVertex))
        AddEdge(parentVertex, *vit);
    }

    if (logger_) {
      logger_->trace("Adding in-edges for 'load after's.");
    }
    for (const auto& file : graph_[*vit].GetLoadAfterFiles()) {
      if (GetVertexByName(file.GetName(), parentVertex))
        AddEdge(parentVertex, *vit);
    }
  }
}

void PluginSorter::AddPriorityEdges() {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (logger_) {
      logger_->trace("Adding priority difference edges to vertex for \"{}\".",
                     graph_[vertex].GetName());
    }
    // If the plugin has a global priority of zero and doesn't load
    // an archive and has no override records, skip it. Plugins without
    // override records can only conflict with plugins that override
    // the records they add, so any edge necessary will be added when
    // evaluating that plugin.
    if (graph_[vertex].GetGlobalPriority().GetValue() == 0 &&
        graph_[vertex].NumOverrideFormIDs() == 0 &&
        !graph_[vertex].LoadsArchive()) {
      continue;
    }

    for (const auto& otherVertex :
         boost::make_iterator_range(boost::vertices(graph_))) {
      // If the plugins have equal priority, or have non-global
      // priorities but don't conflict, don't add a priority edge.
      if ((graph_[vertex].GetLocalPriority() ==
               graph_[otherVertex].GetLocalPriority() &&
           graph_[vertex].GetGlobalPriority() ==
               graph_[otherVertex].GetGlobalPriority()) ||
          (graph_[vertex].GetGlobalPriority().GetValue() == 0 &&
           graph_[otherVertex].GetGlobalPriority().GetValue() == 0 &&
           !graph_[vertex].DoFormIDsOverlap(graph_[otherVertex]))) {
        continue;
      }

      vertex_t toVertex, fromVertex;
      if (graph_[vertex].GetGlobalPriority() <
              graph_[otherVertex].GetGlobalPriority() ||
          (graph_[vertex].GetGlobalPriority() ==
               graph_[otherVertex].GetGlobalPriority() &&
           graph_[vertex].GetLocalPriority() <
               graph_[otherVertex].GetLocalPriority())) {
        fromVertex = vertex;
        toVertex = otherVertex;
      } else {
        fromVertex = otherVertex;
        toVertex = vertex;
      }

      if (!EdgeCreatesCycle(fromVertex, toVertex))
        AddEdge(fromVertex, toVertex);
    }
  }
}

void PluginSorter::AddOverlapEdges() {
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (logger_) {
      logger_->trace("Adding overlap edges to vertex for \"{}\".",
                     graph_[vertex].GetName());
    }

    if (graph_[vertex].NumOverrideFormIDs() == 0) {
      if (logger_) {
        logger_->trace(
            "Skipping vertex for \"{}\": the plugin contains no override "
            "records.",
            graph_[vertex].GetName());
      }
      continue;
    }

    for (const auto& otherVertex :
         boost::make_iterator_range(boost::vertices(graph_))) {
      if (vertex == otherVertex ||
          boost::edge(vertex, otherVertex, graph_).second ||
          boost::edge(otherVertex, vertex, graph_).second ||
          graph_[vertex].NumOverrideFormIDs() ==
              graph_[otherVertex].NumOverrideFormIDs() ||
          !graph_[vertex].DoFormIDsOverlap(graph_[otherVertex])) {
        continue;
      }

      vertex_t toVertex, fromVertex;
      if (graph_[vertex].NumOverrideFormIDs() >
          graph_[otherVertex].NumOverrideFormIDs()) {
        fromVertex = vertex;
        toVertex = otherVertex;
      } else {
        fromVertex = otherVertex;
        toVertex = vertex;
      }

      if (!EdgeCreatesCycle(fromVertex, toVertex))
        AddEdge(fromVertex, toVertex);
    }
  }
}

int PluginSorter::ComparePlugins(const std::string& plugin1,
                                 const std::string& plugin2) const {
  auto it1 = find(begin(oldLoadOrder_), end(oldLoadOrder_), plugin1);
  auto it2 = find(begin(oldLoadOrder_), end(oldLoadOrder_), plugin2);

  if (it1 != end(oldLoadOrder_) && it2 == end(oldLoadOrder_))
    return -1;
  else if (it1 == end(oldLoadOrder_) && it2 != end(oldLoadOrder_))
    return 1;
  else if (it1 != end(oldLoadOrder_) && it2 != end(oldLoadOrder_)) {
    if (distance(begin(oldLoadOrder_), it1) <
        distance(begin(oldLoadOrder_), it2))
      return -1;
    else
      return 1;
  } else {
    // Neither plugin has a load order position. Need to use another
    // comparison to get an ordering.

    // Compare plugin basenames.
    string name1 = boost::locale::to_lower(plugin1);
    name1 = name1.substr(0, name1.length() - 4);
    string name2 = boost::locale::to_lower(plugin2);
    name2 = name2.substr(0, name2.length() - 4);

    if (name1 < name2)
      return -1;
    else if (name2 < name1)
      return 1;
    else {
      // Could be a .esp and .esm plugin with the same basename,
      // compare whole filenames.
      if (plugin1 < plugin2)
        return -1;
      else
        return 1;
    }
  }
  return 0;
}

void PluginSorter::AddTieBreakEdges() {
  // In order for the sort to be performed stably, there must be only one
  // possible result. This can be enforced by adding edges between all vertices
  // that aren't already linked. Use existing load order to decide the direction
  // of these edges.
  for (const auto& vertex :
       boost::make_iterator_range(boost::vertices(graph_))) {
    if (logger_) {
      logger_->trace("Adding tie-break edges to vertex for \"{}\"",
                     graph_[vertex].GetName());
    }

    for (const auto& otherVertex :
         boost::make_iterator_range(boost::vertices(graph_))) {
      if (vertex == otherVertex ||
          boost::edge(vertex, otherVertex, graph_).second ||
          boost::edge(otherVertex, vertex, graph_).second)
        continue;

      vertex_t toVertex, fromVertex;
      if (ComparePlugins(graph_[vertex].GetName(),
                         graph_[otherVertex].GetName()) < 0) {
        fromVertex = vertex;
        toVertex = otherVertex;
      } else {
        fromVertex = otherVertex;
        toVertex = vertex;
      }

      if (!EdgeCreatesCycle(fromVertex, toVertex))
        AddEdge(fromVertex, toVertex);
    }
  }
}
}
