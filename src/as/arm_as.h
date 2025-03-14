#ifndef ARM_ASSEMBLER_H
#define ARM_ASSEMBLER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Assembles a single ARM 32-bit instruction string into its binary encoding.
    // Returns 0 on success, or a negative error code on failure.
    int assemble(const char* instruction, uint32_t* output);

#ifdef __cplusplus
}
#endif

#endif // ARM_ASSEMBLER_H
