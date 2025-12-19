/**
 * File: display_fabric.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the fabric mode diplay functions.
 */

#ifndef DISPLAY_FABRIC_H
#define DISPLAY_FABRIC_H

#include "debug.h"
#include "constants.h"
#include "network.h"
#include "memfunc.h"
#include "display.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "hardware/dma.h"

#include <u8g2.h>
#include "qrcodegen.h"

#define DISPLAY_CONNECTION_STEP1_MSG "Step 1: Connect to device WiFi"
#define DISPLAY_CONNECTION_STEP2_MSG "Step 2: Open device portal"

void display_fabric_start(const char *ssid, const char *password,
                          const char *auth, const char *url1,
                          const char *url2);
void display_fabric_portal_connection();
void display_fabric_wifi_change_status(uint8_t wifi_status);
void display_fabric_portal_change_status(uint8_t portal_status);
void display_fabric_reset();

#endif // DISPLAY_FABRIC_H
