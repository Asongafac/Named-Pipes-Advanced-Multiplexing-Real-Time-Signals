# Named-Pipes-Advanced-Multiplexing-Real-Time-Signals
A centralized logging system where multiple processes send structured log entries to a server using named pipes (FIFOs), with epoll for monitoring and real-time signals for notifications.

**Step 1: Open 5 Terminals**

**Terminal	            Purpose**
Terminal 1	          Log Server
Terminal 2	          Alert Reader
Terminal 3	          Client 1 (fifo1)
Terminal 4	          Client 2 (fifo2)
Terminal 5	          Control Client

**Step 2: Start the Server (Terminal 1)**
./log_server

Expected output:

========================================
Lab 2: Log Server with epoll
Edge-Triggered Mode
========================================

[+] Alert FIFO ready: /tmp/alert_fifo
[+] Control FIFO ready: /tmp/control_fifo
[*] Send commands: echo "ADD /tmp/newfifo" > /tmp/control_fifo
[+] Added FIFO: /tmp/fifo1 (fd=7)
[+] Added FIFO: /tmp/fifo2 (fd=8)
[+] Added FIFO: /tmp/fifo3 (fd=9)

[*] Server running. Waiting for logs...
[*] Press Ctrl+C to stop.

**Step 3: Start Alert Reader (Terminal 2)**

./alert_reader

Expected output:

========================================
ALERT MONITOR
Watching for CRITICAL logs...
========================================

**Step 4: Send Normal Logs (Terminal 3)**

./log_client /tmp/fifo1 INFO "User Alice logged in"
./log_client /tmp/fifo1 WARN "High CPU usage detected"

Server (Terminal 1) shows:

[2024-01-15 10:30:00] [/tmp/fifo1] [INFO] User Alice logged in
[2024-01-15 10:30:05] [/tmp/fifo1] [WARN] High CPU usage detected

**Step 5: Send Critical Log (Terminal 4)**

./log_client /tmp/fifo2 CRITICAL "Database connection lost!"

Server (Terminal 1) shows:

[2024-01-15 10:31:00] [/tmp/fifo2] [CRITICAL] Database connection lost!

Alert Reader (Terminal 2) shows:

🔴 ALERT: [2024-01-15 10:31:00] CRITICAL from /tmp/fifo2: Database connection lost!

**Step 6: Dynamically Add a New FIFO (Terminal 5)**

# Create a new FIFO
mkfifo /tmp/myapp_log

# Tell server to monitor it
./control_client ADD /tmp/myapp_log

Server (Terminal 1) shows:

[+] Added FIFO: /tmp/myapp_log (fd=10)

**Step 7: Send Log to New FIFO (Terminal 5 or new terminal)**

./log_client /tmp/myapp_log ERROR "Custom app crashed"

Server shows:

[2024-01-15 10:32:00] [/tmp/myapp_log] [ERROR] Custom app crashed

**Step 8: Remove a FIFO (Terminal 5)**

./control_client REMOVE /tmp/fifo3

Server shows:

[-] Removed FIFO: /tmp/fifo3
