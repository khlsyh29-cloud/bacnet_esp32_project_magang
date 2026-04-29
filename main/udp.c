#include <stdio.h>
#include <string.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bacnet.h"

#define UDP_PORT 47808

// ================= UDP TASK =================
void udp_task(void *pv)
{
    char rx[128];
    char ip_str[32];

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        printf("Socket failed\n");
        vTaskDelete(NULL);
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("Bind failed\n");
        close(sock);
        vTaskDelete(NULL);
    }

    printf("UDP listening on %d...\n", UDP_PORT);

    while (1)
    {
        struct sockaddr_in source;
        socklen_t len = sizeof(source);

        int r = recvfrom(sock, rx, sizeof(rx), 0,
                         (struct sockaddr *)&source, &len);

        if (r > 0)
        {
            inet_ntoa_r(source.sin_addr, ip_str, sizeof(ip_str));

            printf("Received from %s (HEX): ", ip_str);
            for (int i = 0; i < r; i++)
                printf("%02X ", (unsigned char)rx[i]);
            printf("\n");
            
            printf("Received from %s (ASCII): ", ip_str);
            for (int i = 0; i < r; i++)
            {
                unsigned char c = (unsigned char)rx[i];
                if (c >= 32 && c <= 126)
                    printf("%c", c);
                else
                    printf(".");
            }
            printf("\n");

            // WHO-IS detection - check APDU type and service choice
            // Packet structure: BVLC(2) + Length(2) + NPDU(varies) + APDU
            // rx[10] = APDU type (0x10 = Unconfirmed-Request)
            // rx[11] = Service choice (0x08 = WHO-IS, or 0x00 for filtered WHO-IS)
            if (r >= 12 && (unsigned char)rx[10] == 0x10)
            {
                unsigned char service = (unsigned char)rx[11];
                if (service == 0x08 || service == 0x00)  // WHO-IS service
                {
                    printf("WHO-IS detected -> sending I-AM\n");
                    bacnet_send_i_am(&source);
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
