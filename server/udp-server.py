#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Copyright (c) 2023 fscarmen
#
# This software is released under the MIT License.
# https://opensource.org/licenses/MIT

import socket
import sys
import signal
import setproctitle

# Set process title as udp-server
setproctitle.setproctitle('udp-server')

# Define signal handler function
def signal_handler(sig, frame):
    print("\nExiting UDP server...")
    s.close()
    sys.exit(0)

# Register signal handlers
signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# Get command line arguments, default port is 12345
port = 12345
if len(sys.argv) > 1:
    port = int(sys.argv[1])

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('0.0.0.0', port))
print(f"UDP server running on port {port}...")
print("Press Ctrl+C to exit server")

while True:
    try:
        data, addr = s.recvfrom(1024)
        print(f"Received: {data} from {addr}")
        s.sendto(b"Hello UDP", addr)
    except Exception as e:
        print(f"Error occurred: {e}")