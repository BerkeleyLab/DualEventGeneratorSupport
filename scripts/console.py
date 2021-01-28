#!/usr/bin/env python

import argparse
import socket
import sys
import termios
import threading
import tty
import signal
import sys

if ((len(sys.argv) == 2) and (sys.argv[1][0] != '-')):
    address = sys.argv[1]
else:
    parser = argparse.ArgumentParser(description='Communicate with FPGA console.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-a', '--address', default='131.243.196.245', help='Target IP name or address')
    args = parser.parse_args()
    address = args.address

consolePort = 50004

consoleSock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
consoleSock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
consoleSock.sendto(b'\1', (address, consolePort))

forceFlush = True
def fetchFromFPGA():
    global forceFlush
    while (True): 
        flush = False
        msg = consoleSock.recv(2048).decode('utf-8')
        sys.stdout.write(msg)
        if (forceFlush):
            forceFlush = False
            flush = True
        if ('\n' in msg): flush = True
        if (flush): sys.stdout.flush()

t = threading.Thread(target=fetchFromFPGA)
t.daemon = True
t.start()

def signal_handler(sig, frame):
    global ttySettings
    termios.tcsetattr(fd, termios.TCSADRAIN, ttySettings)
    sys.exit(1)

if (sys.stdin.isatty() and sys.stdout.isatty()):
    isatty = True
    fd = sys.stdin.fileno()
    ttySettings = termios.tcgetattr(fd)
    signal.signal(signal.SIGTERM, signal_handler)
    tty.setraw(fd)
else:
    isatty = False

while (True):
    c = sys.stdin.read(1)
    if (isatty):
        if (c == '\003'):
            termios.tcsetattr(fd, termios.TCSADRAIN, ttySettings)
            sys.exit(0)
    consoleSock.sendto(c.encode('utf-8'), (address, consolePort))
    forceFlush = True
