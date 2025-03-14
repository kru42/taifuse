#include "arm_as.h"
#include "utils.h"
#include <vitasdkkern.h>

// For simplicity we use condition AL (Always) for all instructions.
#define COND_AL 0xE

// --- Helper Functions ---
// Parse a register token (e.g. "R0", "r15"). Returns register number or -1.
static int parse_register(const char* token)
{
    if (token[0] != 'R' && token[0] != 'r')
        return -1;
    int reg = my_atoi(token + 1);
    return (reg >= 0 && reg < 16) ? reg : -1;
}

// Parse an immediate value (token starting with '#' such as "#42").
static int parse_immediate(const char* token, int* value)
{
    if (token[0] != '#')
        return -1;
    *value = my_atoi(token + 1);
    return 0;
}

// Tokenize the input instruction string by whitespace, commas and tabs.
static int tokenize(char* line, char** tokens, int max_tokens)
{
    int   count = 0;
    char* token = my_strtok(line, " ,\t");
    while (token != NULL && count < max_tokens)
    {
        tokens[count++] = token;
        token           = my_strtok(NULL, " ,\t");
    }
    return count;
}

// --- Forward declarations for encoding functions ---
static int assemble_data_processing(char** tokens, int ntokens, uint32_t* encoding);
static int assemble_multiply(char** tokens, int ntokens, uint32_t* encoding);
static int assemble_sdt(char** tokens, int ntokens, uint32_t* encoding);
static int assemble_branch(char** tokens, int ntokens, uint32_t* encoding);

// --- Main Assembly Function ---

int assemble(const char* instruction, uint32_t* output)
{
    if (!instruction || !output)
        return -1;

    // Copy the instruction into a mutable buffer.
    char buffer[256];
    strncpy(buffer, instruction, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    char* line                 = trim_whitespace(buffer);
    if (strlen(line) == 0)
        return -2; // empty instruction

    // Tokenize the line.
    char* tokens[16];
    int   ntokens = tokenize(line, tokens, 16);
    if (ntokens <= 0)
        return -3;

    // Convert the mnemonic (first token) to uppercase.
    char* mnemonic = tokens[0];
    for (char* p = mnemonic; *p; ++p)
        *p = toupper(*p);

    // Determine which instruction type to encode.
    if (strcmp(mnemonic, "AND") == 0 || strcmp(mnemonic, "EOR") == 0 || strcmp(mnemonic, "SUB") == 0 ||
        strcmp(mnemonic, "RSB") == 0 || strcmp(mnemonic, "ADD") == 0 || strcmp(mnemonic, "ADC") == 0 ||
        strcmp(mnemonic, "SBC") == 0 || strcmp(mnemonic, "RSC") == 0 || strcmp(mnemonic, "TST") == 0 ||
        strcmp(mnemonic, "TEQ") == 0 || strcmp(mnemonic, "CMP") == 0 || strcmp(mnemonic, "CMN") == 0 ||
        strcmp(mnemonic, "ORR") == 0 || strcmp(mnemonic, "MOV") == 0 || strcmp(mnemonic, "BIC") == 0 ||
        strcmp(mnemonic, "MVN") == 0)
    {
        return assemble_data_processing(tokens, ntokens, output);
    }
    else if (strcmp(mnemonic, "MUL") == 0 || strcmp(mnemonic, "MLA") == 0)
    {
        return assemble_multiply(tokens, ntokens, output);
    }
    else if (strcmp(mnemonic, "LDR") == 0 || strcmp(mnemonic, "STR") == 0)
    {
        return assemble_sdt(tokens, ntokens, output);
    }
    else if (strcmp(mnemonic, "B") == 0 || strcmp(mnemonic, "BL") == 0)
    {
        return assemble_branch(tokens, ntokens, output);
    }

    // Unrecognized instruction.
    return -4;
}

// --- Data Processing Instruction Encoding ---
//
// Formats supported:
//   mnemonic Rd, Rn, Operand2
//   mnemonic Rd, Operand2   (for MOV/MVN)
//   mnemonic Rn, Operand2   (for TST/TEQ/CMP/CMN)
//
// (This simple implementation does not support shifts/rotations on Operand2.)
static int assemble_data_processing(char** tokens, int ntokens, uint32_t* encoding)
{
    if (ntokens < 3)
        return -10;

    int opcode    = 0;
    int set_flags = 0;
    if (strcmp(tokens[0], "AND") == 0)
        opcode = 0x0;
    else if (strcmp(tokens[0], "EOR") == 0)
        opcode = 0x1;
    else if (strcmp(tokens[0], "SUB") == 0)
        opcode = 0x2;
    else if (strcmp(tokens[0], "RSB") == 0)
        opcode = 0x3;
    else if (strcmp(tokens[0], "ADD") == 0)
        opcode = 0x4;
    else if (strcmp(tokens[0], "ADC") == 0)
        opcode = 0x5;
    else if (strcmp(tokens[0], "SBC") == 0)
        opcode = 0x6;
    else if (strcmp(tokens[0], "RSC") == 0)
        opcode = 0x7;
    else if (strcmp(tokens[0], "TST") == 0)
    {
        opcode    = 0x8;
        set_flags = 1;
    }
    else if (strcmp(tokens[0], "TEQ") == 0)
    {
        opcode    = 0x9;
        set_flags = 1;
    }
    else if (strcmp(tokens[0], "CMP") == 0)
    {
        opcode    = 0xA;
        set_flags = 1;
    }
    else if (strcmp(tokens[0], "CMN") == 0)
    {
        opcode    = 0xB;
        set_flags = 1;
    }
    else if (strcmp(tokens[0], "ORR") == 0)
        opcode = 0xC;
    else if (strcmp(tokens[0], "MOV") == 0)
        opcode = 0xD;
    else if (strcmp(tokens[0], "BIC") == 0)
        opcode = 0xE;
    else if (strcmp(tokens[0], "MVN") == 0)
        opcode = 0xF;
    else
        return -11;

    uint32_t inst = 0;
    inst |= (COND_AL << 28); // condition code
    inst |= (0x0 << 26);     // bits 27-26 = 00 for data processing instructions

    // Determine if Operand2 is immediate (starts with '#').
    int immediate = (tokens[ntokens - 1][0] == '#') ? 1 : 0;
    inst |= (immediate << 25);
    inst |= (opcode << 21);
    inst |= (set_flags << 20);

    int Rd = 0, Rn = 0, operand2 = 0;
    if (strcmp(tokens[0], "MOV") == 0 || strcmp(tokens[0], "MVN") == 0)
    {
        // Format: mnemonic Rd, Operand2
        if (ntokens < 3)
            return -12;
        Rd = parse_register(tokens[1]);
        if (Rd < 0)
            return -13;
        if (immediate)
        {
            int imm;
            if (parse_immediate(tokens[2], &imm) != 0)
                return -14;
            operand2 = imm; // naive immediate encoding (rotation not handled)
        }
        else
        {
            int Rm = parse_register(tokens[2]);
            if (Rm < 0)
                return -15;
            operand2 = Rm;
        }
        // Rn is not used.
    }
    else if (strcmp(tokens[0], "TST") == 0 || strcmp(tokens[0], "TEQ") == 0 || strcmp(tokens[0], "CMP") == 0 ||
             strcmp(tokens[0], "CMN") == 0)
    {
        // Format: mnemonic Rn, Operand2 (no Rd)
        if (ntokens < 3)
            return -16;
        Rn = parse_register(tokens[1]);
        if (Rn < 0)
            return -17;
        if (immediate)
        {
            int imm;
            if (parse_immediate(tokens[2], &imm) != 0)
                return -18;
            operand2 = imm;
        }
        else
        {
            int Rm = parse_register(tokens[2]);
            if (Rm < 0)
                return -19;
            operand2 = Rm;
        }
        // Rd remains zero.
        inst |= (Rn << 16);
    }
    else
    {
        // Format: mnemonic Rd, Rn, Operand2
        if (ntokens < 4)
            return -20;
        Rd = parse_register(tokens[1]);
        Rn = parse_register(tokens[2]);
        if (Rd < 0 || Rn < 0)
            return -21;
        if (immediate)
        {
            int imm;
            if (parse_immediate(tokens[3], &imm) != 0)
                return -22;
            operand2 = imm;
        }
        else
        {
            int Rm = parse_register(tokens[3]);
            if (Rm < 0)
                return -23;
            operand2 = Rm;
        }
        inst |= (Rn << 16);
        inst |= (Rd << 12);
    }
    inst |= (operand2 & 0xFFF);
    *encoding = inst;
    return 0;
}

// --- Multiply Instruction Encoding ---
//
// Supports:
//   MUL  Rd, Rm, Rs          (Rd = Rm * Rs)
//   MLA  Rd, Rm, Rs, Rn      (Rd = Rm * Rs + Rn)
static int assemble_multiply(char** tokens, int ntokens, uint32_t* encoding)
{
    if (ntokens < 4)
        return -30;
    int is_mla = (strcmp(tokens[0], "MLA") == 0);
    int Rd     = parse_register(tokens[1]);
    int Rm     = parse_register(tokens[2]);
    int Rs     = parse_register(tokens[3]);
    if (Rd < 0 || Rm < 0 || Rs < 0)
        return -31;
    int Rn = 0;
    if (is_mla)
    {
        if (ntokens < 5)
            return -32;
        Rn = parse_register(tokens[4]);
        if (Rn < 0)
            return -33;
    }
    uint32_t inst = 0;
    inst |= (COND_AL << 28);
    // Multiply instructions use a fixed bit pattern in bits [27:4] = 000000******1001.
    inst |= (Rd << 16);
    inst |= (Rn << 12);
    inst |= (Rs << 8);
    inst |= (0x9 << 4); // fixed bits (1001)
    inst |= (Rm & 0xF);
    *encoding = inst;
    return 0;
}

// --- Single Data Transfer Instruction Encoding ---
//
// Supports a basic form of LDR/STR with pre-indexed addressing:
//   LDR/STR Rd, [Rn, #offset]
static int assemble_sdt(char** tokens, int ntokens, uint32_t* encoding)
{
    if (ntokens < 3)
        return -40;
    int is_load = (strcmp(tokens[0], "LDR") == 0);
    int Rd      = parse_register(tokens[1]);
    if (Rd < 0)
        return -41;

    // Expect address in token[2] enclosed in '[' and ']'
    char* addr = tokens[2];
    if (addr[0] != '[')
        return -42;
    addr++; // skip '['
    char* end_bracket = strchr(addr, ']');
    if (end_bracket)
        *end_bracket = '\0';

    // Tokenize address: expecting "Rn" or "Rn, #offset"
    char* subtokens[4];
    int   nt = tokenize(addr, subtokens, 4);
    if (nt < 1)
        return -43;
    int Rn = parse_register(subtokens[0]);
    if (Rn < 0)
        return -44;
    int offset = 0;
    if (nt >= 3)
    {
        // Format: Rn , #offset
        if (parse_immediate(subtokens[2], &offset) != 0)
            return -45;
    }
    uint32_t inst = 0;
    inst |= (COND_AL << 28);
    inst |= (0x1 << 26); // SDT instruction
    // I bit = 0 (immediate offset), P bit = 1 (pre-indexed), U bit = 1 (add offset)
    inst |= (1 << 24) | (1 << 23);
    // B (byte) and W (write-back) bits are 0.
    // L bit: set if load.
    if (is_load)
        inst |= (1 << 20);
    inst |= (Rn << 16);
    inst |= (Rd << 12);
    inst |= (offset & 0xFFF);
    *encoding = inst;
    return 0;
}

// --- Branch Instruction Encoding ---
//
// Supports:
//   B  <offset>
//   BL <offset>
//
// In a real assembler the label would be resolved to a PC-relative offset.
// Here we assume the offset is provided as an immediate (e.g. "#0x100").
static int assemble_branch(char** tokens, int ntokens, uint32_t* encoding)
{
    if (ntokens < 2)
        return -50;
    int link   = (strcmp(tokens[0], "BL") == 0);
    int offset = 0;
    if (parse_immediate(tokens[1], &offset) != 0)
        return -51;
    // The branch offset is a word offset (shifted right by 2).
    offset        = (offset >> 2) & 0xFFFFFF;
    uint32_t inst = 0;
    inst |= (COND_AL << 28);
    inst |= (0x5 << 25); // Branch opcode
    if (link)
        inst |= (1 << 24);
    inst |= (offset & 0xFFFFFF);
    *encoding = inst;
    return 0;
}
