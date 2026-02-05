#pragma once

// Ensure ESP-IDF Wi-Fi symbols are available inside ESPHome lambdas.
// (ESPHome often includes these already, but explicitly including removes
// ambiguity across framework/version changes.)

#include <string.h>

extern "C" {
#include "esp_wifi.h"
}
