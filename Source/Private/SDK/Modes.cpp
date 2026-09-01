// Copyright BattleDash. All Rights Reserved.

#include <SDK/Modes.h>

#include <string>

namespace Kyber
{
    std::vector<const char*> GetStartPoints(const char* level)
    {
        for (const StartPointMap& mode : s_level_to_point)
        {
            if (std::strcmp(mode.level, level) == 0)
            {
                return mode.startpoints;
            }
        }
        return {};
    }
} // namespace Kyber
