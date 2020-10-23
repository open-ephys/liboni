#pragma once

#include <onix.h>

namespace onix {

    inline const char *device_str(int dev_id)
    {
        return onix_device_str(dev_id);
    }
}
