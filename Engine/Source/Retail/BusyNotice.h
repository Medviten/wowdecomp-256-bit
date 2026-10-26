#ifndef ENGINE_SOURCE_RETAIL_BUSYNOTICE_H
#define ENGINE_SOURCE_RETAIL_BUSYNOTICE_H
#include <cstdint>

namespace Retail {

// Shows active acquisition/preparation work after a short delay. Nested notices restore their
// parent's message; acquisition takes priority. The window hides as soon as all work finishes.
class BusyNotice {
public:
    explicit BusyNotice(const char* text, int priority = 0);
    ~BusyNotice();
    BusyNotice(const BusyNotice&) = delete;
    BusyNotice& operator=(const BusyNotice&) = delete;
private:
    uint64_t m_id = 0;
};

}
#endif
