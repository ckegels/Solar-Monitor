#pragma once
#include <array>
#include <cstdint>
#include <cstring>

// Home Usage settings of the site that is NOT active (two Home Assistant sites).
// The active site's settings live in the normal hu_* globals; at boot the firmware swaps the
// two when the display starts on the other site. Stored in flash as one block: the hu_alt_set
// global is a plain byte array (globals are declared before this header is included), and
// hu_set_load / hu_set_store convert.
// Slots 0-4 = device 1-5, slots 5-6 = subpanel 1-2.
struct HuSiteSet {
  char name[7][21];     // 20 characters, like the on-screen editor
  int8_t icon[7];
  int8_t route[5];      // devices only: 0 = main panel, 1/2 = subpanel
  uint8_t sp_enabled[2];
};

static_assert(sizeof(HuSiteSet) == 161, "hu_alt_set in solar-display.yaml is std::array<uint8_t, 161>");

inline HuSiteSet hu_set_load(const std::array<uint8_t, 161> &raw) {
  HuSiteSet s;
  std::memcpy(&s, raw.data(), sizeof(s));
  for (auto &n : s.name) n[sizeof(n) - 1] = '\0';
  return s;
}

inline void hu_set_store(std::array<uint8_t, 161> &raw, const HuSiteSet &s) { std::memcpy(raw.data(), &s, sizeof(s)); }
