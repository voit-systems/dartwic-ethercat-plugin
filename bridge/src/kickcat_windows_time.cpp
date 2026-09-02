#include <chrono>
#include <thread>

#include "OS/Time.h"

namespace kickcat {

nanoseconds now() {
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch());
}

nanoseconds since_unix_epoch() {
    return duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
}

void sleep(nanoseconds duration) {
    if (duration.count() > 0) std::this_thread::sleep_for(duration);
}

} // namespace kickcat
