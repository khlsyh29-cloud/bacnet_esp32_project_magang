#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "wifi.h"

#define UDP_PORT 47808
#define DEVICE_INSTANCE 301

// ================= I-AM RESPONSE =================
void bacnet_send_i_am(struct sockaddr_in *dest_addr)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) return;

    uint8_t packet[] = {
        0x81, 0x0A,

        0x00, 0x11,   // length

        0x01,         // NPDU
        0x00,         // no network layer msg

        0x10,         // I-AM

        // Device Instance (301)
        0x00, 0x01, 0x2D,

        // Max APDU length
        0x04, 0x00,

        // segmentation supported = none
        0x00,

        // vendor ID (random simple)
        0x00, 0x00
    };

    sendto(sock, packet, sizeof(packet), 0,
           (struct sockaddr *)dest_addr,
           sizeof(*dest_addr));

    printf("I-AM SENT OK\n");

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
