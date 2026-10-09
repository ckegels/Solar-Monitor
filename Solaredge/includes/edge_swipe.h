#pragma once
// Edge swipe: a touch that starts at the left or right edge of the screen and moves
// inwards switches to the previous / next page (the touchscreen's on_update in the YAML).
//
// Only the gesture logic lives here (no ESPHome/LVGL types), so it can be tested on a PC.

#include <cstdlib>

namespace edge_swipe {

// Distances in display pixels (1024x600 panel, ~6.6 px per mm)
static const int EDGE_PX = 60;        // a swipe must start this close to the left/right edge
static const int MIN_TRAVEL_PX = 110; // ...and move this far inwards
static const int MAX_DRIFT_PX = 40;   // vertical movement always allowed (finger wobble)
static const float MAX_SLOPE = 0.6f;  // beyond that: at most 0.6 px vertical per px inwards

class Detector {
 public:
  // Feed every touch update. x_org/y_org is where the touch started.
  // allowed: whether a swipe may start (screen on, page takes part); read only at touch start.
  // Returns -1 (previous page) or +1 (next page) once when the swipe is recognised, else 0.
  int update(int x_org, int y_org, int x, int y, int width, bool allowed) {
    if (!this->active_) {
      this->active_ = true;
      this->side_ = 0;
      if (allowed) {
        if (x_org < EDGE_PX)
          this->side_ = 1;  // left edge, moving right
        else if (x_org >= width - EDGE_PX)
          this->side_ = -1;  // right edge, moving left
      }
    }
    if (this->side_ == 0)
      return 0;

    int inward = (x - x_org) * this->side_;
    int drift = std::abs(y - y_org);
    if (drift > MAX_DRIFT_PX && drift > inward * MAX_SLOPE) {
      this->side_ = 0;  // mostly vertical: a scroll, not a swipe
      return 0;
    }
    if (inward < MIN_TRAVEL_PX)
      return 0;

    int dir = this->side_ == 1 ? -1 : 1;
    this->side_ = 0;  // once per touch
    return dir;
  }

  // Call when the finger is lifted.
  void release() { this->active_ = false; }

 protected:
  bool active_{false};
  int side_{0};  // 1 = started at the left edge, -1 = right edge, 0 = not a swipe (anymore)
};

inline Detector detector;

}  // namespace edge_swipe
