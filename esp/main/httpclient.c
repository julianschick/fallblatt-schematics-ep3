#include "httpclient.h"

//#define WEB_SERVER "192.168.2.102"
//#define WEB_PORT 80
//#define WEB_URL "/index.php"

/*static const char *REQUEST = 
    "GET " WEB_URL " HTTP/1.1\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp32/splitflap\r\n"
    "Accept: */

static const char *REQUEST_PART1 = 
    " HTTP/1.1\r\n"
    "Host: ";

static const char *REQUEST_PART2 = 
    "\r\n"
    "User-Agent: esp32/splitflap\r\n"
    "Accept: */*\r\n"
    "\r\n";


void http_client_task()
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

    while(1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        EventBits_t uxBits = xEventGroupWaitBits(
            event_group, 
            WIFI_CONNECTED_BIT | HTTP_PULL_BIT, 
            false, 
            true, 
            0
        );

        ESP_LOGI(TAG_HTTP_CLIENT, "server=%s", http_pull_server);
        ESP_LOGI(TAG_HTTP_CLIENT, "address=%s", http_pull_address);

        if (((uxBits & (WIFI_CONNECTED_BIT | HTTP_PULL_BIT)) == (WIFI_CONNECTED_BIT | HTTP_PULL_BIT)) &&
            strlen(http_pull_server) > 0 && strlen(http_pull_address) > 0) {
            ESP_LOGI(TAG_HTTP_CLIENT, "Connected to AP and HTTP pull enabled");    

            char REQUEST[128] = "GET ";
            strcat(REQUEST, http_pull_address);
            strcat(REQUEST, REQUEST_PART1);
            strcat(REQUEST, http_pull_server);
            strcat(REQUEST, REQUEST_PART2);
            ESP_LOGI(TAG_HTTP_CLIENT, "%s", REQUEST);

            int err = getaddrinfo(http_pull_server, "80", &hints, &res);

            if(err != 0 || res == NULL) {
                ESP_LOGE(TAG_HTTP_CLIENT, "DNS lookup failed err=%d res=%p", err, res);
                continue;
            }

            /* Code to print the resolved IP.

               Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
            addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
            ESP_LOGI(TAG_HTTP_CLIENT, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

            s = socket(res->ai_family, res->ai_socktype, 0);
            if(s < 0) {
                ESP_LOGE(TAG_HTTP_CLIENT, "... Failed to allocate socket.");
                freeaddrinfo(res);
                continue;
            }
            ESP_LOGI(TAG_HTTP_CLIENT, "... allocated socket");

            if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
                ESP_LOGE(TAG_HTTP_CLIENT, "... socket connect failed errno=%d", errno);
                close(s);
                freeaddrinfo(res);
                continue;
            }

            ESP_LOGI(TAG_HTTP_CLIENT, "... connected");
            freeaddrinfo(res);

            if (write(s, REQUEST, strlen(REQUEST)) < 0) {
                ESP_LOGE(TAG_HTTP_CLIENT, "... socket send failed");
                close(s);
                continue;
            }
            ESP_LOGI(TAG_HTTP_CLIENT, "... socket send success");

            struct timeval receiving_timeout;
            receiving_timeout.tv_sec = 1;
            receiving_timeout.tv_usec = 0;
            if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                    sizeof(receiving_timeout)) < 0) {
                ESP_LOGE(TAG_HTTP_CLIENT, "... failed to set socket receiving timeout");
                close(s);
                continue;
            }
            ESP_LOGI(TAG_HTTP_CLIENT, "... set socket receiving timeout success");

            /* Read HTTP response */
            int rbefore = 0;
            r = 0;

            do {
                //bzero(recv_buf, sizeof(recv_buf));
                rbefore = r;
                r = read(s, recv_buf, sizeof(recv_buf)-1);
                /*for(int i = 0; i < r; i++) {
                    putchar(recv_buf[i]);
                }*/
            } while(r > 0);

            if (rbefore > 2) {
                recv_buf[rbefore] = 0;
                char* number_start = recv_buf + rbefore - 2;
                char* number_end;
                int flap = strtol(number_start, &number_end, 10);

                if (number_start != number_end && flap >= 0 && flap < NUMBER_OF_FLAPS) {
                    ESP_LOGI("cmd", "FLAP %d", flap);
                    xTaskNotify(flap_task_h, flap, eSetValueWithOverwrite); 
                }
            }

            ESP_LOGI(TAG_HTTP_CLIENT, "... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);
            close(s);

        } else {
            ESP_LOGI(TAG_HTTP_CLIENT, "Not connected to AP or HTTP pull disabled");
        }

        vTaskDelay(60*1000 / portTICK_PERIOD_MS);

        ESP_LOGI(TAG_HTTP_CLIENT, "Starting again!");
    }
}