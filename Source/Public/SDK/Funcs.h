#pragma once

#include <SDK/TypeInfo.h>
#include <SDK/SDK.h>

#include <Hook/Func.h>

namespace Kyber
{
TL_DECLARE_FUNC(0x1437F67E0, void, LevelSetup_ctor, LevelSetup* levelsetup);
//TL_DECLARE_FUNC(0x14740C290, void, LevelSetup_setEquals, LevelSetup* inst, LevelSetup* that);
//TL_DECLARE_FUNC(0x1436659C0, void, LevelSetup_setInclusionOption, LevelSetup*, char* category, char* mode);
TL_DECLARE_FUNC(0x141A60C10, void, ServerLoadLevelMessage_post, LevelSetup* levelSetup, bool one, bool two);
} // namespace Kyber
