/**
 * File: display_usb.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the usb storage mode diplay functions.
 */

#ifndef DISPLAY_USB_H
#define DISPLAY_USB_H

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

#define DISPLAY_USB_CONNECTED_MESSAGE "USB connected"
#define DISPLAY_USB_INSTRUCTIONS_1 "You can access the files in the SD card from your PC/Mac/Linux"
#define DISPLAY_USB_INSTRUCTIONS_2 "and copy files to the SD card to use them in your"

#define DISPLAY_USB_INSTRUCTIONS_3 "Unplug the USB cable to enter the management mode"


void display_usb_start();

#endif // DISPLAY_USB_H
