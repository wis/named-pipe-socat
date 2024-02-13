#include <windows.h>
#include <stdio.h>
#include <stdlib.h> // For atoi function

int main(int argc, char *argv[]) {
    HANDLE pipe;
    char buffer[1024];
    DWORD bytesWritten;
    BOOL success;
    OVERLAPPED overlapped = {0};
    DWORD timeout = 50; // Default timeout value, in milliseconds

    // Check for the correct usage and optional timeout argument
    if (argc < 2 || argc > 3) {
        printf("Usage: %s <pipe name> [timeout in milliseconds]\n", argv[0]);
        return 1;
    }

    // If timeout is provided as an argument, parse it
    if (argc == 3) {
        timeout = (DWORD)atoi(argv[2]);
    }

    // Try to open the named pipe
    char pipePath[256];
    snprintf(pipePath, sizeof(pipePath), "\\\\.\\pipe\\%s", argv[1]);

    pipe = CreateFile(
        pipePath,            // Pipe name
        GENERIC_READ | GENERIC_WRITE, // Read and Write access
        0,                   // No sharing
        NULL,                // Default security attributes
        OPEN_EXISTING,       // Opens existing pipe
        FILE_FLAG_OVERLAPPED, // Overlapped I/O
        NULL);               // No template file

    // Check if the pipe handle is valid.
    if (pipe == INVALID_HANDLE_VALUE) {
        printf("Failed to connect to the pipe. Error: %lu\n", GetLastError());
        return 1;
    }

    // Create an event for the overlapped operation
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (overlapped.hEvent == NULL) {
        printf("Failed to create event. Error: %lu\n", GetLastError());
        CloseHandle(pipe);
        return 1;
    }

    // Main loop: Read from stdin, write to pipe, attempt to read from pipe, write to stdout
    while (fgets(buffer, sizeof(buffer), stdin)) {
        // Write to the pipe
        success = WriteFile(
            pipe,                // Pipe handle
            buffer,              // Message
            strlen(buffer),      // Message length
            &bytesWritten,       // Bytes written
            NULL);               // Not overlapped

        if (!success) {
            printf("Failed to write to the pipe. Error: %lu\n", GetLastError());
            break;
        }

        // Initiate an overlapped read
        DWORD bytesRead = 0;
        success = ReadFile(
            pipe,                // Pipe handle
            buffer,              // Buffer to receive reply
            sizeof(buffer),      // Size of buffer
            &bytesRead,          // Number of bytes read
            &overlapped);        // Overlapped

        // Check if the read is pending, indicating the operation is in progress
        if (!success && GetLastError() == ERROR_IO_PENDING) {
            // Wait for the read operation to complete or a timeout
            DWORD wait = WaitForSingleObject(overlapped.hEvent, timeout); // Configurable timeout
            if (wait == WAIT_OBJECT_0) {
                // If the read operation completed, retrieve the number of bytes read
                if (GetOverlappedResult(pipe, &overlapped, &bytesRead, FALSE)) {
                    // Write the received message to stdout
                    fwrite(buffer, sizeof(char), bytesRead, stdout);
                }
            } else if (wait == WAIT_TIMEOUT) {
                // If the operation timed out, you can handle it as needed (e.g., retry or skip)
                printf("Read operation timed out.\n");
            }
        } else if (success) {
            // If ReadFile succeeded immediately, write the received message to stdout
            fwrite(buffer, sizeof(char), bytesRead, stdout);
        } else {
            printf("Read from the pipe failed. Error: %lu\n", GetLastError());
            break;
        }

        // Reset the event for the next operation
        ResetEvent(overlapped.hEvent);
    }

    // Cleanup
    CloseHandle(overlapped.hEvent);
    CloseHandle(pipe);

    return 0;
}
