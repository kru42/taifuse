#include "utils.h"
#include <vitasdkkern.h>
#include "console.h"

// Global heap id
extern SceUID g_taifuse_pool;

// Allocate memory using the kernel heap.
void* kernel_alloc(size_t size)
{
    return ksceKernelAllocHeapMemory(g_taifuse_pool, size);
}

// Free memory allocated via kernel_alloc.
void kernel_free(void* ptr)
{
    ksceKernelFreeHeapMemory(g_taifuse_pool, ptr);
}

// A simple realloc implementation: allocates new memory, copies over data, frees old block.
// Note: Since we cannot query the old size, the caller must ensure new_size is appropriate.
void* kernel_realloc(void* ptr, size_t new_size)
{
    if (new_size == 0)
    {
        if (ptr)
            kernel_free(ptr);
        return NULL;
    }
    if (!ptr)
        return kernel_alloc(new_size);

    void* new_ptr = kernel_alloc(new_size);
    if (!new_ptr)
        return NULL;
    // Copy new_size bytes from old block (caller must ensure correctness)
    memcpy(new_ptr, ptr, new_size);
    kernel_free(ptr);
    return new_ptr;
}

// Check if a character is a whitespace.
inline int my_isspace(char c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

static char* my_strchr(const char* s, int c)
{
    while (*s)
    {
        if (*s == (char)c)
            return (char*)s; // Found the character, return its position.
        s++;
    }
    return (c == '\0') ? (char*)s : NULL; // If searching for '\0', return its position.
}

char* my_strtok(char* str, const char* delim)
{
    static char* saved = NULL;
    if (str)
        saved = str;
    if (!saved || *saved == '\0')
        return NULL;

    // Skip leading delimiters
    while (*saved && my_strchr(delim, *saved))
        saved++;
    if (*saved == '\0')
        return NULL;

    char* token_start = saved;

    // Find the end of the token
    while (*saved && !my_strchr(delim, *saved))
        saved++;

    if (*saved)
    {
        *saved = '\0'; // Null-terminate the token
        saved++;       // Move to next token
    }

    return token_start;
}

// A simple atoi implementation.
int my_atoi(const char* s)
{
    int val = 0, sign = 1;
    while (*s && my_isspace(*s))
        s++;
    if (*s == '-')
    {
        sign = -1;
        s++;
    }
    else if (*s == '+')
    {
        s++;
    }
    while (*s >= '0' && *s <= '9')
    {
        val = val * 10 + (*s - '0');
        s++;
    }
    return sign * val;
}

// A simple strtoul implementation with base autodetection (if base == 0).
unsigned long my_strtoul(const char* s, int base)
{
    unsigned long val = 0;
    while (*s && my_isspace(*s))
        s++;
    if (base == 0)
    {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        {
            base = 16;
            s += 2;
        }
        else if (s[0] == '0')
        {
            base = 8;
            s++;
        }
        else
        {
            base = 10;
        }
    }
    while (*s)
    {
        int digit = 0;
        if (*s >= '0' && *s <= '9')
        {
            digit = *s - '0';
        }
        else if (*s >= 'a' && *s <= 'f')
        {
            digit = *s - 'a' + 10;
        }
        else if (*s >= 'A' && *s <= 'F')
        {
            digit = *s - 'A' + 10;
        }
        else
        {
            break;
        }
        if (digit >= base)
            break;
        val = val * base + digit;
        s++;
    }
    return val;
}

// Read one line from a file descriptor using ksceIoRead.
int read_line(SceUID fd, char* buf, size_t max_len)
{
    size_t pos = 0;
    char   ch;
    int    ret;
    while (pos < max_len - 1)
    {
        ret = ksceIoRead(fd, &ch, 1);
        if (ret <= 0)
            break; // EOF or error.
        if (ch == '\n')
            break;
        buf[pos++] = ch;
    }
    buf[pos] = '\0';
    return (int)pos;
}

// Trim whitespace from both ends of a string (in-place).
char* trim_whitespace(char* str)
{
    char* end;
    while (my_isspace((unsigned char)*str))
        str++;
    if (*str == '\0')
        return str;
    end = str + strlen(str) - 1;
    while (end > str && my_isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
    return str;
}

// Duplicate a string using kernel_alloc.
char* k_strdup(const char* s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s) + 1;
    char*  dup = kernel_alloc(len);
    if (dup)
        memcpy(dup, s, len);
    return dup;
}
