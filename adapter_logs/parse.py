#!/usr/bin/env python3

from sys import argv

gameboy = []
gameboy_device = None
adapter = []
adapter_device = None

for line in open(argv[1]):
    split = line.split("\t")
    gameboy.append(int(split[0], 16))
    adapter.append(int(split[1], 16))

assert len(gameboy) == len(adapter)

x = 0
while x < len(gameboy):
    buf = None
    recv_buf = None
    if gameboy[x] == 0x99 and gameboy[x + 1] == 0x66:
        buf = gameboy
        recv_buf = adapter
        print(">>> ", end="")
    elif adapter[x] == 0x99 and adapter[x + 1] == 0x66:
        buf = adapter
        recv_buf = gameboy
        print("<<< ", end="")
    if not buf:
        x += 1
        continue

    x += 2
    cmd = buf[x]
    print("(%02X" % cmd, end="")
    if buf[x + 1] or buf[x + 2]:
        print(" HUH??? %02X %02X" % (buf[x + 1], buf[x + 2]), end="")

    length = buf[x + 3]
    checksum = buf[x + 0] + buf[x + 1] + buf[x + 2] + buf[x + 3]
    for y in range(length):
        checksum += buf[x + 4 + y]
    checksum &= 0xFFFF
    check = buf[x + 4 + length + 0] << 8 | buf[x + 4 + length + 1]
    if check != checksum:
        print(" CHECKSUM FAIL!! (calc: %02X, got: %02X)" % (checksum, check), end="")

    device = buf[x + 4 + length + 2]
    recv_device = recv_buf[x + 4 + length + 2]
    if buf == gameboy:
        gb_dev = device
        adp_dev = recv_device
    elif buf == adapter:
        adp_dev = device
        gb_dev = recv_device
    if not gameboy_device:
        gameboy_device = gb_dev
    if not adapter_device:
        adapter_device = adp_dev
    if gb_dev != gameboy_device:
        print(" GB DEVICE MISMATCH!! (had: %02X, got: %02X)" % (gameboy_device, gb_dev), end="")
    if adp_dev != adapter_device:
        print(" ADP DEVICE MISMATCH!! (had: %02X, got: %02X)" % (adapter_device, adp_dev), end="")

    ack = buf[x + 4 + length + 3]
    recv_ack = recv_buf[x + 4 + length + 3]
    if ack != 0:
        print(" SEND ACK FAIL!! (%02X)" % ack, end="")
    if recv_ack != cmd ^ 0x80:
        print(" RECV ACK FAIL!! (%02X)" % recv_ack, end="")
    print(")")

    for i in range(0, length, 0x10):
        print("\t", end="")
        for y in range(i, i + 0x10):
            if y >= length:
                break
            print("%02X " % buf[x + 4 + y], end="")
        print()

    x += 4 + length + 4
