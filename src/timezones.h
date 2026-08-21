#pragma once

// Maps an IANA timezone name to the POSIX TZ string that newlib's tzset()
// understands. The ESP32 has no tz database, so the DST rules have to be
// spelled out. The list mirrors the dropdown in the settings page.
//
// Returns nullptr for an unknown name.
const char *posixTzFor(const char *ianaName);

// Number of entries, and accessors, so the web UI can be generated from the
// same list instead of duplicating it in HTML.
int timezoneCount();
const char *timezoneNameAt(int index);
