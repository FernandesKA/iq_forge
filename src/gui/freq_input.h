#pragma once

namespace iqforge {

// Which unit a FrequencyInputHz widget is currently using for entered values.
// The underlying value is always stored in Hz.
enum class FreqUnit { Hz, kHz, MHz, GHz };

// Draws a numeric input plus a row of Hz/kHz/MHz/GHz buttons next to
// `label`. `hz` holds the real value in Hz and is updated in place. Selecting
// a unit assigns that unit to the number currently shown: entering 2000 and
// pressing kHz therefore stores 2 MHz, while the field continues to show
// 2000. `unit` persists the selected button. Returns true when `*hz` changes.
bool FrequencyInputHz(const char* label, double* hz, FreqUnit* unit);

} // namespace iqforge
