#include <vitasdkkern.h>
#include <taihen.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "cheats.h"
#include "log.h"
#include "utils.h"

//---------------------------------------------------------------------
// Stub for assembling patch code.
// Integrate your lightweight assembler here if needed.
int assemble_patch(const char* asm_code, uint8_t** patch_bytes, size_t* patch_size)
{
    // For now, this stub does nothing and returns an error.
    *patch_bytes = NULL;
    *patch_size  = 0;
    return -1;
}

//---------------------------------------------------------------------
// Cheat system functions
//---------------------------------------------------------------------

// load_cheats: Reads a cheat file using ksceIo* functions and populates cheat_groups.
// The file uses an INI-like format. Example:
//   # Simple patch cheat
//   [PCSA00092]
//   enabled = 1
//   address = 0x814F4478
//   patch   = 0x1
//   description = Toggle god mode
//
//   ; Assembly patch cheat
//   [PCSE00261]
//   enabled = 1
//   address = 0x81091D44
//   code    = mov r0, #1; ret
//   description = Increase damage output
int load_cheats(const char* filename, cheat_group_t** cheat_groups, size_t* group_count)
{
    SceUID fd = ksceIoOpen(filename, SCE_O_RDONLY, 0);
    if (fd < 0)
        return -1;

    cheat_group_t* groups       = NULL;
    size_t         groups_alloc = 0, groups_count = 0;
    cheat_group_t* current_group = NULL;

    // Temporary cheat entry storage.
    cheat_t temp_cheat;
    memset(&temp_cheat, 0, sizeof(temp_cheat));

    char line[MAX_LINE_LEN];
    while (read_line(fd, line, sizeof(line)) > 0)
    {
        char* trimmed = trim_whitespace(line);
        // Skip empty lines or comments.
        if (!*trimmed || trimmed[0] == '#' || trimmed[0] == ';')
            continue;

        if (trimmed[0] == '[')
        {
            // New group header: extract titleid.
            char* end_bracket = strchr(trimmed, ']');
            if (!end_bracket)
                continue;
            *end_bracket  = '\0';
            char* titleid = trimmed + 1;

            // Reset current_group; create a new group.
            current_group = NULL;
            if (groups_count >= groups_alloc)
            {
                groups_alloc = groups_alloc ? groups_alloc * 2 : 4;
                groups       = kernel_realloc(groups, groups_alloc * sizeof(cheat_group_t));
                if (!groups)
                {
                    ksceIoClose(fd);
                    return -1;
                }
            }
            current_group = &groups[groups_count++];
            memset(current_group, 0, sizeof(cheat_group_t));
            strncpy(current_group->titleid, titleid, MAX_TITLEID_LEN - 1);
        }
        else
        {
            // Expect a key=value format.
            char* eq = strchr(trimmed, '=');
            if (!eq)
                continue;
            *eq         = '\0';
            char* key   = trim_whitespace(trimmed);
            char* value = trim_whitespace(eq + 1);

            if (strstr(key, "enabled") != NULL)
            {
                temp_cheat.enabled = (my_atoi(value) != 0);
            }
            else if (strstr(key, "address") != NULL)
            {
                temp_cheat.address = (uintptr_t)my_strtoul(value, 0);
            }
            else if (strstr(key, "patch") != NULL)
            {
                // Simple patch: one byte.
                temp_cheat.patch = (uint8_t)my_strtoul(value, 0);
                // Ensure any previous asm_code is cleared.
                if (temp_cheat.asm_code)
                {
                    kernel_free(temp_cheat.asm_code);
                    temp_cheat.asm_code = NULL;
                }
            }
            else if (strstr(key, "code") != NULL)
            {
                // Save assembly code string.
                if (temp_cheat.asm_code)
                    kernel_free(temp_cheat.asm_code);
                temp_cheat.asm_code = k_strdup(value);
            }
            else if (strstr(key, "description") != NULL)
            {
                // Set description and complete the cheat entry.
                strncpy(temp_cheat.description, value, MAX_DESCRIPTION_LEN - 1);
                if (!current_group)
                {
                    // If no group header, create a default group.
                    if (groups_count >= groups_alloc)
                    {
                        groups_alloc = groups_alloc ? groups_alloc * 2 : 4;
                        groups       = kernel_realloc(groups, groups_alloc * sizeof(cheat_group_t));
                        if (!groups)
                        {
                            ksceIoClose(fd);
                            return -1;
                        }
                    }
                    current_group = &groups[groups_count++];
                    memset(current_group, 0, sizeof(cheat_group_t));
                    strncpy(current_group->titleid, "default", MAX_TITLEID_LEN - 1);
                }
                // Append the cheat to the current group's cheat array.
                size_t   cheat_alloc = current_group->cheat_count;
                cheat_t* new_array   = kernel_realloc(current_group->cheats, (cheat_alloc + 1) * sizeof(cheat_t));
                if (!new_array)
                    continue;
                current_group->cheats              = new_array;
                current_group->cheats[cheat_alloc] = temp_cheat;
                current_group->cheat_count++;
                // Reset temporary cheat storage.
                memset(&temp_cheat, 0, sizeof(temp_cheat));
            }
        }
    }
    ksceIoClose(fd);
    *cheat_groups = groups;
    *group_count  = groups_count;
    return 0;
}

// free_cheats: Releases all memory allocated for cheat groups and entries.
void free_cheats(cheat_group_t* cheat_groups, size_t group_count)
{
    if (!cheat_groups)
        return;

    for (size_t i = 0; i < group_count; i++)
    {
        cheat_group_t* grp = &cheat_groups[i];
        for (size_t j = 0; j < grp->cheat_count; j++)
        {
            cheat_t* ch = &grp->cheats[j];
            if (ch->asm_code)
                kernel_free(ch->asm_code);
            if (ch->patch_bytes)
                kernel_free(ch->patch_bytes);
        }
        if (grp->cheats)
            kernel_free(grp->cheats);
    }
    kernel_free(cheat_groups);
}

// set_cheats_enabled_for_game: Enable or disable all cheats for a given game title.
void set_cheats_enabled_for_game(cheat_group_t* cheat_groups, size_t group_count, const char* titleid, bool enabled)
{
    for (size_t i = 0; i < group_count; i++)
    {
        if (strcmp(cheat_groups[i].titleid, titleid) == 0)
        {
            for (size_t j = 0; j < cheat_groups[i].cheat_count; j++)
            {
                cheat_groups[i].cheats[j].enabled = enabled;
            }
        }
    }
}

// apply_cheats: For the given game title, iterate over its cheats and inject them if enabled.
void apply_cheats(SceUID pid, const char* titleid, cheat_group_t* cheat_groups, size_t group_count)
{
    for (size_t i = 0; i < group_count; i++)
    {
        if (strcmp(cheat_groups[i].titleid, titleid) == 0)
        {
            for (size_t j = 0; j < cheat_groups[i].cheat_count; j++)
            {
                cheat_t* cheat = &cheat_groups[i].cheats[j];
                if (!cheat->enabled)
                    continue;
                if (cheat->asm_code)
                {
                    // If using assembly, attempt to assemble it if not already done.
                    if (cheat->patch_bytes == NULL && cheat->patch_size == 0)
                    {
                        if (assemble_patch(cheat->asm_code, &cheat->patch_bytes, &cheat->patch_size) < 0)
                        {
                            LOG("Failed to assemble cheat code for: %s", cheat->description);
                            continue;
                        }
                    }
                    if (cheat->patch_bytes && cheat->patch_size > 0)
                    {
                        taiInjectAbsForKernel(pid, (void*)cheat->address, cheat->patch_bytes, cheat->patch_size);
                        LOG("Applied assembled cheat: %s", cheat->description);
                    }
                }
                else
                {
                    // Use simple single-byte patch.
                    taiInjectAbsForKernel(pid, (void*)cheat->address, &cheat->patch, sizeof(cheat->patch));
                    LOG("Applied cheat: %s", cheat->description);
                }
            }
            break;
        }
    }
}
