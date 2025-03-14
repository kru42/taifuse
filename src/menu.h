#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <vitasdkkern.h>

// Initializes any menu-specific state (if needed in the future).
void menu_init(void);

// Handles input for the menu, including toggling visibility and navigation.
void menu_handle_input(SceCtrlButtons buttons);

// Draws the menu to the internal GUI buffer.
void menu_draw(void);

// Returns whether the menu is currently active.
bool menu_is_active(void);

// Toggles the menu's visibility.
void menu_toggle(void);

#endif // MENU_H
