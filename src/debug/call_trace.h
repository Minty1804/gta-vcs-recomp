// [Minty1804 fork] VCS_TRACE_CALLERS=0xADDR,...: counts a game function's callers ($ra, a0); the rig's `callers` command.
#pragma once

class PS2Runtime;

namespace vcs::calltrace
{
    void install(PS2Runtime &runtime);
}
