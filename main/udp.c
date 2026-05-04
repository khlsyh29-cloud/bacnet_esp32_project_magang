#include <stdio.h>
#include <string.h>
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bacnet.h"

#define UDP_PORT 47808

static uint32_t last_i_am = 0;

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

            // DEBUG
            printf("RX from %s\n", ip_str);

            // BASIC BACnet check
            int apdu_index = 4;

            if (r > apdu_index && (unsigned char)rx[apdu_index] == 0x10)
            {
                unsigned char service = (unsigned char)rx[apdu_index + 1];

                if (service == 0x08 || service == 0x00)
                {
                    uint32_t now = xTaskGetTickCount();

                    if ((now - last_i_am) > pdMS_TO_TICKS(3000))
                    {
                        last_i_am = now;

                        printf("WHO-IS detected -> sending I-AM\n");
                        bacnet_send_i_am(&source);
                    }
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}