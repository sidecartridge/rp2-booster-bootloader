/**
 * File: display.h
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Header file for the shared displat functions
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <u8g2.h>

#include "constants.h"
#include "debug.h"
#include "hardware/dma.h"
#include "memfunc.h"
#include "network.h"
#include "qrcodegen.h"

// Define custom display dimensions

// For Atari ST display
#ifdef DISPLAY_ATARIST
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 200
#define DISPLAY_QR_BORDER 2
#define DISPLAY_QR_SCALE 4
#define DISPLAY_TILES_WIDTH 40
// If 1, the display will not use the framebuffer and will write directly to the
// display memory. This is useful to reduce the memory usage.
// When not using the framebuffer, the endianess swap must be done in the remote
// computer.
#define DISPLAY_BYPASS_FRAMEBUFFER 1

// #define DISPLAY_COMMAND_ADDRESS (ROM_IN_RAM_ADDRESS + 0x10000 + 8000) //
// increment 64K bytes to get the second 64K block + 8000 bytes to get the 8K
// block #define DISPLAY_HIGHRES_TRANSTABLE_ADDRESS (ROM_IN_RAM_ADDRESS +
// 0x1000) // increment 4K bytes to create the translation table
#define DISPLAY_HIGHRES_INVERT \
  0  // If 1, the highres display will be inverted, otherwise it will be normal
#define DISPLAY_BYPASS_MESSAGE "Press any SHIFT key to boot from GEMDOS."
#define DISPLAY_TARGET_COMPUTER_NAME "Atari ST"
#endif

// Buffer size calculation: width * (height / 8)
#define DISPLAY_BUFFER_SIZE (uint32_t)((DISPLAY_WIDTH / 8) * DISPLAY_HEIGHT)
#define DISPLAY_COPYRIGHT_MESSAGE "(C)GOODDATA LABS SL 2023-25"
#define DISPLAY_PRODUCT_MSG "SidecarTridge Multi-Device"
#define DISPLAY_RESET_WAIT_MESSAGE "Resetting the computer"
#define DISPLAY_RESET_FORCE_MESSAGE "Reset manually if it doesn't boot."

// Buffer for the QR code generation. Using 4K to avoid strange crash due memory
// overflow of default buffer
#define DISPLAY_QR_BUFFER_LEN_MAX 4096

// Display buffer offset
#define DISPLAY_BUFFER_OFFSET 0x8000

// Commands offset. BUFFER_OFFSET + ADDRESS_OFFSET
#define DISPLAY_COMMAND_ADDRESS_OFFSET 8000

// Highres translate table offset: BUFFER_OFFSET + TRANSTABLE_OFFSET
#define DISPLAY_HIGHRES_TRANSTABLE_OFFSET 0x1000

// Commands sent to the active loop in the display terminal application
#define DISPLAY_COMMAND_NOP 0x0       // Do nothing, clean the command buffer
#define DISPLAY_COMMAND_RESET 0x1     // Reset the computer
#define DISPLAY_COMMAND_CONTINUE 0x2  // Continue the boot process

/**
 * @brief Sends a command to the display.
 *
 * @param command The command to be sent to the display.
 */
#define SEND_COMMAND_TO_DISPLAY(command)                                \
  do {                                                                  \
    DPRINTF("Sending command: %08x\n", command);                        \
    WRITE_AND_SWAP_LONGWORD(get_display_command_address(), 0, command); \
  } while (0)

/**
 * @brief Computes the left padding needed to center a string in a line.
 *
 * @param STR  The string to center.
 * @param WIDTH The total number of characters in the line.
 *
 * @return The number of spaces (left padding) for centering the string.
 */
#define LEFT_PADDING_FOR_CENTER(STR, WIDTH) \
  (((WIDTH) > strlen(STR)) ? (((WIDTH) - (size_t)strlen(STR)) / 2) : 0)

void display_draw_qr(const uint8_t qrcode[], uint16_t display_size_x,
                     uint16_t display_size_y, uint16_t pos_x, uint16_t pos_y,
                     int border, int scale);
void display_create_qr(uint8_t qrcode[], const char* text);
void display_setup_u8g2();
void display_refresh();
void display_draw_product_info();
void display_generate_mask_table(uint32_t memory_address);
u8g2_t* display_get_u8g2_ref();
void display_scrollup(uint16_t blank_bytes);

uint32_t get_display_address();
uint32_t get_display_command_address();
uint32_t get_displays_highres_transtable_address();

#endif  // DISPLAY_H
