#include "logger.h"

// Global variables
int epoll_fd;
int running = 1;
int alert_fd = -1;

// FIFO tracking structure
typedef struct {
    int fd;
    char path[256];
    int active;
} FifoInfo;

FifoInfo fifos[100];
int fifo_count = 0;

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n[!] Shutting down server...\n");
        running = 0;
    }
}

// Get current time as string
void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Process a single log message
void process_log(const char *fifo_name, LogMessage *msg, const char *message) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));
    
    const char *level_str;
    switch (msg->level) {
        case LOG_INFO: level_str = "INFO"; break;
        case LOG_WARN: level_str = "WARN"; break;
        case LOG_ERROR: level_str = "ERROR"; break;
        case LOG_CRITICAL: level_str = "CRITICAL"; break;
        default: level_str = "UNKNOWN";
    }
    
    // Print to console
    printf("[%s] [%s] [%s] %s\n", timestamp, fifo_name, level_str, message);
    fflush(stdout);
    
    // If CRITICAL, also write to alert FIFO
    if (msg->level == LOG_CRITICAL && alert_fd > 0) {
        char alert_msg[512];
        snprintf(alert_msg, sizeof(alert_msg), "[%s] CRITICAL from %s: %s\n", 
                 timestamp, fifo_name, message);
        write(alert_fd, alert_msg, strlen(alert_msg));
    }
}

// Read from a FIFO (edge-triggered - MUST read until EAGAIN)
void read_from_fifo(int fd, const char *fifo_name) {
    char buffer[MAX_MSG_SIZE + sizeof(LogMessage)];
    int offset = 0;
    ssize_t bytes;
    
    while (1) {
        bytes = read(fd, buffer + offset, sizeof(buffer) - offset);
        
        if (bytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data - this is expected for edge-triggered mode
                break;
            } else {
                perror("read error");
                break;
            }
        } else if (bytes == 0) {
            // FIFO was closed by the writer
            printf("[!] FIFO %s was closed\n", fifo_name);
            break;
        } else {
            offset += bytes;
            
            // Process all complete messages in the buffer
            int processed = 0;
            while (processed + sizeof(LogMessage) <= offset) {
                LogMessage *msg = (LogMessage*)(buffer + processed);
                
                if (processed + sizeof(LogMessage) + msg->msg_len <= offset) {
                    // Complete message received
                    char *message_text = buffer + processed + sizeof(LogMessage);
                    process_log(fifo_name, msg, message_text);
                    processed += sizeof(LogMessage) + msg->msg_len;
                } else {
                    // Incomplete message, wait for more data
                    break;
                }
            }
            
            // Move remaining data to front of buffer
            if (processed > 0) {
                memmove(buffer, buffer + processed, offset - processed);
                offset -= processed;
            }
        }
    }
}

// Add a new FIFO to epoll
int add_fifo(const char *fifo_path) {
    // Check if already added
    for (int i = 0; i < fifo_count; i++) {
        if (strcmp(fifos[i].path, fifo_path) == 0 && fifos[i].active) {
            printf("[!] FIFO %s already monitored\n", fifo_path);
            return -1;
        }
    }
    
    // Create the FIFO if it doesn't exist
    if (mkfifo(fifo_path, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo failed");
            return -1;
        }
    }
    
    // Open FIFO in non-blocking mode (critical for edge-triggered epoll)
    int fd = open(fifo_path, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("open fifo failed");
        return -1;
    }
    
    // Add to epoll with edge-triggered mode
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered!
    ev.data.fd = fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl add failed");
        close(fd);
        return -1;
    }
    
    // Store FIFO info
    strcpy(fifos[fifo_count].path, fifo_path);
    fifos[fifo_count].fd = fd;
    fifos[fifo_count].active = 1;
    fifo_count++;
    
    printf("[+] Added FIFO: %s (fd=%d)\n", fifo_path, fd);
    return 0;
}

// Remove a FIFO from epoll
void remove_fifo(int index) {
    if (index < 0 || index >= fifo_count || !fifos[index].active) {
        return;
    }
    
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fifos[index].fd, NULL);
    close(fifos[index].fd);
    printf("[-] Removed FIFO: %s\n", fifos[index].path);
    fifos[index].active = 0;
}

// Handle commands from control FIFO
void handle_control_fifo(int control_fd) {
    char buffer[512];
    ssize_t bytes = read(control_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = '\0';
        
        // Parse command: "ADD /path/to/fifo" or "REMOVE /path/to/fifo"
        char cmd[16];
        char fifo_path[256];
        
        if (sscanf(buffer, "%15s %255s", cmd, fifo_path) == 2) {
            if (strcmp(cmd, "ADD") == 0) {
                add_fifo(fifo_path);
            } else if (strcmp(cmd, "REMOVE") == 0) {
                for (int i = 0; i < fifo_count; i++) {
                    if (fifos[i].active && strcmp(fifos[i].path, fifo_path) == 0) {
                        remove_fifo(i);
                        break;
                    }
                }
            } else {
                printf("[!] Unknown command: %s\n", cmd);
            }
        } else {
            printf("[!] Invalid command format: %s\n", buffer);
        }
    }
}

// Create and add all static FIFOs (fifo1, fifo2, fifo3)
void init_static_fifos() {
    add_fifo(FIFO1);
    add_fifo(FIFO2);
    add_fifo(FIFO3);
}

int main() {
    struct sigaction sa;
    int control_fd;
    struct epoll_event events[64];
    
    printf("========================================\n");
    printf("Lab 2: Log Server with epoll\n");
    printf("Edge-Triggered Mode\n");
    printf("========================================\n\n");
    
    // Setup signal handlers
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(1);
    }
    
    // Create alert FIFO
    mkfifo(ALERT_FIFO, 0666);
    alert_fd = open(ALERT_FIFO, O_WRONLY | O_NONBLOCK);
    if (alert_fd == -1) {
        printf("[!] Warning: Could not open alert FIFO\n");
    } else {
        printf("[+] Alert FIFO ready: %s\n", ALERT_FIFO);
    }
    
    // Create control FIFO and add to epoll
    mkfifo(CONTROL_FIFO, 0666);
    control_fd = open(CONTROL_FIFO, O_RDONLY | O_NONBLOCK);
    if (control_fd == -1) {
        perror("open control fifo failed");
        exit(1);
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = control_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, control_fd, &ev);
    printf("[+] Control FIFO ready: %s\n", CONTROL_FIFO);
    printf("[*] Send commands: echo \"ADD /tmp/newfifo\" > %s\n", CONTROL_FIFO);
    
    // Initialize static FIFOs
    init_static_fifos();
    
    printf("\n[*] Server running. Waiting for logs...\n");
    printf("[*] Press Ctrl+C to stop.\n\n");
    
    // Main epoll loop
    while (running) {
        int n = epoll_wait(epoll_fd, events, 64, 1000);
        
        if (n == -1) {
            if (errno == EINTR) continue;
            perror("epoll_wait failed");
            break;
        }
        
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            
            if (fd == control_fd) {
                // Control FIFO has a command
                handle_control_fifo(control_fd);
            } else {
                // Regular log FIFO has data
                char fifo_name[256] = "unknown";
                for (int j = 0; j < fifo_count; j++) {
                    if (fifos[j].active && fifos[j].fd == fd) {
                        strcpy(fifo_name, fifos[j].path);
                        break;
                    }
                }
                read_from_fifo(fd, fifo_name);
            }
        }
    }
    
    // Cleanup
    printf("\n[*] Cleaning up...\n");
    
    for (int i = 0; i < fifo_count; i++) {
        if (fifos[i].active) {
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fifos[i].fd, NULL);
            close(fifos[i].fd);
            unlink(fifos[i].path);
        }
    }
    
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, control_fd, NULL);
    close(control_fd);
    unlink(CONTROL_FIFO);
    
    if (alert_fd > 0) {
        close(alert_fd);
        unlink(ALERT_FIFO);
    }
    
    close(epoll_fd);
    printf("[*] Server stopped.\n");
    
    return 0;
}