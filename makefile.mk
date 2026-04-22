CC = gcc
CFLAGS = -Wall -Wextra

all: log_server log_client control_client alert_reader

log_server: log_server.c logger.h
	$(CC) $(CFLAGS) -o log_server log_server.c

log_client: log_client.c logger.h
	$(CC) $(CFLAGS) -o log_client log_client.c

control_client: control_client.c logger.h
	$(CC) $(CFLAGS) -o control_client control_client.c

alert_reader: alert_reader.c logger.h
	$(CC) $(CFLAGS) -o alert_reader alert_reader.c

clean:
	rm -f log_server log_client control_client alert_reader
	rm -f /tmp/fifo1 /tmp/fifo2 /tmp/fifo3
	rm -f /tmp/alert_fifo /tmp/control_fifo

run-server: log_server
	./log_server

run-alert: alert_reader
	./alert_reader

.PHONY: all clean run-server run-alert