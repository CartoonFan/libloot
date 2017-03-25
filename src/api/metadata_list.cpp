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

#include "api/metadata_list.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/log/trivial.hpp>

#include "loot/exception/file_access_error.h"
#include "loot/yaml/plugin_metadata.h"
#include "api/game/game.h"
#include "api/metadata/condition_evaluator.h"

namespace loot {
void MetadataList::Load(const boost::filesystem::path& filepath) {
  Clear();

  BOOST_LOG_TRIVIAL(debug) << "Loading file: " << filepath;

  boost::filesystem::ifstream in(filepath);
  if (!in.good())
    throw FileAccessError("Cannot open " + filepath.string());

  YAML::Node metadataList = YAML::Load(in);
  in.close();

  if (!metadataList.IsMap())
    throw FileAccessError("The root of the metadata file " + filepath.string() + " is not a YAML map.");

  if (metadataList["plugins"]) {
    for (const auto& node : metadataList["plugins"]) {
      PluginMetadata plugin(node.as<PluginMetadata>());
      if (plugin.IsRegexPlugin())
        regexPlugins_.push_back(plugin);
      else if (!plugins_.insert(plugin).second)
        throw FileAccessError("More than one entry exists for \"" + plugin.GetName() + "\"");
    }
  }
  if (metadataList["globals"])
    messages_ = metadataList["globals"].as<std::vector<Message>>();

  if (metadataList["bash_tags"])
    bashTags_ = metadataList["bash_tags"].as<std::set<std::string>>();

  BOOST_LOG_TRIVIAL(debug) << "File loaded successfully.";
}

void MetadataList::Save(const boost::filesystem::path& filepath) const {
  BOOST_LOG_TRIVIAL(trace) << "Saving metadata list to: " << filepath;
  YAML::Emitter yout;
  yout.SetIndent(2);
  yout << YAML::BeginMap
    << YAML::Key << "bash_tags" << YAML::Value << bashTags_
    << YAML::Key << "globals" << YAML::Value << messages_
    << YAML::Key << "plugins" << YAML::Value << Plugins()
    << YAML::EndMap;

  boost::filesystem::ofstream uout(filepath);
  if (uout.fail())
    throw FileAccessError("Couldn't open output file.");

  uout << yout.c_str();
  uout.close();
}

void MetadataList::Clear() {
  bashTags_.clear();
  plugins_.clear();
  regexPlugins_.clear();
  messages_.clear();
}

std::list<PluginMetadata> MetadataList::Plugins() const {
  std::list<PluginMetadata> pluginList(plugins_.begin(), plugins_.end());

  pluginList.insert(pluginList.end(), regexPlugins_.begin(), regexPlugins_.end());

  return pluginList;
}

std::vector<Message> MetadataList::Messages() const {
  return messages_;
}

std::set<std::string> MetadataList::BashTags() const {
  return bashTags_;
}

// Merges multiple matching regex entries if any are found.
PluginMetadata MetadataList::FindPlugin(const PluginMetadata& plugin) const {
  PluginMetadata match(plugin.GetName());

  auto it = plugins_.find(plugin);

  if (it != plugins_.end())
    match = *it;

// Now we want to also match possibly multiple regex entries.
  auto regIt = find(regexPlugins_.begin(), regexPlugins_.end(), plugin);
  while (regIt != regexPlugins_.end()) {
    match.MergeMetadata(*regIt);

    regIt = find(++regIt, regexPlugins_.end(), plugin);
  }

  return match;
}

void MetadataList::AddPlugin(const PluginMetadata& plugin) {
  if (plugin.IsRegexPlugin())
    regexPlugins_.push_back(plugin);
  else {
    if (!plugins_.insert(plugin).second)
      throw std::invalid_argument("Cannot add \"" + plugin.GetName() + "\" to the metadata list as another entry already exists.");
  }
}

// Doesn't erase matching regex entries, because they might also
// be required for other plugins.
void MetadataList::ErasePlugin(const PluginMetadata& plugin) {
  auto it = plugins_.find(plugin);

  if (it != plugins_.end()) {
    plugins_.erase(it);
    return;
  }
}

void MetadataList::AppendMessage(const Message& message) {
  messages_.push_back(message);
}

void MetadataList::EvalAllConditions(const ConditionEvaluator& conditionEvaluator) {
  if (unevaluatedPlugins_.empty())
    unevaluatedPlugins_.swap(plugins_);
  else
    plugins_.clear();

  for (const auto& plugin : unevaluatedPlugins_) {
    plugins_.insert(conditionEvaluator.evaluateAll(plugin));
  }

  if (unevaluatedRegexPlugins_.empty())
    unevaluatedRegexPlugins_ = regexPlugins_;
  else
    regexPlugins_ = unevaluatedRegexPlugins_;

  for (auto& plugin : regexPlugins_) {
    plugin = conditionEvaluator.evaluateAll(plugin);
  }

  if (unevaluatedMessages_.empty())
    unevaluatedMessages_.swap(messages_);
  else
    messages_.clear();

  for (const auto& message : unevaluatedMessages_) {
    if (conditionEvaluator.evaluate(message.GetCondition()))
      messages_.push_back(message);
  }
}
}
