#pragma once
// ---------------------------------------------------------------------------
// Deferred console command registry.
//
// Console commands used to be registered in one 3472 line file, thousands of lines
// away from the code they configure - which is how a cvar and its own default value
// drift apart (see spawn_antifreeze_max_per_frame). A translation unit can now keep
// its command next to its implementation:
//
//     static CCC_Integer cmd_spawn_antifreeze_max_per_frame(
//         "spawn_antifreeze_max_per_frame", &psSpawnAntifreezeMaxPerFrame, 0, 1000000);
//     XR_CONSOLE_REGISTER(cmd_spawn_antifreeze_max_per_frame);
//
// Registration is deferred on purpose: static initialisers run before the console
// exists, so XR_CONSOLE_REGISTER only queues the command; xr_console_apply_deferred()
// (called from the registrars, which run once the console is alive) adds the queue.
// ---------------------------------------------------------------------------
#include <vector>

class IConsole_Command;

inline std::vector<IConsole_Command*>& xr_console_deferred()
{
	static std::vector<IConsole_Command*> list;
	return list;
}

inline void xr_console_register_deferred(IConsole_Command* command)
{
	if (command != nullptr)
		xr_console_deferred().push_back(command);
}

// Defined in xr_ioc_cmd.cpp, which owns the console. Safe to call more than once.
void xr_console_apply_deferred();

// Convenience for the common case, so a translation unit does not have to include the
// command classes (and their macro/`Console` ordering requirements) at all.
void xr_console_register_int_cvar(const char* name, int* value, int min, int max);

#define XR_CONSOLE_REGISTER(var)                        \
	namespace                                           \
	{                                                   \
		struct var##_registrar                          \
		{                                               \
			var##_registrar() { xr_console_register_deferred(&var); } \
		} var##_instance;                               \
	}
