#include "nancy/base/memorys.h"

int main() {
    nc::base::memorys m({
        {1024, 128},
        {1024, 256}
    });
    for (int i = 0; i < 100000; ++i) {
        auto unit = m.get(128);
        m.recycle(std::move(unit));
    }
}