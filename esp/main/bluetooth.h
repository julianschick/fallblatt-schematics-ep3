#ifndef BLUETOOTH_H_INCLUDED
#define BLUETOOTH_H_INCLUDED

#include "globals.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"

#define SPP_SERVER_NAME "Splitflap Serial Command Input"
#define BT_DEVICE_NAME "Splitflap"

#define BT_BUFFER_LEN 256
char bt_buffer[BT_BUFFER_LEN];
int bt_buffer_end;

void setup_bluetooth();

#endif //BLUETOOTH_H_INCLUDED