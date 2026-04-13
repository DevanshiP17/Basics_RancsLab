/*
 * RANCS Lab - C++ Fundamentals
 * Covers: TCP, UDP, bitwise ops, packing/unpacking, endianness
 *
 * Compile:
 *   macOS/Linux: g++ -std=c++17 -o demo networking_and_binary.cpp
 *   Run server:  ./demo server
 *   Run client:  ./demo client
 *   Run demos:   ./demo (no args)
 */

#include <arpa/inet.h>   // htons, htonl, ntohs, ntohl
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <cmath>

// ============================================================================
// SECTION 1: ENDIANNESS
// ============================================================================
//
// On x86/ARM (your laptop), integers are stored LITTLE-ENDIAN.
// Network protocols (TCP/IP, sensor protocols) almost always use BIG-ENDIAN.
//
// C standard library provides conversion functions:
//   htons(x)  — host-to-network short  (16-bit): your CPU order → big-endian
//   htonl(x)  — host-to-network long   (32-bit)
//   ntohs(x)  — network-to-host short
//   ntohl(x)  — network-to-host long
//
// For 64-bit values or floats you need to do it manually (see below).

void demo_endianness() {
    std::cout << "\n============================================================\n";
    std::cout << "ENDIANNESS DEMO\n";
    std::cout << "============================================================\n";

    uint32_t value = 0x01020304;

    // Write raw bytes to a buffer manually in little-endian order
    uint8_t le_buf[4];
    std::memcpy(le_buf, &value, 4);   // memcpy respects native (LE) order

    // Convert to big-endian with htonl
    uint32_t be_value = htonl(value);
    uint8_t be_buf[4];
    std::memcpy(be_buf, &be_value, 4);

    std::cout << std::hex << std::uppercase;
    std::cout << "Value: 0x" << value << "\n";
    std::cout << "Little-endian bytes: ";
    for (int i = 0; i < 4; ++i) std::cout << std::setw(2) << std::setfill('0') << (int)le_buf[i] << " ";
    std::cout << "\nBig-endian bytes:    ";
    for (int i = 0; i < 4; ++i) std::cout << std::setw(2) << std::setfill('0') << (int)be_buf[i] << " ";
    std::cout << "\n";

    // Swap back
    uint32_t restored = ntohl(be_value);
    std::cout << "Restored via ntohl: 0x" << restored << "\n";

    // Float endian swap — htonl/ntohl only work on integers.
    // For floats, copy the float bits into a uint32, swap, then copy back.
    float speed = 13.75f;
    uint32_t raw;
    std::memcpy(&raw, &speed, 4);          // reinterpret float bytes as uint32
    uint32_t raw_be = htonl(raw);          // swap to big-endian
    uint8_t float_be_bytes[4];
    std::memcpy(float_be_bytes, &raw_be, 4);

    std::cout << "\nFloat " << std::dec << speed << " as big-endian bytes: ";
    for (int i = 0; i < 4; ++i) std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)float_be_bytes[i] << " ";

    // Unpack: reverse the process
    uint32_t raw_back = ntohl(raw_be);
    float restored_float;
    std::memcpy(&restored_float, &raw_back, 4);
    std::cout << "\nRestored float: " << std::dec << restored_float << "\n";
}


// ============================================================================
// SECTION 2: BITWISE OPERATIONS
// ============================================================================

void demo_bitwise() {
    std::cout << "\n============================================================\n";
    std::cout << "BITWISE OPERATIONS DEMO\n";
    std::cout << "============================================================\n";

    uint8_t status_byte = 0b11001010;  // = 0xCA

    // AND (&): mask off bits you want
    uint8_t GPS_LOCK_MASK = 0b01000000;
    bool gps_locked = (status_byte & GPS_LOCK_MASK) != 0;
    std::cout << "& (AND) — GPS locked? " << std::boolalpha << gps_locked << "\n";

    // Extract bits 4-5 (operating mode)
    uint8_t MODE_MASK = 0b00110000;
    uint8_t mode = (status_byte & MODE_MASK) >> 4;
    const char* modes[] = {"idle", "active", "calibrating", "unknown"};
    std::cout << "& then >> — mode: " << (int)mode << " (" << modes[mode] << ")\n";

    // Extract bits 0-3 (error code)
    uint8_t ERROR_MASK = 0b00001111;
    uint8_t error_code = status_byte & ERROR_MASK;
    std::cout << "& (AND) — error code: " << (int)error_code << "\n";

    // OR (|): set a bit
    uint8_t byte = 0x00;
    byte = byte | 0b00000100;   // set bit 2
    std::cout << "| (OR)  — set bit 2: 0x" << std::hex << (int)byte << "\n";

    // XOR (^): toggle a bit
    byte = byte ^ 0b00000100;   // toggle bit 2 off
    std::cout << "^ (XOR) — toggle bit 2: 0x" << (int)byte << "\n";

    // Shift operators
    uint8_t val = 0b00000001;
    std::cout << "<< (left shift):  " << (int)val << " << 3 = " << (int)(val << 3) << " (multiply by 8)\n";
    std::cout << ">> (right shift): " << (int)0b10000000 << " >> 3 = " << (int)(0b10000000 >> 3) << " (divide by 8)\n";

    // Build a status byte from fields
    uint8_t fault   = 1;  // bit 7
    uint8_t gps     = 1;  // bit 6
    uint8_t op_mode = 2;  // bits 4-5
    uint8_t error   = 5;  // bits 0-3
    uint8_t header = (fault << 7) | (gps << 6) | (op_mode << 4) | error;
    std::cout << std::dec << "Built header byte: 0x" << std::hex << (int)header << "\n";
}


// ============================================================================
// SECTION 3: PACKING AND UNPACKING PACKETS
// ============================================================================
//
// Sensor packet layout (big-endian, 15 bytes):
//   bytes 0-1:   uint16  message_id
//   bytes 2-5:   float32 x
//   bytes 6-9:   float32 y
//   bytes 10-13: float32 velocity
//   bytes 14:    uint8   flags
//
// In C++ there's no struct.pack — you manually copy each field into a buffer
// while applying the correct byte-order conversion.

#pragma pack(push, 1)   // disable padding so struct is exactly 15 bytes
struct SensorPacket {
    uint16_t msg_id;
    float    x;
    float    y;
    float    velocity;
    uint8_t  flags;
};
#pragma pack(pop)

constexpr size_t PACKET_SIZE = sizeof(SensorPacket);  // 15 bytes

// Helper: swap float to big-endian bytes
static void pack_float_be(uint8_t* buf, float val) {
    uint32_t raw;
    std::memcpy(&raw, &val, 4);
    raw = htonl(raw);
    std::memcpy(buf, &raw, 4);
}

// Helper: read big-endian bytes as float
static float unpack_float_be(const uint8_t* buf) {
    uint32_t raw;
    std::memcpy(&raw, buf, 4);
    raw = ntohl(raw);
    float val;
    std::memcpy(&val, &raw, 4);
    return val;
}

// Serialize SensorPacket → byte buffer (big-endian)
void pack_sensor_packet(uint8_t* buf, const SensorPacket& pkt) {
    uint16_t id_be = htons(pkt.msg_id);
    std::memcpy(buf + 0,  &id_be, 2);
    pack_float_be(buf + 2,  pkt.x);
    pack_float_be(buf + 6,  pkt.y);
    pack_float_be(buf + 10, pkt.velocity);
    buf[14] = pkt.flags;
}

// Deserialize byte buffer → SensorPacket
SensorPacket unpack_sensor_packet(const uint8_t* buf) {
    SensorPacket pkt;
    uint16_t id_be;
    std::memcpy(&id_be, buf + 0, 2);
    pkt.msg_id   = ntohs(id_be);
    pkt.x        = unpack_float_be(buf + 2);
    pkt.y        = unpack_float_be(buf + 6);
    pkt.velocity = unpack_float_be(buf + 10);
    pkt.flags    = buf[14];
    return pkt;
}

void demo_packing() {
    std::cout << "\n============================================================\n";
    std::cout << "PACKET PACKING / UNPACKING DEMO\n";
    std::cout << "============================================================\n";
    std::cout << "Packet size: " << PACKET_SIZE << " bytes\n";

    SensorPacket original = {42, 3.14f, -1.5f, 9.8f, 0xAB};
    uint8_t buf[PACKET_SIZE];
    pack_sensor_packet(buf, original);

    std::cout << "Packed bytes: ";
    for (size_t i = 0; i < PACKET_SIZE; ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buf[i] << " ";
    std::cout << "\n";

    SensorPacket unpacked = unpack_sensor_packet(buf);
    std::cout << std::dec;
    std::cout << "msg_id:   " << unpacked.msg_id   << "\n";
    std::cout << "x:        " << unpacked.x        << "\n";
    std::cout << "y:        " << unpacked.y        << "\n";
    std::cout << "velocity: " << unpacked.velocity << "\n";
    std::cout << "flags:    0x" << std::hex << (int)unpacked.flags << "\n";
}


// ============================================================================
// SECTION 4: UDP
// ============================================================================

constexpr int UDP_PORT = 5005;
constexpr const char* HOST = "127.0.0.1";

void udp_server_once() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (sockaddr*)&addr, sizeof(addr));
    std::cout << "[UDP Server] Listening on port " << UDP_PORT << "\n";

    uint8_t buf[256];
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, (sockaddr*)&client_addr, &client_len);

    if (n == PACKET_SIZE) {
        SensorPacket pkt = unpack_sensor_packet(buf);
        std::cout << "[UDP Server] Received " << n << " bytes: "
                  << "msg_id=" << pkt.msg_id
                  << " x=" << pkt.x
                  << " vel=" << pkt.velocity << "\n";
    }
    close(sockfd);
}

void udp_client_once() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(UDP_PORT);
    inet_pton(AF_INET, HOST, &dest.sin_addr);

    SensorPacket pkt = {1, 3.14f, 2.71f, 9.8f, 0xAB};
    uint8_t buf[PACKET_SIZE];
    pack_sensor_packet(buf, pkt);

    sendto(sockfd, buf, PACKET_SIZE, 0, (sockaddr*)&dest, sizeof(dest));
    std::cout << "[UDP Client] Sent " << PACKET_SIZE << " bytes\n";
    close(sockfd);
}


// ============================================================================
// SECTION 5: TCP
// ============================================================================
//
// TCP framing: we prefix every message with a 2-byte big-endian length so
// the receiver knows exactly how many bytes to read for each message.

constexpr int TCP_PORT = 5006;

void tcp_server_once() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(srv, (sockaddr*)&addr, sizeof(addr));
    listen(srv, 1);
    std::cout << "[TCP Server] Listening on port " << TCP_PORT << "\n";

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int conn = accept(srv, (sockaddr*)&client_addr, &client_len);
    std::cout << "[TCP Server] Connection accepted\n";

    // Read 2-byte length prefix
    uint8_t len_buf[2];
    recv(conn, len_buf, 2, MSG_WAITALL);
    uint16_t msg_len = ntohs(*(uint16_t*)len_buf);

    // Read exactly msg_len bytes
    uint8_t data[256];
    recv(conn, data, msg_len, MSG_WAITALL);

    SensorPacket pkt = unpack_sensor_packet(data);
    std::cout << "[TCP Server] Received " << msg_len << " bytes: "
              << "msg_id=" << pkt.msg_id
              << " x=" << pkt.x
              << " vel=" << pkt.velocity << "\n";

    close(conn);
    close(srv);
}

void tcp_client_once() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(TCP_PORT);
    inet_pton(AF_INET, HOST, &dest.sin_addr);

    connect(sockfd, (sockaddr*)&dest, sizeof(dest));

    SensorPacket pkt = {99, 1.0f, 2.0f, 3.0f, 0x0F};
    uint8_t buf[PACKET_SIZE];
    pack_sensor_packet(buf, pkt);

    // Send 2-byte length prefix followed by data
    uint16_t len_be = htons(PACKET_SIZE);
    send(sockfd, &len_be, 2, 0);
    send(sockfd, buf, PACKET_SIZE, 0);
    std::cout << "[TCP Client] Sent " << PACKET_SIZE << " bytes (+ 2-byte prefix)\n";
    close(sockfd);
}


// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "udp_server")  { udp_server_once(); return 0; }
        if (mode == "udp_client")  { udp_client_once(); return 0; }
        if (mode == "tcp_server")  { tcp_server_once(); return 0; }
        if (mode == "tcp_client")  { tcp_client_once(); return 0; }
    }

    // Run all non-network demos inline
    demo_endianness();
    demo_bitwise();
    demo_packing();

    std::cout << "\n============================================================\n";
    std::cout << "To test networking, open two terminals:\n";
    std::cout << "  Terminal 1: ./demo udp_server   (or tcp_server)\n";
    std::cout << "  Terminal 2: ./demo udp_client   (or tcp_client)\n";
    std::cout << "============================================================\n";
    return 0;
}
