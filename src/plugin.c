#include "plugin.h"
#include "log.h"
#include <vitasdkkern.h>
#include <taihen.h>
#include <string.h>
#include <stdio.h>

int load_plugins_for_game(SceUID pid, const char* titleid)
{
    // Check if it's a game (starts with PC)
    if (strncmp(titleid, "PC", 2) != 0)
    {
        return -1;
    }

    char dir_path[256];
    snprintf(dir_path, sizeof(dir_path), "ux0:data/taifuse/plugins/%s", titleid);

    SceUID dfd = ksceIoDopen(dir_path);
    if (dfd < 0)
    {
        LOG("No plugins found for %s (directory %.*s does not exist)", titleid, 100, dir_path);
        return dfd;
    }

    int         ret = 0;
    SceIoDirent dir;
    memset(&dir, 0, sizeof(dir));

    while (ksceIoDread(dfd, &dir) > 0)
    {
        // Check if file name ends with ".suprx"
        const char* ext = strrchr(dir.d_name, '.');
        if (!ext || strcmp(ext, ".suprx") != 0)
            continue;

        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, dir.d_name);

        int status;
        SceUID modid = ksceKernelLoadStartModuleForPid(pid, file_path, 0, NULL, 0, NULL, &status);
        if (modid < 0)
        {
            LOG("Failed to load plugin: %.*s (error 0x%08X)", 100, file_path, modid);
            ret = modid; // record error code (last error)
        }
        else
        {
            LOG("Loaded plugin: %.*s (modid %d)", 100, file_path, modid);
        }
    }

    ksceIoDclose(dfd);
    return ret;
}
