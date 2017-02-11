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

#ifndef LOOT_API_MASTERLIST
#define LOOT_API_MASTERLIST

#include <string>

#include <boost/filesystem.hpp>

#include "api/metadata_list.h"
#include "loot/struct/masterlist_info.h"

namespace loot {
class Game;

class Masterlist : public MetadataList {
public:
  bool Update(const boost::filesystem::path& path,
              const std::string& repoURL,
              const std::string& repoBranch);

  static MasterlistInfo GetInfo(const boost::filesystem::path& path, bool shortID);

  static bool IsLatest(const boost::filesystem::path& path,
                       const std::string& repoBranch);
};
}

#endif
