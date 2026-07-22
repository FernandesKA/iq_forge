#pragma once

namespace iqforge {

// Which unit a DurationInputSec widget is currently using for entered
// values. The underlying value is always stored in seconds.
enum class TimeUnit { Ns, Us, Ms, S };

// Draws a numeric input plus a row of ns/us/ms/s buttons next to `label`.
// `seconds` holds the real value in seconds and is updated in place.
// Selecting a unit assigns that unit to the number currently shown, mirroring
// FrequencyInputHz. `unit` persists the selected button. Returns true when
// `*seconds` changes.
bool DurationInputSec(const char* label, double* seconds, TimeUnit* unit);

} // namespace iqforge
