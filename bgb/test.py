#!/usr/bin/env python3

import sys
import time
import socket
import struct
import subprocess

class BGBMaster():
    BGB_CMD_VERSION = 1
    BGB_CMD_JOYPAD = 101
    BGB_CMD_SYNC1 = 104
    BGB_CMD_SYNC2 = 105
    BGB_CMD_SYNC3 = 106
    BGB_CMD_STATUS = 108
    BGB_CMD_WANTDISCONNECT = 109

    def __init__(self, host="127.0.0.1", port=8765):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((host, port))
        sock.listen(1)
        self.sock = sock
        self.conn = None

        self.timeoffset = 0

    def recv(self):
        try:
            pack = self.conn.recv(8)
        except BlockingIOError:
            return None
        unpack = struct.unpack("<BBBBI", pack)
        return {
            "cmd": unpack[0],
            "b2": unpack[1],
            "b3": unpack[2],
            "b4": unpack[3],
            "timestamp": unpack[4],
        }

    def send(self, pack):
        for field in ["b2", "b3", "b4", "timestamp"]:
            if field not in pack:
                pack[field] = 0
        pack = struct.pack("<BBBBI",
            pack["cmd"],
            pack["b2"],
            pack["b3"],
            pack["b4"],
            pack["timestamp"]
        )
        self.conn.send(pack)

    def accept(self):
        conn, addr = self.sock.accept()
        self.sock.close()
        self.sock = None
        self.conn = conn
        self.conn.setblocking(True)

        pack = self.recv()
        ver = {
            "cmd": BGBMaster.BGB_CMD_VERSION,
            "b2": 1,
            "b3": 4,
            "b4": 0,
            "timestamp": 0,
        }
        for x in ver:
            if ver[x] != pack[x]:
                return False
        self.send(ver)

        # self.conn.setblocking(False)

    def handle(self):
        pack = self.recv()
        if not pack:
            return (0,)

        if pack["cmd"] in [BGBMaster.BGB_CMD_JOYPAD, BGBMaster.BGB_CMD_STATUS,
                BGBMaster.BGB_CMD_SYNC3]:
            # Nothing to do
            return (pack["cmd"],)
        if pack["cmd"] == BGBMaster.BGB_CMD_SYNC2:
            return (pack["cmd"], pack["b2"])

        print("BGBMaster.handle: Unhandled packet:", pack, file=sys.stderr)
        return (0,)

    def add_time(self, offset):
        # Offset in seconds
        self.timeoffset += offset
        self.update()

    def get_timestamp(self):
        return int((time.time() + self.timeoffset) * (1 << 21)) & 0x7FFFFFFF

    def update(self):
        pack = {
            "cmd": BGBMaster.BGB_CMD_SYNC3,
            "timestamp": self.get_timestamp(),
        }
        self.send(pack)

    def transfer(self, byte):
        pack = {
            "cmd": BGBMaster.BGB_CMD_SYNC1,
            "b2": byte,
            "b3": 0x81,
            "timestamp": self.get_timestamp(),
        }
        self.send(pack)

        byte_ret = None
        while True:
            res = self.handle()
            if res[0] == BGBMaster.BGB_CMD_SYNC2:
                byte_ret = res[1]
                break
        # print("%02X %02X" % (byte, byte_ret))
        return byte_ret

class Mobile():
    MOBILE_COMMAND_BEGIN_SESSION = 0x10
    MOBILE_COMMAND_END_SESSION = 0x11
    MOBILE_COMMAND_DIAL_TELEPHONE = 0x12
    MOBILE_COMMAND_HANG_UP_TELEPHONE = 0x13
    MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL = 0x14
    MOBILE_COMMAND_TRANSFER_DATA = 0x15
    MOBILE_COMMAND_RESET = 0x16
    MOBILE_COMMAND_TELEPHONE_STATUS = 0x17
    MOBILE_COMMAND_SIO32_MODE = 0x18
    MOBILE_COMMAND_READ_CONFIGURATION_DATA = 0x19
    MOBILE_COMMAND_WRITE_CONFIGURATION_DATA = 0x1A
    MOBILE_COMMAND_TRANSFER_DATA_END = 0x1F
    MOBILE_COMMAND_ISP_LOGIN = 0x21
    MOBILE_COMMAND_ISP_LOGOUT = 0x22
    MOBILE_COMMAND_OPEN_TCP_CONNECTION = 0x23
    MOBILE_COMMAND_CLOSE_TCP_CONNECTION = 0x24
    MOBILE_COMMAND_OPEN_UDP_CONNECTION = 0x25
    MOBILE_COMMAND_CLOSE_UDP_CONNECTION = 0x26
    MOBILE_COMMAND_DNS_QUERY = 0x28
    MOBILE_COMMAND_FIRMWARE_VERSION = 0x3F
    MOBILE_COMMAND_ERROR = 0x6E

    def __init__(self, bus):
        self.bus = bus
        self.transfer_noret = False

    def transfer(self, cmd, data):
        self.bus.update()

        full = [0x99, 0x66, cmd, 0, 0, len(data)] + data
        cksum = cmd + len(data) + sum(data)
        full += [(cksum >> 8) & 0xFF, cksum & 0xFF]
        for byte in full:
            res = self.bus.transfer(byte)
            if res != 0xD2:
                print("Mobile.transfer: Unexpected idle byte: %02X" % res, file=sys.stderr)
                return None

        self.bus.transfer(0x80)
        err = self.bus.transfer(0)
        if err != cmd ^ 0x80:
            print("Mobile.transfer: Unexpected acknowledgement byte: %02X" % err, file=sys.stderr)
            return None

        if self.transfer_noret:
            self.transfer_noret = False
            return None

        while True:
            while self.bus.transfer(0x4B) != 0x99:
                time.sleep(0.01)
                continue
            if self.bus.transfer(0x4B) == 0x66:
                break

        pack = []
        for x in range(4):
            pack.append(self.bus.transfer(0x4B))
        for x in range(pack[3]):
            pack.append(self.bus.transfer(0x4B))
        cksum = self.bus.transfer(0x4B) << 8
        cksum |= self.bus.transfer(0x4B)
        if cksum != (sum(pack) & 0xFFFF):
            print("Mobile.transfer: invalid checksum", file=sys.stderr)
            return None
        self.bus.transfer(0x80)
        self.bus.transfer(pack[0] ^ 0x80)
        if pack[0] ^ 0x80 != cmd:
            print("Mobile.transfer: unexpected packet:", pack, file=sys.stderr)
            return None
        return pack[4:]

    def transfer_noreply(self, cmd, data):
        res = self.transfer(cmd, data)
        if res is not None:
            return True
        return None

    def set_transfer_noret(self):
        # Don't wait for return for the next transfer
        self.transfer_noret = True

    def cmd_begin_session(self):
        data = list(b"NINTENDO")
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_BEGIN_SESSION, data)

    def cmd_end_session(self):
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_END_SESSION, [])

    def cmd_dial_telephone(self, number, prot=0):
        data = [prot] + list(number.encode())
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_DIAL_TELEPHONE, data)

    def cmd_hang_up_telephone(self):
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_HANG_UP_TELEPHONE, [])

    def cmd_wait_for_telephone_call(self):
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_WAIT_FOR_TELEPHONE_CALL, [])

    def cmd_transfer_data(self, conn, data=[]):
        if isinstance(data, str):
            data = data.encode()
        if not isinstance(data, list):
            data = list(data)
        if conn is None:
            conn = 0xFF
        res = self.transfer(Mobile.MOBILE_COMMAND_TRANSFER_DATA, [conn] + data)
        if not res:
            return None
        conn = res[0]
        return (conn, res[1:])

    def cmd_telephone_status(self):
        res = self.transfer(Mobile.MOBILE_COMMAND_TELEPHONE_STATUS, [])
        if not res:
            return None
        return {
            "state": res[0],
            "service": res[1],
            "flags": res[2]
        }

    def cmd_isp_login(self, s_id="nozomi", s_pass="wahaha1",
            dns1=(127,0,0,1), dns2=(127,0,0,1)):
        data = ([len(s_id)] + list(s_id.encode()) +
                [len(s_pass)] + list(s_pass.encode()) +
                list(dns1) + list(dns2))
        res = self.transfer(Mobile.MOBILE_COMMAND_ISP_LOGIN, data)
        if not res:
            return None
        return {
            "ip": (res[0], res[1], res[2], res[3]),
            "dns1": (res[4], res[5], res[6], res[7]),
            "dns2": (res[8], res[9], res[10], res[11]),
        }

    def cmd_isp_logout(self):
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_ISP_LOGOUT, [])

    def cmd_open_tcp_connection(self, ip=(0,0,0,0), port=0):
        data = list(ip) + [(port >> 8) & 0xFF, port & 0xFF]
        res = self.transfer(Mobile.MOBILE_COMMAND_OPEN_TCP_CONNECTION, data)
        if not res:
            return None
        return res[0]

    def cmd_close_tcp_connection(self, conn):
        if conn is None:
            conn = 0xFF
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_CLOSE_TCP_CONNECTION, [conn])

    def cmd_open_udp_connection(self, ip=(0,0,0,0), port=0):
        data = list(ip) + [(port >> 8) & 0xFF, port & 0xFF]
        res = self.transfer(Mobile.MOBILE_COMMAND_OPEN_UDP_CONNECTION, data)
        if not res:
            return None
        return res[0]

    def cmd_close_udp_connection(self, conn):
        return self.transfer_noreply(Mobile.MOBILE_COMMAND_CLOSE_UDP_CONNECTION, [conn])

    def cmd_dns_query(self, addr):
        res = self.transfer(Mobile.MOBILE_COMMAND_DNS_QUERY, list(addr.encode()))
        if not res:
            return None
        return res

class SimpleServer():
    def __init__(self, host="127.0.0.1", port=8766):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((host, port))
        sock.listen(1)
        self.sock = sock
        self.conn = None

    def accept(self):
        conn, addr = self.sock.accept()
        self.sock.close()
        self.sock = None
        self.conn = conn

    def recv(self, *args, **kwargs):
        return self.conn.recv(*args, **kwargs)

    def send(self, *args, **kwargs):
        self.conn.send(*args, **kwargs)

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None
        if self.conn:
            self.conn.close()
            self.conn = None

if __name__ == "__main__":
    b = BGBMaster()
    # s = subprocess.Popen(["./mobile"])
    s = subprocess.Popen(["wine", "./mobile.exe"])
    # s = subprocess.Popen(["valgrind", "--leak-check=full", "--show-leak-kinds=all", "./mobile"])
    b.accept()

    m = Mobile(b)

    # Simple session
    m.cmd_begin_session()
    print(m.cmd_telephone_status())
    m.cmd_end_session()

    # Phone connection (server)
    m.cmd_begin_session()
    t = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    t.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    m.cmd_wait_for_telephone_call()
    t.setblocking(False)
    t.connect_ex(("127.0.0.1", 1027))
    t.setblocking(True)
    m.cmd_wait_for_telephone_call()
    t.connect(("127.0.0.1", 1027))
    m.cmd_transfer_data(0xFF, "Hello World!")
    t.send(t.recv(1024))
    t.close()
    m.cmd_transfer_data(0xFF)
    m.cmd_transfer_data(0xFF)
    m.cmd_transfer_data(0xFF)
    m.cmd_hang_up_telephone()
    m.cmd_end_session()

    # Phone connection (client)
    m.cmd_begin_session()
    t = SimpleServer("127.0.0.1", 1027)
    m.cmd_dial_telephone("127000000001")
    m.cmd_transfer_data(0xFF, "Hello World!")
    t.accept()
    t.send(t.recv(1024))
    t.close()
    m.cmd_transfer_data(0xFF)
    m.cmd_transfer_data(0xFF)
    m.cmd_transfer_data(0xFF)
    m.cmd_hang_up_telephone()
    m.cmd_end_session()

    # TCP connection
    m.cmd_begin_session()
    m.cmd_dial_telephone("0755311973")
    m.cmd_isp_login()
    t = SimpleServer("127.0.0.1", 8766)
    c = m.cmd_open_tcp_connection((127,0,0,1), 8766)
    t.accept()
    m.cmd_transfer_data(c, "Hello World!")
    t.send(t.recv(1024))
    t.close()
    m.cmd_transfer_data(c)
    m.cmd_transfer_data(c)
    m.cmd_close_tcp_connection(c)
    m.cmd_isp_logout()
    m.cmd_hang_up_telephone()
    m.cmd_end_session()

    # DNS Query
    m.cmd_begin_session()
    m.cmd_dial_telephone("0755311973")
    m.cmd_isp_login()
    print(m.cmd_dns_query("example.com"))
    m.cmd_isp_logout()
    m.cmd_hang_up_telephone()
    m.cmd_end_session()

    # Auto session ending
    m.cmd_begin_session()
    b.add_time(5)
    time.sleep(0.1)
    m.cmd_begin_session()
    m.cmd_end_session()

    # Auto session ending while connecting
    # m.cmd_begin_session()
    # m.set_transfer_noret()
    # m.cmd_dial_telephone("123156189013")
    # b.add_time(5)
    # time.sleep(0.2)

    # Timed out connection
    # m.cmd_begin_session()
    # m.cmd_dial_telephone("127000000001")
    # m.cmd_end_session()

    s.terminate()
