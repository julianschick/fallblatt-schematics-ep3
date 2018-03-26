#ifndef HTTPSERVER_H_INCLUDED
#define HTTPSERVER_H_INCLUDED

#include "globals.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

const static char http_resp_hdr_good[] =
    "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";
const static char http_resp_hdr_bad[] =
    "HTTP/1.1 400 Bad Request\r\nContent-type: text/html\r\n\r\n";
const static char http_resp_reboot[] = 
    "<!DOCTYPE html><html><body>REBOOT</body></html>";
const static char http_resp_flap[] = 
    "<!DOCTYPE html><html><body>FLAP XX</body></html>";
const static char http_resp_pull[] = 
    "<!DOCTYPE html><html><body>PULL</body></html>";


void http_server_task();

#endif //HTTPSERVER_H_INCLUDED