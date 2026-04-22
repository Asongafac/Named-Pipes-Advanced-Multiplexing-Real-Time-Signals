#include "logger.h"

int send_log(const char *fifo_path, int level, const char *message) {
    // Open FIFO for writing (blocks until server opens it)
    int fd = open(fifo_path, O_WRONLY);
    if (fd == -1) {
        perror("open fifo failed");
        printf("Make sure the server is running and FIFO exists\n");
        return -1;
    }
    
    // Prepare message
    LogMessage msg;
    msg.level = level;
    msg.timestamp = time(NULL);
    msg.msg_len = strlen(message);
    
    // Build packet: header + message
    char buffer[MAX_MSG_SIZE + sizeof(LogMessage)];
    memcpy(buffer, &msg, sizeof(LogMessage));
    memcpy(buffer + sizeof(LogMessage), message, msg.msg_len);
    
    size_t total_len = sizeof(LogMessage) + msg.msg_len;
    
    // Write (atomic because total_len <= PIPE_BUF)
    ssize_t bytes = write(fd, buffer, total_len);
    
    close(fd);
    
    if (bytes == total_len) {
        const char *level_str;
        switch (level) {
            case LOG_INFO: level_str = "INFO"; break;
            case LOG_WARN: level_str = "WARN"; break;
            case LOG_ERROR: level_str = "ERROR"; break;
            case LOG_CRITICAL: level_str = "CRITICAL"; break;
        }
        printf("[✓] Sent %s log: %s\n", level_str, message);
        return 0;
    } else {
        printf("[✗] Failed to send full message\n");
        return -1;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <fifo_path> <level> <message>\n", argv[0]);
        printf("\nLevels: INFO, WARN, ERROR, CRITICAL\n");
        printf("\nExamples:\n");
        printf("  %s /tmp/fifo1 INFO \"User logged in\"\n", argv[0]);
        printf("  %s /tmp/fifo2 CRITICAL \"Disk full!\"\n", argv[0]);
        printf("  %s /tmp/myapp_log WARN \"High memory usage\"\n", argv[0]);
        return 1;
    }
    
    char *fifo_path = argv[1];
    char *level_str = argv[2];
    
    // Combine remaining args as message
    char message[512] = {0};
    for (int i = 3; i < argc; i++) {
        strcat(message, argv[i]);
        if (i < argc - 1) strcat(message, " ");
    }
    
    int level;
    if (strcmp(level_str, "INFO") == 0) level = LOG_INFO;
    else if (strcmp(level_str, "WARN") == 0) level = LOG_WARN;
    else if (strcmp(level_str, "ERROR") == 0) level = LOG_ERROR;
    else if (strcmp(level_str, "CRITICAL") == 0) level = LOG_CRITICAL;
    else {
        printf("Invalid level. Use: INFO, WARN, ERROR, CRITICAL\n");
        return 1;
    }
    
    send_log(fifo_path, level, message);
    return 0;
}