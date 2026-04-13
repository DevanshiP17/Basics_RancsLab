"""
RANCS Lab - Python Fundamentals
Covers: TCP, UDP, bitwise ops, packing/unpacking, endianness
"""

import socket
import struct
import threading

# ===========================================================================
# SECTION 1: ENDIANNESS — what it is and why it matters
# ===========================================================================
#
# Computers store multi-byte numbers (like a 32-bit int or float) in memory
# as individual bytes. The ORDER of those bytes is called "byte order" or
# "endianness".
#
#   Big-endian (BE):    most-significant byte FIRST  → network standard
#   Little-endian (LE): most-significant byte LAST   → most x86/ARM CPUs
#
# Example: the integer 0x01020304 (= 16909060 in decimal)
#   Big-endian    bytes in memory: 01 02 03 04
#   Little-endian bytes in memory: 04 03 02 01
#
# WHY DOES THIS MATTER IN YOUR LAB?
#   Sensors (LiDAR, GPS, IMU), CAN bus frames, and network packets all send
#   raw bytes. If your laptop is little-endian but the sensor sends big-endian
#   data and you forget to swap, every number you read will be garbage.
#
# struct format characters:
#   '>'  = big-endian       (network byte order)
#   '<'  = little-endian
#   '!'  = network (= big-endian, alias)
#   '='  = native byte order of this machine
#   'B'  = unsigned 8-bit int   (1 byte)
#   'H'  = unsigned 16-bit int  (2 bytes)
#   'I'  = unsigned 32-bit int  (4 bytes)
#   'f'  = 32-bit float         (4 bytes)
#   'd'  = 64-bit double        (8 bytes)

def demo_endianness():
    print("=" * 60)
    print("ENDIANNESS DEMO")
    print("=" * 60)

    value = 0x01020304

    be_bytes = struct.pack('>I', value)   # big-endian 32-bit uint
    le_bytes = struct.pack('<I', value)   # little-endian 32-bit uint

    print(f"Value:          {hex(value)}")
    print(f"Big-endian:    {be_bytes.hex(' ')}")   # 01 02 03 04
    print(f"Little-endian: {le_bytes.hex(' ')}")   # 04 03 02 01

    # Unpack back to int
    (be_val,) = struct.unpack('>I', be_bytes)
    (le_val,) = struct.unpack('<I', le_bytes)
    print(f"Unpacked BE:   {hex(be_val)}")
    print(f"Unpacked LE:   {hex(le_val)}")

    # Python int ↔ bytes (no struct needed)
    n = 305419896          # 0x12345678
    as_be = n.to_bytes(4, byteorder='big')
    as_le = n.to_bytes(4, byteorder='little')
    print(f"\n0x12345678 as big-endian bytes:    {as_be.hex(' ')}")
    print(f"0x12345678 as little-endian bytes: {as_le.hex(' ')}")

    back_be = int.from_bytes(as_be, byteorder='big')
    back_le = int.from_bytes(as_le, byteorder='little')
    print(f"Back from BE bytes: {hex(back_be)}")
    print(f"Back from LE bytes: {hex(back_le)}")


# ===========================================================================
# SECTION 2: BITWISE OPERATIONS
# ===========================================================================
#
# In autonomous vehicles you often receive packed status bytes where each
# individual bit (or group of bits) means something different.
#
# Example: a CAN bus status byte from a sensor might encode:
#   bit 7: sensor fault
#   bit 6: GPS lock
#   bits 4-5: operating mode (00=idle, 01=active, 10=calibrating)
#   bits 0-3: error code
#
# Operators:
#   &   AND  — extract / mask bits
#   |   OR   — set bits
#   ^   XOR  — toggle bits
#   ~   NOT  — invert all bits
#   <<  left shift  — multiply by 2^n, move bits to higher positions
#   >>  right shift — divide by 2^n, move bits to lower positions

def demo_bitwise():
    print("\n" + "=" * 60)
    print("BITWISE OPERATIONS DEMO")
    print("=" * 60)

    status_byte = 0b11001010   # = 0xCA = 202
    print(f"status_byte = {bin(status_byte)}  (0x{status_byte:02X})")

    # --- AND: extract (mask) specific bits ---
    # To check bit 6 (GPS lock), mask with 0b01000000 = 0x40
    GPS_LOCK_MASK = 0b01000000
    gps_locked = (status_byte & GPS_LOCK_MASK) != 0
    print(f"\n& (AND) — is GPS locked? {gps_locked}")
    print(f"  {bin(status_byte)} & {bin(GPS_LOCK_MASK)} = {bin(status_byte & GPS_LOCK_MASK)}")

    # Extract bits 4-5 (mode field) — mask then shift right
    MODE_MASK = 0b00110000   # mask bits 4 and 5
    mode = (status_byte & MODE_MASK) >> 4
    modes = {0: 'idle', 1: 'active', 2: 'calibrating', 3: 'unknown'}
    print(f"\n& then >> — operating mode bits: {bin(mode)} = {modes.get(mode)}")

    # Extract bits 0-3 (error code)
    ERROR_MASK = 0b00001111
    error_code = status_byte & ERROR_MASK
    print(f"\n& (AND) — error code: {error_code}")

    # --- OR: set a bit ---
    byte = 0b00000000
    byte = byte | 0b00000100   # set bit 2
    print(f"\n| (OR)  — set bit 2: {bin(byte)}")

    # --- XOR: toggle a bit ---
    byte = byte ^ 0b00000100   # toggle bit 2 off again
    print(f"^ (XOR) — toggle bit 2: {bin(byte)}")

    # --- Shift operators ---
    val = 0b00000001
    print(f"\n<< (left shift):  {bin(val)} << 3 = {bin(val << 3)}  (multiply by 8)")
    print(f">> (right shift): {bin(0b10000000)} >> 3 = {bin(0b10000000 >> 3)}  (divide by 8)")

    # Practical: build a packet header byte
    fault      = 1   # 1 bit  → bit 7
    gps        = 1   # 1 bit  → bit 6
    op_mode    = 2   # 2 bits → bits 4-5  (2 = calibrating)
    error      = 5   # 4 bits → bits 0-3

    header = (fault << 7) | (gps << 6) | (op_mode << 4) | error
    print(f"\nBuilt header byte: {bin(header)}  (0x{header:02X})")


# ===========================================================================
# SECTION 3: PACKING AND UNPACKING PACKETS (struct)
# ===========================================================================
#
# Real sensor packets look like:
#   bytes 0-1:   uint16  message_id
#   bytes 2-5:   float32 x_position   (meters)
#   bytes 6-9:   float32 y_position
#   bytes 10-13: float32 velocity     (m/s)
#   bytes 14:    uint8   status_flags
# Total: 15 bytes

PACKET_FORMAT = '>HfffB'   # big-endian: uint16, float, float, float, uint8
PACKET_SIZE = struct.calcsize(PACKET_FORMAT)

def pack_sensor_packet(msg_id: int, x: float, y: float, vel: float, flags: int) -> bytes:
    """Pack sensor data into a binary packet."""
    return struct.pack(PACKET_FORMAT, msg_id, x, y, vel, flags)

def unpack_sensor_packet(data: bytes):
    """Unpack a binary packet back into sensor fields."""
    msg_id, x, y, vel, flags = struct.unpack(PACKET_FORMAT, data)
    return {'msg_id': msg_id, 'x': x, 'y': y, 'velocity': vel, 'flags': flags}

def demo_packing():
    print("\n" + "=" * 60)
    print("PACKET PACKING / UNPACKING DEMO")
    print("=" * 60)
    print(f"Packet format: '{PACKET_FORMAT}'  ({PACKET_SIZE} bytes)")

    packet = pack_sensor_packet(msg_id=42, x=10.5, y=-3.2, vel=5.0, flags=0b11001010)
    print(f"\nPacked bytes ({len(packet)} bytes): {packet.hex(' ')}")

    fields = unpack_sensor_packet(packet)
    print(f"Unpacked: {fields}")

    # Convert float ↔ bytes manually (no struct)
    speed = 13.75
    as_bytes_be = struct.pack('>f', speed)
    as_bytes_le = struct.pack('<f', speed)
    print(f"\nFloat {speed} as big-endian bytes:    {as_bytes_be.hex(' ')}")
    print(f"Float {speed} as little-endian bytes: {as_bytes_le.hex(' ')}")
    print(f"Back from BE bytes: {struct.unpack('>f', as_bytes_be)[0]}")


# ===========================================================================
# SECTION 4: UDP — User Datagram Protocol
# ===========================================================================
#
# UDP is "fire and forget":
#   - No connection setup (no handshake)
#   - Fast, low overhead
#   - Packets can arrive out of order or be dropped — no guarantees
#
# WHY UDP IN AV LABS?
#   LiDAR point clouds, camera streams, radar data — these come at very high
#   rates (10–20 Hz). You can't afford the overhead of TCP for every frame.
#   A dropped frame is tolerable; latency from re-transmission is not.

HOST = '127.0.0.1'
UDP_PORT = 5005
TCP_PORT = 5006

def udp_server():
    """Receive UDP packets and unpack them."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((HOST, UDP_PORT))
        print(f"[UDP Server] Listening on {HOST}:{UDP_PORT}")
        data, addr = sock.recvfrom(1024)
        fields = unpack_sensor_packet(data)
        print(f"[UDP Server] Received from {addr}: {fields}")

def udp_client():
    """Send a binary sensor packet over UDP."""
    packet = pack_sensor_packet(msg_id=1, x=3.14, y=2.71, vel=9.8, flags=0xAB)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet, (HOST, UDP_PORT))
        print(f"[UDP Client] Sent {len(packet)} bytes to {HOST}:{UDP_PORT}")


# ===========================================================================
# SECTION 5: TCP — Transmission Control Protocol
# ===========================================================================
#
# TCP is connection-oriented:
#   - Client and server do a 3-way handshake before any data flows
#   - Guarantees delivery and ordering (re-transmits lost packets)
#   - Higher overhead than UDP
#
# WHY TCP IN AV LABS?
#   Commands sent TO the vehicle (steering, throttle, brake), configuration,
#   and logged data that must not be lost. You need the guarantee.
#
# KEY DIFFERENCE from UDP:
#   TCP is a stream — there are no built-in message boundaries.
#   You must define your own framing (e.g., a 2-byte length prefix) so the
#   receiver knows where one message ends and the next begins.

def tcp_server():
    """Accept one TCP connection, read a length-prefixed packet."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((HOST, TCP_PORT))
        srv.listen(1)
        print(f"[TCP Server] Listening on {HOST}:{TCP_PORT}")
        conn, addr = srv.accept()
        with conn:
            print(f"[TCP Server] Connection from {addr}")
            # Read 2-byte length prefix
            raw_len = conn.recv(2)
            (msg_len,) = struct.unpack('>H', raw_len)
            # Read exactly msg_len bytes
            data = b''
            while len(data) < msg_len:
                chunk = conn.recv(msg_len - len(data))
                if not chunk:
                    break
                data += chunk
            fields = unpack_sensor_packet(data)
            print(f"[TCP Server] Received ({msg_len} bytes): {fields}")

def tcp_client():
    """Connect via TCP and send a length-prefixed binary packet."""
    packet = pack_sensor_packet(msg_id=99, x=1.0, y=2.0, vel=3.0, flags=0x0F)
    length_prefix = struct.pack('>H', len(packet))   # 2-byte big-endian length
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((HOST, TCP_PORT))
        sock.sendall(length_prefix + packet)
        print(f"[TCP Client] Sent {len(packet)} bytes (+ 2-byte length prefix)")


# ===========================================================================
# SECTION 6: RUN ALL DEMOS
# ===========================================================================

if __name__ == '__main__':
    demo_endianness()
    demo_bitwise()
    demo_packing()

    print("\n" + "=" * 60)
    print("UDP DEMO")
    print("=" * 60)
    # Server runs in background thread; client runs in main thread
    t = threading.Thread(target=udp_server, daemon=True)
    t.start()
    import time; time.sleep(0.1)
    udp_client()
    t.join(timeout=2)

    print("\n" + "=" * 60)
    print("TCP DEMO")
    print("=" * 60)
    t2 = threading.Thread(target=tcp_server, daemon=True)
    t2.start()
    time.sleep(0.1)
    tcp_client()
    t2.join(timeout=2)

    print("\nAll demos complete.")
