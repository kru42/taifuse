#ifndef CHEATS_H
#define CHEATS_H

typedef struct
{
    const char *titleid;
    uint32_t cheat_code;
    bool is_patch;
    void *address;
};


#endif