/**
 * File: display_usb.c
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: Display functions for the USB storage mode
 */

#include "display_usb.h"


// Static assert to ensure buffer size fits within uint32_t
_Static_assert(DISPLAY_BUFFER_SIZE <= UINT32_MAX, "Buffer size exceeds allowed limits");

void draw_usb_connected_scr() {
    // Clear the buffer first   
    u8g2_ClearBuffer(display_get_u8g2_ref());

    // Use Amstrad CPC font! 
    u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_amstrad_cpc_extended_8f);
    u8g2_DrawStr(display_get_u8g2_ref(), LEFT_PADDING_FOR_CENTER(DISPLAY_USB_CONNECTED_MESSAGE, DISPLAY_TILES_WIDTH) * 8, 100, DISPLAY_USB_CONNECTED_MESSAGE);

    // Show information about how it works
    char computer_str[80] = {0};
    snprintf(computer_str, sizeof(computer_str), "%s %s", DISPLAY_USB_INSTRUCTIONS_2, DISPLAY_TARGET_COMPUTER_NAME);

    u8g2_SetFont(display_get_u8g2_ref(), u8g2_font_squeezed_b7_tr);
    u8g2_DrawStr(display_get_u8g2_ref(), LEFT_PADDING_FOR_CENTER(DISPLAY_USB_INSTRUCTIONS_1, 65) * 6, DISPLAY_HEIGHT - (8 * 8), DISPLAY_USB_INSTRUCTIONS_1);
    u8g2_DrawStr(display_get_u8g2_ref(), LEFT_PADDING_FOR_CENTER(computer_str, 65) * 6, DISPLAY_HEIGHT - (7 * 8), computer_str);

    u8g2_DrawStr(display_get_u8g2_ref(), LEFT_PADDING_FOR_CENTER(DISPLAY_USB_INSTRUCTIONS_3, 60) * 6, DISPLAY_HEIGHT - (5 * 8), DISPLAY_USB_INSTRUCTIONS_3);
    u8g2_DrawStr(display_get_u8g2_ref(), LEFT_PADDING_FOR_CENTER(DISPLAY_BYPASS_MESSAGE, 60) * 6, DISPLAY_HEIGHT - (4 * 8), DISPLAY_BYPASS_MESSAGE);

    // Product info
    display_draw_product_info();
}

// The main function should be as follows:
void display_usb_start()
{
    size_t buffer_size = DISPLAY_BUFFER_SIZE; // Safe usage
    (void)buffer_size; // To avoid unused variable warning if not used elsewhere

    // Initialize the u8g2 library for a custom buffer
    display_setup_u8g2();

    // Set the flag to NOT-RESET the computer   
    SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_NOP);

    draw_usb_connected_scr();
    display_refresh();

    DPRINTF("Exiting fabric display\n");
}

