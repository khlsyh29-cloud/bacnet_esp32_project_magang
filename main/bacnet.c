//BACNET PROJECT KHLYEEDDD GASKEN CLEKKK
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "wifi.h"

#define UDP_PORT 47808
#define DEVICE_INSTANCE 301

// ================= ENCODE OBJECT ID =================
// Encodes BACnet object identifier (type and instance)
// Format: class (10 bits) | instance (22 bits)
static void encode_object_id(uint8_t *buf, uint32_t class, uint32_t instance)
{
    uint32_t value = ((class & 0x3FF) << 22) | (instance & 0x3FFFFF);
    buf[0] = (value >> 24) & 0xFF;
    buf[1] = (value >> 16) & 0xFF;
    buf[2] = (value >> 8) & 0xFF;
    buf[3] = value & 0xFF;
}

// ================= ENCODE UNSIGNED INT =================
// Encodes BACnet unsigned integer (tag format)
static int encode_unsigned(uint8_t *buf, uint32_t value)
{
    if (value < 256)
    {
        buf[0] = 0x21;  // Context tag 0, 1 byte
        buf[1] = value & 0xFF;
        return 2;
    }
    else if (value < 65536)
    {
        buf[0] = 0x22;  // Context tag 0, 2 bytes
        buf[1] = (value >> 8) & 0xFF;
        buf[2] = value & 0xFF;
        return 3;
    }
    else
    {
        buf[0] = 0x23;  // Context tag 0, 4 bytes
        buf[1] = (value >> 24) & 0xFF;
        buf[2] = (value >> 16) & 0xFF;
        buf[3] = (value >> 8) & 0xFF;
        buf[4] = value & 0xFF;
        return 5;
    }
}

// ================= I-AM RESPONSE =================
void bacnet_send_i_am(struct sockaddr_in *dest_addr)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) return;

    uint8_t pdu[64];
    int idx = 0;

    // BVLC Header
    pdu[idx++] = 0x81;              // BVLC type (IPv4/UDP)
    pdu[idx++] = 0x0A;              // Function (Unicast NPDU)

    int len_pos = idx;              // Save position for length
    idx += 2;                        // Skip length (will fill later)

    // NPDU
    pdu[idx++] = 0x01;              // NPDU version
    pdu[idx++] = 0x00;              // Control (no flags)

    // APDU - Unconfirmed Request Service (I-AM)
    pdu[idx++] = 0x10;              // PDU Type 0, No wait (Unconfirmed-Request), I-AM service

    // I-AM Service: Device Identifier
    pdu[idx++] = 0xC4;              // Constructed, context tag 0 (device object)
    encode_object_id(&pdu[idx], 8, DEVICE_INSTANCE);  // Device class=8, instance=301
    idx += 4;

    // Max APDU Length Accepted
    pdu[idx++] = 0x2C;              // Context tag 1, 2 bytes unsigned
    pdu[idx++] = 0x04;
    pdu[idx++] = 0x00;              // 1024 bytes

    // Segmentation Supported
    pdu[idx++] = 0x4E;              // Context tag 2, enumerated
    pdu[idx++] = 0x03;              // no segmentation (3)

    // Vendor ID
    pdu[idx++] = 0x2D;              // Context tag 1, 2 bytes unsigned
    pdu[idx++] = 0x00;
    pdu[idx++] = 0xFF;              // Vendor ID 255 (experimental)

    // Fill in total length
    int total_len = idx - 4;        // Excluding BVLC type + function + length field
    pdu[len_pos] = (total_len >> 8) & 0xFF;
    pdu[len_pos + 1] = total_len & 0xFF;

    sendto(sock, pdu, idx, 0,
           (struct sockaddr *)dest_addr,
           sizeof(*dest_addr));

    printf("I-AM SENT OK (len=%d bytes)\n", idx);

    close(sock);
}

// ================= BACNET BROADCAST =================
void bacnet_send_broadcast(void)
{
    if (!wifi_is_connected())
    {
        printf("WiFi not ready, skip BACnet send\n");
        return;
    }

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(UDP_PORT);
    dest.sin_addr.s_addr = inet_addr("255.255.255.255");

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        printf("BACnet socket failed\n");
        return;
    }

    int b = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &b, sizeof(b));

    uint8_t who_is[] = {
        0x81, 0x0B,
        0x00, 0x0C,
        0x01,
        0x20,
        0xFF, 0xFF,
        0x00,
        0xFF,
        0x10, 0x08
    };

    sendto(sock, who_is, sizeof(who_is), 0,
           (struct sockaddr *)&dest,
           sizeof(dest));

    printf("BACnet Who-Is broadcast sent!\n");

    close(sock);
}
