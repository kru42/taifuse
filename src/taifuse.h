#ifndef TAIFUSE_H
#define TAIFUSE_H

#include <psp2kern/types.h>
#include "console.h"

/**
 * Print a message to the kernel console, newline included
 *
 * @param format - The format string
 * @param ... - Arguments for the format string
 */
void kuConsolePrintf(const char* format, ...);

#endif /* TAIFUSE_H */