#ifndef HTTPCLIENT_H_INCLUDED
#define HTTPCLIENT_H_INCLUDED

#include "globals.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

void http_client_task();

#endif //HTTPCLIENT_H_INCLUDED