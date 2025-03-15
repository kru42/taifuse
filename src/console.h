#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <vitasdkkern.h>

// Maximum number of lines in the console log.
#define CONSOLE_MAX_LINES 20
// Maximum characters per console line.
#define CONSOLE_MAX_LINE_LENGTH 256

// Initializes the console.
void console_init(void);
// Deinitializes the console.
void console_deinit(void);

// Toggles the console's visibility.
void console_toggle(void);
// Returns whether the console is active.
bool console_is_active(void);

// Clears the console log.
void console_clear(void);

// Logs a message to the console
void console_log(const char* buffer);

void console_handle_input(SceCtrlButtons buttons);

// Draws the console on the overlay.
void console_draw(void);

#endif // CONSOLE_H
