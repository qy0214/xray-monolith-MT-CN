#pragma once

// Build identity for the MT PiP work.
//
// The startup stamp used to print only __DATE__/__TIME__ of r4.cpp, which an incremental
// build does not refresh: two different payloads logged the same build time, and the
// "which exe is actually running" question could not be answered from the log. This number
// is bumped deliberately whenever the PiP chain changes, and the stamp prints it next to
// the loaded module's own timestamp and size, both of which I can reproduce from the
// deployed file. Bump it in the same commit as any change to the PiP path.
#define MT_PIP_BUILD 10
