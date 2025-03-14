#ifndef PLUGIN_H
#define PLUGIN_H

#include <psp2kern/types.h>

/**
 * Loads all suprx plugin modules located in the directory
 * "ux0:data/taifuse/plugins/{GAME_TITLEID}/" into the target process (pid).
 *
 * @param pid      Process ID of the target.
 * @param titleid  The game title ID (e.g. "PCSA00092") used to build the plugin path.
 * @return         0 on success, or a negative error code if any load fails.
 */
int load_plugins_for_game(SceUID pid, const char* titleid);

#endif // PLUGIN_H
