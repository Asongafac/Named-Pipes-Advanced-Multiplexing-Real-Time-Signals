#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

// Log levels
#define LOG_INFO     1
#define LOG_WARN     2
#define LOG_ERROR    3
#define LOG_CRITICAL 4

// Maximum message size (must be <= PIPE_BUF for atomic writes)
#define MAX_MSG_SIZE 4096

// FIFO paths
#define FIFO1 "/tmp/fifo1"
#define FIFO2 "/tmp/fifo2"
#define FIFO3 "/tmp/fifo3"
#define ALERT_FIFO "/tmp/alert_fifo"
#define CONTROL_FIFO "/tmp/control_fifo"

// Message structure (sent over the pipe)
typedef struct {
    int level;       // LOG_INFO, LOG_WARN, LOG_ERROR, LOG_CRITICAL
    time_t timestamp;
    int msg_len;
    // char message[] follows
} LogMessage;

#endif