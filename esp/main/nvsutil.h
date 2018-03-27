#ifndef NVSUTIL_H_INCLUDED
#define NVSUTIL_H_INCLUDED

#include "globals.h"

#include "nvs.h"
#include "nvs_flash.h"

void setup_nvs();

void restore_http_pull_data();
void store_http_pull_data();

void restore_http_pull_bit();
void store_http_pull_bit();


#endif //NVSUTIL_H_INCLUDED