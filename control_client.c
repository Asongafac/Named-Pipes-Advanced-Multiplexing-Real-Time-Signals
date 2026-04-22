#include "logger.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <command> <fifo_path>\n", argv[0]);
        printf("\nCommands: ADD, REMOVE\n");
        printf("\nExamples:\n");
        printf("  %s ADD /tmp/myapp_log\n", argv[0]);
        printf("  %s REMOVE /tmp/fifo2\n", argv[0]);
        return 1;
    }
    
    char *command = argv[1];
    char *fifo_path = argv[2];
    
    // Open control FIFO
    int fd = open(CONTROL_FIFO, O_WRONLY);
    if (fd == -1) {
        perror("open control fifo failed");
        printf("Make sure the log server is running\n");
        return 1;
    }
    
    // Send command
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s %s", command, fifo_path);
    
    ssize_t bytes = write(fd, buffer, strlen(buffer));
    if (bytes == strlen(buffer)) {
        printf("[✓] Command sent: %s %s\n", command, fifo_path);
    } else {
        printf("[✗] Failed to send command\n");
    }
    
    close(fd);
    return 0;
}