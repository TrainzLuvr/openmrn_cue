/** \copyright
 * Copyright (c) 2014-2016, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file FindProtocolDefs.cxx
 *
 * Definitions for the train node find protocol.
 *
 * @author Balazs Racz
 * @date 19 Feb 2016
 */

#include "commandstation/FindProtocolDefs.hxx"
#include "commandstation/TrainDb.hxx"

namespace commandstation {

namespace {
/// @returns true for a character that is a digit.
bool is_number(char c) { return ('0' <= c) && (c <= '9'); }

/// @returns the same bitmask as match_query_to_node.
uint8_t attempt_match(const string name, unsigned pos, nmranet::EventId event) {
  int count_matches = 0;
  for (int shift = FindProtocolDefs::TRAIN_FIND_MASK - 4;
       shift >= FindProtocolDefs::TRAIN_FIND_MASK_LOW; shift -= 4) {
    uint8_t nibble = (event >> shift) & 0xf;
    if ((0 <= nibble) && (nibble <= 9)) {
      while (pos < name.size() && !is_number(name[pos])) ++pos;
      if (pos > name.size()) return 0;
      if ((name[pos] - '0') != nibble) return 0;
      ++pos;
      ++count_matches;
      continue;
    } else {
      continue;
    }
  }
  // If we are here, we have exhausted (and matched) all digits that were sent
  // by the query.
  if (count_matches == 0) {
    // Query had no digits. That's weird and should be incompliant.
    return 0;
  }
  // Look for more digits in the name.
  while (pos < name.size() && !is_number(name[pos])) ++pos;
  if (pos < name.size()) {
    if (event & FindProtocolDefs::EXACT) {
      return 0;
    }
    return FindProtocolDefs::MATCH_ANY;
  } else {
    return FindProtocolDefs::EXACT | FindProtocolDefs::MATCH_ANY;
  }
}

}  // namespace


// static
unsigned FindProtocolDefs::query_to_address(nmranet::EventId event, DccMode* mode) {
  unsigned supplied_address = 0;
  bool has_prefix_zero = false;
  for (int shift = TRAIN_FIND_MASK - 4; shift >= TRAIN_FIND_MASK_LOW;
       shift -= 4) {
    uint8_t nibble = (event >> shift) & 0xf;
    if (0 == nibble && 0 == supplied_address) {
      has_prefix_zero = true;
    }
    if ((0 <= nibble) && (nibble <= 9)) {
      supplied_address *= 10;
      supplied_address += nibble;
      continue;
    }
    // For the moment we just ignore every non-numeric character. Including
    // gluing together all digits entered by the user into one big number.
  }
  uint8_t nmode = event & 7;
  // The default drive mode is known by AllTrainNodes. It is okay to leave it
  // as zero.
  if (has_prefix_zero || (event & FindProtocolDefs::DCC_FORCE_LONG)) {
    nmode |= commandstation::DCC_LONG_ADDRESS;
  }
  *mode = static_cast<DccMode>(nmode);
  return supplied_address;
}

// static
uint8_t FindProtocolDefs::match_query_to_node(nmranet::EventId event,
                                              TrainDbEntry* train) {
  unsigned legacy_address = train->get_legacy_address();
  DccMode mode;
  unsigned supplied_address = query_to_address(event, &mode);
  if (supplied_address == legacy_address) {
    // TODO: we have to check here if long address only is set here.
    return MATCH_ANY | ADDRESS_ONLY | EXACT;
  }
  bool has_address_prefix_match = false;
  if ((event & EXACT) == 0) {
    // Search for the supplied number being a prefix of the existing addresses.
    unsigned address_prefix = legacy_address / 10;
    while (address_prefix) {
      if (address_prefix == supplied_address) {
        has_address_prefix_match = true;
        break;
      }
      address_prefix /= 10;
    }
  }
  if (event & ADDRESS_ONLY) {
    if (!has_address_prefix_match) return 0;
    return MATCH_ANY | ADDRESS_ONLY;
  }
  // Match against the train name string.
  uint8_t first_name_match = 0xFF;
  uint8_t best_name_match = 0;
  string name = train->get_train_name();
  // Find the beginning of numeric components in the train name
  unsigned pos = 0;
  while (pos < name.size()) {
    if (is_number(name[pos])) {
      uint8_t current_match = attempt_match(name, pos, event);
      if (first_name_match == 0xff) {
        first_name_match = current_match;
        best_name_match = current_match;
      }
      if ((!best_name_match && current_match) ||
          (current_match & EXACT)) {
        // We overwrite the best name match if there was no previous match, or
        // if the current match is exact. This is somewhat questionable,
        // because it will allow an exact match on the middle of the train name
        // even though there may have been numbers before. However, this is
        // arguably okay in case the train name is a model number and a cab
        // number, and we're matching against the cab number, e.g. 
        // "Re 4/4 11239" which should be exact-matching the query 11239.
        best_name_match = current_match;
      }
      // Skip through the sequence of numbers
      while (pos < name.size() && is_number(name[pos])) {
        ++pos;
      }
    } else {
      // non number: skip
      ++pos;
    }
  }
  if (first_name_match == 0xff) {
    // No numbers in the train name.
    best_name_match = 0;
  }
  if (((best_name_match & EXACT) == 0) && has_address_prefix_match) {
    // We prefer a partial address match over a non-exact name match.  If
    // address_prefix_match == true then the query had the EXACT bit false.
    return MATCH_ANY | ADDRESS_ONLY;
  }
  return best_name_match;
}

}  // namespace commandstation