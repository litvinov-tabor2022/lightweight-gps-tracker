#ifndef LIGHTWEIGHT_GPS_TRACKER_HWLOCKS_H
#define LIGHTWEIGHT_GPS_TRACKER_HWLOCKS_H

#include <mutex>

class HwLocks {
public:
    static std::recursive_mutex SERIAL_LOCK;
};

#endif //LIGHTWEIGHT_GPS_TRACKER_HWLOCKS_H
