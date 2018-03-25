#include "httpserver.h"

static void http_server_netconn_serve(struct netconn *conn);

static void http_server_netconn_serve(struct netconn *conn)
{
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;

    /* Read the data from the port, blocking if nothing yet there.
    We assume the request (the part we care about) is in one netbuf */
    err = netconn_recv(conn, &inbuf);

    bool erroneous_request = true;
    bool reboot = false;

    if (err == ERR_OK) {
        netbuf_data(inbuf, (void**)&buf, &buflen);

        char instr[buflen+1];
        memcpy(instr, buf, buflen);
        instr[buflen] = 0x00;

        if (strncmp(instr, "GET /FLAP/", 10) == 0) {
            if (strlen(instr) > 10) {
                char* number_start = instr + 10;
                char* number_end;
                int flap = strtol(number_start, &number_end, 10);

                if (number_start != number_end && flap >= 0 && flap < NUMBER_OF_FLAPS) {
                    netconn_write(conn, http_resp_hdr_good, sizeof(http_resp_hdr_good)-1, NETCONN_NOCOPY);
                    netconn_write(conn, http_resp_flap, sizeof(http_resp_flap)-1, NETCONN_NOCOPY);

                    ESP_LOGI("cmd", "FLAP %d", flap);
                    xTaskNotify(flap_task_h, flap, eSetValueWithOverwrite); 
                    erroneous_request = false;                     
                }
            }

        } else if (strncmp(instr, "GET /REBOOT", 11) == 0) {
            netconn_write(conn, http_resp_hdr_good, sizeof(http_resp_hdr_good)-1, NETCONN_NOCOPY);
            netconn_write(conn, http_resp_reboot, sizeof(http_resp_reboot)-1, NETCONN_NOCOPY);

            ESP_LOGI("cmd", "REBOOT");
            reboot = true;
            erroneous_request = false;
        }

        if (erroneous_request) {
            netconn_write(conn, http_resp_hdr_bad, sizeof(http_resp_hdr_bad)-1, NETCONN_NOCOPY);
        }

        

        // strncpy(_mBuffer, buf, buflen);

        /* Is this an HTTP GET command? (only check the first 5 chars, since
        there are other formats for GET, and we're keeping it very simple )*/
        /*printf("buffer = %s \n", buf);

        if (buflen>=5 &&
            buf[0]=='G' &&
            buf[1]=='E' &&
            buf[2]=='T' &&
            buf[3]==' ' &&
            buf[4]=='/' ) {
              printf("buf[5] = %c\n", buf[5]);*/
          /* Send the HTML header
                 * subtract 1 from the size, since we dont send the \0 in the string
                 * NETCONN_NOCOPY: our data is const static, so no need to copy it
           */

           /* netconn_write(conn, http_html_hdr, sizeof(http_html_hdr)-1, NETCONN_NOCOPY);
            netconn_write(conn, http_index_hml, sizeof(http_index_hml)-1, NETCONN_NOCOPY);*/
        //}
    }
    /* Close the connection (server closes in HTTP) */
    netconn_close(conn);

    /* Delete the buffer (netconn_recv gives us ownership,
       so we have to make sure to deallocate the buffer) */
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