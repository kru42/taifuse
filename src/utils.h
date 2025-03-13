#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <stddef.h>
#include <vitasdkkern.h>

// Allocate memory using the kernel heap
void* kernel_alloc(size_t size);

// Free memory allocated via kernel_alloc
void kernel_free(void* ptr);

// A simple realloc implementation: allocates new memory, copies over data, frees old block.
void* kernel_realloc(void* ptr, size_t new_size);

// Check if a character is a whitespace.
int my_isspace(char c);

// Convert a string to an integer.
int my_atoi(const char* s);

char* my_strtok(char* str, const char* delim);

// Convert a string to an unsigned long with base autodetection.
unsigned long my_strtoul(const char* s, int base);

// Read a line from a file descriptor.
int read_line(SceUID fd, char* buf, size_t max_len);

// Trim whitespace from a string in-place.
char* trim_whitespace(char* str);

// Duplicate a string using kernel_alloc.
char* k_strdup(const char* s);

#endif // MY_UTILS_H
