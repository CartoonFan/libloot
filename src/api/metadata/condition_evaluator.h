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
    <http://www.gnu.org/licenses/>.
    */

#ifndef LOOT_API_METADATA_CONDITION_EVALUATOR
#define LOOT_API_METADATA_CONDITION_EVALUATOR

#include <regex>
#include <string>

#include <boost/filesystem.hpp>

#include "api/game/game_cache.h"
#include "api/game/load_order_handler.h"
#include "api/helpers/version.h"
#include "loot/metadata/plugin_cleaning_data.h"
#include "loot/metadata/plugin_metadata.h"

namespace loot {
class ConditionEvaluator {
public:
  ConditionEvaluator();
  ConditionEvaluator(const GameType gameType,
                     const boost::filesystem::path& dataPath,
                     std::shared_ptr<GameCache> gameCache,
                     std::shared_ptr<LoadOrderHandler> loadOrderHandler);

  bool evaluate(const std::string& condition) const;
  bool evaluate(const PluginCleaningData& cleaningData,
                const std::string& pluginName) const;
  PluginMetadata evaluateAll(const PluginMetadata& pluginMetadata) const;

  bool fileExists(const std::string& filePath) const;
  bool regexMatchExists(const std::string& regexString) const;
  bool regexMatchesExist(const std::string& regexString) const;

  bool isPluginActive(const std::string& pluginName) const;
  bool isPluginMatchingRegexActive(const std::string& regexString) const;
  bool arePluginsActive(const std::string& regexString) const;

  bool checksumMatches(const std::string& filePath,
                       const uint32_t checksum) const;

  bool compareVersions(const std::string& filePath,
                       const std::string& testVersion,
                       const std::string& comparator) const;

private:
  static void validatePath(const boost::filesystem::path& path);
  static void validateRegex(const std::string& regexString);

  static boost::filesystem::path getRegexParentPath(
      const std::string& regexString);
  static std::string getRegexFilename(const std::string& regexString);

  // Split a regex string into the non-regex filesystem parent path, and the
  // regex filename.
  static std::pair<boost::filesystem::path, std::regex> splitRegex(
      const std::string& regexString);

  bool isGameSubdirectory(const boost::filesystem::path& path) const;
  bool isRegexMatchInDataDirectory(
      const std::pair<boost::filesystem::path, std::regex>& pathRegex,
      const std::function<bool(const std::string&)> condition) const;
  bool areRegexMatchesInDataDirectory(
      const std::pair<boost::filesystem::path, std::regex>& pathRegex,
      const std::function<bool(const std::string&)> condition) const;

  bool parseCondition(const std::string& condition) const;

  Version getVersion(const std::string& filePath) const;

  bool shouldParseOnly() const;

  uint32_t getCrc(const std::string& file) const;

  const GameType gameType_;
  const boost::filesystem::path dataPath_;
  const std::shared_ptr<GameCache> gameCache_;
  const std::shared_ptr<LoadOrderHandler> loadOrderHandler_;
};
}

#endif
