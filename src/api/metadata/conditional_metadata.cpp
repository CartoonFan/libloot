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

#include "loot/metadata/conditional_metadata.h"

#include "api/game/game.h"
#include "api/helpers/logging.h"
#include "api/metadata/condition_evaluator.h"

using std::string;

namespace loot {
ConditionalMetadata::ConditionalMetadata() {}

ConditionalMetadata::ConditionalMetadata(const string& condition) : condition_(condition) {}

bool ConditionalMetadata::IsConditional() const {
  return !condition_.empty();
}

std::string ConditionalMetadata::GetCondition() const {
  return condition_;
}

void ConditionalMetadata::ParseCondition() const {
  if (!condition_.empty()) {
    auto logger = getLogger();
    if (logger) {
      logger->trace("Testing condition syntax: {}", condition_);
    }
    ConditionEvaluator().evaluate(condition_);
  }
}
}
