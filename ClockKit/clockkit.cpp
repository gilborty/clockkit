#include "clockkit.h"

#include "ClockClient.h"
#include "ConfigReader.h"
#include "Exceptions.h"
#include "HighResolutionClock.h"
#include "limits"

dex::PhaseLockedClock* ckClock = nullptr;
std::string ckTimeString;  // Static storage for the pointer returned by ckTimeAsString().

void ckInitialize(const char* path)
{
    if (!ckClock)
        ckClock = dex::PhaseLockedClockFromConfigFile(std::string(path));
}

dex::timestamp_t ckTimeAsValue()
{
    if (!ckClock)
        return 0;  // "Zero usec since the epoch" is obviously invalid.
    try {
        return ckClock->getValue();
    }
    catch (dex::ClockException& e) {
        return 0;
    }
}

const char* ckTimeAsString()
{
    if (!ckClock)
        return "";
    try {
        ckTimeString = dex::Timestamp::timestampToString(ckClock->getValue());
        return ckTimeString.c_str();
    }
    catch (dex::ClockException& e) {
        return "";
    }
}

bool ckInSync()
{
    if (!ckClock)
        return false;
    return ckClock->isSynchronized();
}

int ckOffset()
{
    // Typically 2147483647 usec, or 35 minutes, obviously invalid.
    static const int invalid = std::numeric_limits<int>::max();
    if (!ckClock)
        return invalid;
    try {
        return ckClock->getOffset();
    }
    catch (dex::ClockException& e) {
        return invalid;
    }
}
