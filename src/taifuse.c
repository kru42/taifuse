#include "taifuse.h"
#include "console.h"

#include <vitasdkkern.h>
#include <taihen.h>

#define MAX_MESSAGE_LENGTH 256 // Adjust as necessary

// A custom function to calculate the length of a userland string
int custom_strlen(const char* user_str)
{
    char temp_char;
    int  length = 0;

    while (1)
    {
        // Read one byte at a time from userland memory
        if (ksceKernelMemcpyUserToKernel(&temp_char, user_str + length, 1) < 0)
        {
            return -1; // If copying fails, return error
        }

        // If the byte is a null terminator, stop
        if (temp_char == '\0')
        {
            break;
        }

        length++;
    }

    return length;
}

int tf_console_print(char* message)
{
    uint32_t state;
    ENTER_SYSCALL(state);

    // Allocate a buffer to hold the copied message in kernel space
    char kernel_buffer[MAX_MESSAGE_LENGTH];

    // Calculate the length of the message manually
    int length = custom_strlen(message);
    if (length < 0)
    {
        // ksceDebugPrintf("Failed to get string length from userland.\n");
        EXIT_SYSCALL(state);
        return -1;
    }

    if (length >= MAX_MESSAGE_LENGTH)
    {
        length = MAX_MESSAGE_LENGTH - 1; // Prevent buffer overflow
    }

    // Use ksceKernelMemcpyUserToKernel to copy the data
    if (ksceKernelMemcpyUserToKernel(kernel_buffer, message, length) < 0)
    {
        // ksceDebugPrintf("Failed to copy message from userland to kernel space.\n");
        EXIT_SYSCALL(state);
        return -1;
    }

    // Null-terminate the string to ensure safe usage in the kernel
    kernel_buffer[length] = '\0';

    // Finally log to console
    console_log(kernel_buffer);

    EXIT_SYSCALL(state);
    return 0;
}