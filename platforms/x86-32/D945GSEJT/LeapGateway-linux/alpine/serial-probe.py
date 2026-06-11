#!/usr/bin/env python3
# Talk to the QEMU guest over the serial TCP socket: log in, run commands,
# dump output. Throwaway bring-up helper for the Alpine gateway image.
import socket
import sys
import time

HOST, PORT = "127.0.0.1", 14555
commands = sys.argv[1:] or ["echo PROBE_OK"]

s = socket.create_connection((HOST, PORT), timeout=5)
s.settimeout(0.5)
buf = b""


def drain(seconds):
    global buf
    end = time.time() + seconds
    while time.time() < end:
        try:
            data = s.recv(4096)
            if not data:
                break
            buf += data
        except socket.timeout:
            pass
    return buf


def send(line):
    s.sendall(line.encode() + b"\n")


# Wake the console, then log in if a login prompt appears.
send("")
drain(3)
if b"login:" in buf[-200:]:
    send("root")
    drain(3)

send("echo SHELL_READY")
drain(2)
if b"SHELL_READY" not in buf:
    # Maybe sitting at a stale login; try once more.
    send("root")
    drain(3)
    send("echo SHELL_READY")
    drain(2)

for cmd in commands:
    marker = "CMD_DONE_%d" % int(time.time() * 1000 % 100000)
    send(cmd + " ; echo " + marker)
    end = time.time() + 25
    while time.time() < end:
        drain(1)
        if marker.encode() in buf:
            break

out = buf.decode("utf-8", "replace")
print(out)
