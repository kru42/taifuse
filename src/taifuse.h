#ifndef TAIFUSE_H
#define TAIFUSE_H

#include <taihen.h>

#define TAIFUSE_VERSION_STRING "taifuse v0.1"
#define PSVS_VERSION_VER       "TAIF00533"

extern tai_hook_ref_t g_ctrl_hook_refs[8];

/**
 * Print a message to the kernel taifuse osd console
 *
 * @param format - The format string
 * @param ... - Arguments for the format string
 * 
 * @return int - Success or failure code
 */
int tf_console_print(char* message);

#endif /* TAIFUSE_H */