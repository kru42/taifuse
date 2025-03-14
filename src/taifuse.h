#ifndef TAIFUSE_H
#define TAIFUSE_H

#include <vitasdkkern.h>
#include <taihen.h>
#include "console.h"
#include "menu.h"

#define TAIFUSE_VERSION_STRING "taifuse v0.1"
#define PSVS_VERSION_VER       "TAIF00533"

extern tai_hook_ref_t g_ctrl_hook_refs[8];

/**
 * Print a message to the kernel console, newline included
 *
 * @param format - The format string
 * @param ... - Arguments for the format string
 */
void kuConsolePrintf(const char* format, ...);

#endif /* TAIFUSE_H */