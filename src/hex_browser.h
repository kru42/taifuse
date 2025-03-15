#ifndef HEX_BROWSER_H
#define HEX_BROWSER_H

#include <vitasdkkern.h>
#include <stdbool.h>

// Initialize the hex browser
void hex_browser_init(void);

// Toggle the hex browser active state
void hex_browser_toggle(void);

// Check if hex browser is active
bool hex_browser_is_active(void);

// Process input for hex browser
void hex_browser_handle_input(SceCtrlButtons buttons);

// Draw the hex browser UI
void hex_browser_draw(void);

#endif /* HEX_BROWSER_H */