#ifndef CHEATS_H
#define CHEATS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_LINE_LEN        256
#define MAX_DESCRIPTION_LEN 128
#define MAX_TITLEID_LEN     32

// A cheat entry. For simple patches, set 'asm_code' to NULL and use 'patch'.
// Otherwise, if 'asm_code' is not NULL, you may call assemble_patch() later
// to convert the assembly instructions into binary patch data.
typedef struct
{
    bool      enabled;     // Activate/deactivate this cheat
    uintptr_t address;     // Memory address to patch
    uint8_t   patch;       // Single-byte patch (if asm_code is NULL)
    char*     asm_code;    // Textual assembly code (NULL if not used)
    uint8_t*  patch_bytes; // Assembled binary code (allocated later)
    size_t    patch_size;  // Size of assembled code (0 if using simple patch)
    char      description[MAX_DESCRIPTION_LEN];
} cheat_t;

// A group of cheats for one game (identified by titleid).
typedef struct
{
    char     titleid[MAX_TITLEID_LEN]; // Game title ID, e.g. "PCSA00092"
    cheat_t* cheats;                   // Dynamically allocated array of cheats
    size_t   cheat_count;              // How many cheats are in this group
} cheat_group_t;

/**
 * Loads cheats from a file (e.g. "ux0:data/taifuse/cheats.txt").
 *
 * The file is formatted in a simple INI-like style. Example:
 *
 *   # Example cheat file
 *   [PCSA00092]
 *   enabled = 1
 *   address = 0x814F4478
 *   patch   = 0x1
 *   description = Toggle god mode
 *
 *   ; Assembly patch example (if you later implement an assembler)
 *   [PCSE00261]
 *   enabled = 1
 *   address = 0x81091D44
 *   code    = mov r0, #1; ret
 *   description = Increase damage output
 *
 * When a cheat entry is complete (a description is encountered) it is added to the current group.
 *
 * On success, returns 0 and sets *cheat_groups (an allocated array of cheat_group_t)
 * and *group_count accordingly. The caller must free the returned memory via free_cheats().
 */
int load_cheats(const char* filename, cheat_group_t** cheat_groups, size_t* group_count);

/**
 * Frees the allocated cheat groups and their cheat arrays.
 */
void free_cheats(cheat_group_t* cheat_groups, size_t group_count);

/**
 * (Optional) Assemble a textual patch code into binary patch data.
 * This is just a stub; you can integrate an assembler as needed.
 * If successful, allocates *patch_bytes and sets *patch_size.
 * Returns 0 on success or a negative error code.
 */
int assemble_patch(const char* asm_code, uint8_t** patch_bytes, size_t* patch_size);

// Sets the enabled flag for all cheats in a group matching the titleid.
void set_cheats_enabled_for_game(cheat_group_t* cheat_groups, size_t group_count, const char* titleid, bool enabled);

// Applies cheats for a given game (identified by titleid) to process pid.
void apply_cheats(SceUID pid, const char* titleid, cheat_group_t* cheat_groups, size_t group_count);

#endif // CHEATS_H
