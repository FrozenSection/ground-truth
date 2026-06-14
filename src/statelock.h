#pragma once
#include <mutex>

// Guards device state shared between the Arduino main loop (seismic fetch/publish,
// settings apply) and the ESPAsyncWebServer task (/api/state read). Held only for
// quick publish/read sections — never across network I/O — so contention is trivial.
extern std::mutex g_stateMutex;
