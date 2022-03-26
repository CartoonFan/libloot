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

#include "api/helpers/crc.h"

#include <boost/crc.hpp>
#include <boost/format.hpp>
#include <fstream>

#include "api/helpers/logging.h"
#include "loot/exception/file_access_error.h"

using std::string;
using std::wstring;

namespace loot {
size_t GetStreamSize(std::istream& stream) {
  const std::streampos startingPosition = stream.tellg();

  stream.seekg(0, std::ios_base::end);
  const size_t streamSize = stream.tellg();
  stream.seekg(startingPosition, std::ios_base::beg);

  return streamSize;
}

// Calculate the CRC of the given file for comparison purposes.
uint32_t GetCrc32(const std::filesystem::path& filename) {
  try {
    auto logger = getLogger();
    if (logger) {
      logger->trace("Calculating CRC for: {}", filename.u8string());
    }

    std::ifstream ifile(filename, std::ios::binary);
    ifile.exceptions(std::ios_base::badbit | std::ios_base::failbit);

    static constexpr size_t BUFFER_SIZE = 8192;
    std::array<char, BUFFER_SIZE> buffer{};
    boost::crc_32_type result;
    size_t bytesLeft = GetStreamSize(ifile);
    while (bytesLeft > 0) {
      if (bytesLeft > buffer.size())
        ifile.read(buffer.data(), buffer.size());
      else
        ifile.read(buffer.data(), bytesLeft);

      result.process_bytes(buffer.data(), ifile.gcount());
      bytesLeft -= ifile.gcount();
    }

    uint32_t checksum = result.checksum();
    if (logger) {
      auto u8Filename = filename.u8string();
      logger->debug("CRC32(\"{}\"): {:x}", u8Filename, checksum);
    }
    return checksum;

  } catch (const std::exception& e) {
    throw FileAccessError("Unable to open \"" + filename.u8string() +
                          "\" for CRC calulation: " + e.what());
  }
}

std::string CrcToString(uint32_t crc) {
  return (boost::format("%08X") % crc).str();
}
}
