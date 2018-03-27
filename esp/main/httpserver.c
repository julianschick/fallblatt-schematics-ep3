#include "httpserver.h"
#include "nvsutil.h"

static void http_server_netconn_serve(struct netconn *conn)
{
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;

    err = netconn_recv(conn, &inbuf);

    bool erroneous_request = true;
    bool reboot = false;

    if (err == ERR_OK) {
        netbuf_data(inbuf, (void**)&buf, &buflen);

        char instr[buflen+1];
        memcpy(instr, buf, buflen);
        instr[buflen] = 0;

        if (strncmp(instr, "GET /FLAP/", 10) == 0) {
            if (strlen(instr) > 10) {
                char* number_start = instr + 10;
                char* number_end;
                int flap = strtol(number_start, &number_end, 10);

                if (number_start != number_end && flap >= 0 && flap < NUMBER_OF_FLAPS) {
                    char body[100];
                    strcpy(body, http_resp_flap);
                    body[32] = flap / 10 + 48;
                    body[33] = flap % 10 + 48;

                    netconn_write(conn, http_resp_hdr_good, sizeof(http_resp_hdr_good)-1, NETCONN_NOCOPY);
                    netconn_write(conn, body, sizeof(http_resp_flap)-1, NETCONN_NOCOPY);

                    ESP_LOGI("cmd", "FLAP %d", flap);
                    xEventGroupClearBits(event_group, HTTP_PULL_BIT);
                    xTaskNotify(flap_task_h, flap, eSetValueWithOverwrite); 
                    store_http_pull_bit();
                    erroneous_request = false;                     
                }
            }

        } else if (strncmp(instr, "GET /REBOOT", 11) == 0) {
            netconn_write(conn, http_resp_hdr_good, sizeof(http_resp_hdr_good)-1, NETCONN_NOCOPY);
            netconn_write(conn, http_resp_reboot, sizeof(http_resp_reboot)-1, NETCONN_NOCOPY);

            ESP_LOGI("cmd", "REBOOT");
            reboot = true;
            erroneous_request = false;
        } else if (strncmp(instr, "GET /PULL", 9) == 0) {
            netconn_write(conn, http_resp_hdr_good, sizeof(http_resp_hdr_good)-1, NETCONN_NOCOPY);
            netconn_write(conn, http_resp_pull, sizeof(http_resp_pull)-1, NETCONN_NOCOPY);

            ESP_LOGI("cmd", "PULL");
            xEventGroupSetBits(event_group, HTTP_PULL_BIT);
            store_http_pull_bit();

            erroneous_request = false;
        }

        if (erroneous_request) {
            netconn_write(conn, http_resp_hdr_bad, sizeof(http_resp_hdr_bad)-1, NETCONN_NOCOPY);
        }
        
    }

    netconn_close(conn);
    netbuf_delete(inbuf);

    if (reboot) {
        esp_restart();
    }
}

void http_server_task() {

    struct netconn *conn, *newconn;
    err_t err;
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, 80);
    netconn_listen(conn);
    
    do {
        err = netconn_accept(conn, &newconn);
        if (err == ERR_OK) {
            http_server_netconn_serve(newconn);
            netconn_delete(newconn);
        }
    } while(err == ERR_OK);
    
    netconn_close(conn);
    netconn_delete(conn);
}