/**
 * File: term.c
 * Author: Diego Parrilla Santamaría
 * Date: January 2025
 * Copyright: 2025 - GOODDATA LABS
 * Description: Online terminal
 */

#include "include/term.h"

static TransmissionProtocol last_protocol;
static bool last_protocol_valid = false;

static uint32_t memory_shared_address = 0;
static uint32_t memory_random_token_address = 0;
static uint32_t memory_random_token_seed_address = 0;

// Command handlers
static void cmdClear(const char *arg);
static void cmdExit(const char *arg);
static void cmdHelp(const char *arg);
static void cmdSettings(const char *arg);
static void cmdReset(const char *arg);
static void cmdSave(const char *arg);
static void cmdErase(const char *arg);
static void cmdGet(const char *arg);
static void cmdPutInt(const char *arg);
static void cmdPutBool(const char *arg);
static void cmdPutString(const char *arg);
static void cmdNetwork(const char *arg);
static void cmdUnknown(const char *arg);

// Command table
static const Command commands[] = {
    {"clear", cmdClear},       {"exit", cmdExit},
    {"help", cmdHelp},         {"settings", cmdSettings},
    {"reset", cmdReset},       {"save", cmdSave},
    {"erase", cmdErase},       {"get", cmdGet},
    {"put_int", cmdPutInt},    {"put_bool", cmdPutBool},
    {"put_str", cmdPutString}, {"network", cmdNetwork}};

// Number of commands in the table
const size_t numCommands = sizeof(commands) / sizeof(commands[0]);

/**
 * @brief Callback that handles the protocol command received.
 *
 * This callback copy the content of the protocol to the last_protocol
 * structure. The last_protocol_valid flag is set to true to indicate that the
 * last_protocol structure contains a valid protocol. We return to the
 * dma_irq_handler_lookup function to continue asap with the next
 *
 * @param protocol The TransmissionProtocol structure containing the protocol
 * information.
 */
static void __not_in_flash_func(handle_protocol_command)(
    const TransmissionProtocol *protocol) {
  // Copy the content of protocol to last_protocol
  memcpy(&last_protocol, protocol, sizeof(TransmissionProtocol));
  last_protocol_valid = true;
};

static void __not_in_flash_func(handle_protocol_checksum_error)(
    const TransmissionProtocol *protocol) {
  DPRINTF("Checksum error detected (ID=%u, Size=%u)\n", protocol->command_id,
          protocol->payload_size);
}

// Interrupt handler for DMA completion
void __not_in_flash_func(term_dma_irq_handler_lookup)(void) {
  // Read the rom3 signal and if so then process the command
  bool rom3_gpio = (1ul << ROM3_GPIO) & sio_hw->gpio_in;
  // bool rom4_gpio = (1ul << ROM4_GPIO) & sio_hw->gpio_in;
  // DPRINTF("ROM3_GPIO: %d ROM4_GPIO: %d\n", rom3_gpio, rom4_gpio);

  dma_hw->ints1 = 1u << 2;

  // Read the ROM3_GPIO and if active then process the command
  if (!rom3_gpio) {
    // Read the address to process and invert the highest bit
    uint16_t addr_lsb = dma_hw->ch[2].al3_read_addr_trig ^ 0x8000;
    tprotocol_parse(addr_lsb, handle_protocol_command,
                    handle_protocol_checksum_error);
  }
}

void term_init(void) {
  // Memory shared address
  memory_shared_address = (unsigned int)&__rom_in_ram_start__;
  memory_random_token_address =
      memory_shared_address + TERM_RANDOM_TOKEN_OFFSET;
  memory_random_token_seed_address =
      memory_shared_address + TERM_RANDON_TOKEN_SEED_OFFSET;
  SET_SHARED_VAR(TERM_HARDWARE_TYPE, 0, memory_shared_address,
                 TERM_SHARED_VARIABLES_OFFSET);  // Clean the hardware type
  SET_SHARED_VAR(TERM_HARDWARE_VERSION, 0, memory_shared_address,
                 TERM_SHARED_VARIABLES_OFFSET);  // Clean the hardware version

  // Initialize the random seed (add this line)
  srand(time(NULL));
  // Init the random token seed in the shared memory for the next command
  uint32_t new_random_seed_token =
      rand();  // Generate a new random 32-bit value
  TPROTO_SET_RANDOM_TOKEN(memory_random_token_seed_address,
                          new_random_seed_token);
}

static char screen[TERM_SCREEN_SIZE];
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

// Store previous cursor position for block removal
static uint8_t prev_cursor_x = 0;
static uint8_t prev_cursor_y = 0;

// Buffer to keep track of chars entered between newlines
static char input_buffer[TERM_INPUT_BUFFER_SIZE];
static size_t input_length = 0;

// Clears entire screen buffer and resets cursor
void term_clear_screen(void) {
  memset(screen, 0, TERM_SCREEN_SIZE);
  cursor_x = 0;
  cursor_y = 0;
  display_term_clear();
}

// Scrolls the screen up by one row
static void term_scroll_up(void) {
  memmove(screen, screen + TERM_SCREEN_SIZE_X,
          TERM_SCREEN_SIZE - TERM_SCREEN_SIZE_X);
  memset(screen + TERM_SCREEN_SIZE - TERM_SCREEN_SIZE_X, 0, TERM_SCREEN_SIZE_X);
  display_scrollup(TERM_DISPLAY_ROW_BYTES);
}

// Prints a character to the screen, handles scrolling
static void term_put_char(char c) {
  screen[cursor_y * TERM_SCREEN_SIZE_X + cursor_x] = c;
  display_term_char(cursor_x, cursor_y, c);
  cursor_x++;
  if (cursor_x >= TERM_SCREEN_SIZE_X) {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= TERM_SCREEN_SIZE_Y) {
      term_scroll_up();
      cursor_y = TERM_SCREEN_SIZE_Y - 1;
    }
  }
}

// Renders a single character, with special handling for newline and carriage
// return
static void term_render_char(char c) {
  // First, remove the old block by restoring the character
  display_term_char(prev_cursor_x, prev_cursor_y, ' ');
  if (c == '\n' || c == '\r') {
    // Move to new line
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= TERM_SCREEN_SIZE_Y) {
      term_scroll_up();
      cursor_y = TERM_SCREEN_SIZE_Y - 1;
    }
  } else {
    term_put_char(c);
  }

  // Draw a block at the new cursor position
  display_term_cursor(cursor_x, cursor_y);
  // Update previous cursor coords
  prev_cursor_x = cursor_x;
  prev_cursor_y = cursor_y;
}

// Prints entire screen to stdout
static void term_print_screen(void) {
  for (int y = 0; y < TERM_SCREEN_SIZE_Y; y++) {
    for (int x = 0; x < TERM_SCREEN_SIZE_X; x++) {
      char c = screen[y * TERM_SCREEN_SIZE_X + x];
      putchar(c ? c : ' ');
    }
    putchar('\n');
  }
}

// If we want to do a normal programmatic print ignoring input buffer:
static void term_print_string(const char *str) {
  while (*str) {
    term_render_char(*str);
    str++;
  }
  display_term_refresh();
}

// Called whenever a character is entered by the user
// This is the single point of entry for user input
static void term_input_char(char c) {
  // Check for backspace
  if (c == '\b') {
    display_term_char(prev_cursor_x, prev_cursor_y, ' ');

    // If we have chars in input_buffer, remove last char
    if (input_length > 0) {
      input_length--;
      // Also reflect on screen
      // same as term_backspace
      if (cursor_x == 0) {
        if (cursor_y > 0) {
          cursor_y--;
          cursor_x = TERM_SCREEN_SIZE_X - 1;
        } else {
          // already top-left corner
          return;
        }
      } else {
        cursor_x--;
      }
      screen[cursor_y * TERM_SCREEN_SIZE_X + cursor_x] = 0;
      display_term_char(cursor_x, cursor_y, ' ');
    }

    display_term_cursor(cursor_x, cursor_y);
    prev_cursor_x = cursor_x;
    prev_cursor_y = cursor_y;
    display_term_refresh();
    return;
  }

  // If it's newline or carriage return, finalize the line
  if (c == '\n' || c == '\r') {
    // Render newline on screen
    term_render_char('\n');

    // Process input_buffer
    // Split the input into command and argument
    char command[TERM_INPUT_BUFFER_SIZE] = {0};
    char arg[TERM_INPUT_BUFFER_SIZE] = {0};
    sscanf(input_buffer, "%63s %63[^\n]", command,
           arg);  // Split at the first space

    bool command_found = false;
    for (size_t i = 0; i < numCommands; i++) {
      if (strcmp(command, commands[i].command) == 0) {
        commands[i].handler(arg);  // Pass the argument to the handler
        command_found = true;
      }
    }
    if ((!command_found) && (strlen(command) > 0)) {
      term_print_string("Command not found. Type 'help' for commands.\n");
    }

    // Reset input buffer
    memset(input_buffer, 0, TERM_INPUT_BUFFER_SIZE);
    input_length = 0;

    term_print_string("> ");
    display_term_refresh();
    return;
  }

  // If it's a normal character
  // Add it to input_buffer if there's space
  if (input_length < TERM_INPUT_BUFFER_SIZE - 1) {
    input_buffer[input_length++] = c;
    // Render char on screen
    term_render_char(c);

    // show block cursor

    display_term_refresh();
  } else {
    // Buffer full, ignore or beep?
  }
}

// For convenience, we can also have a helper function that "types" a string as
// if typed by user
static void term_type_string(const char *str) {
  while (*str) {
    term_input_char(*str);
    str++;
  }
}

// Invoke this function to process the commands from the active loop in the main
// function
void __not_in_flash_func(term_loop)() {
  if (last_protocol_valid) {
    // Shared by all commands
    // Read the random token from the command and increment the payload pointer
    // to the first parameter available in the payload
    uint32_t random_token = TPROTO_GET_RANDOM_TOKEN(last_protocol.payload);
    uint16_t *payloadPtr = ((uint16_t *)(last_protocol).payload);
    uint16_t command_id = last_protocol.command_id;
    DPRINTF(
        "Command ID: %d. Size: %d. Random token: 0x%08X, Checksum: 0x%04X\n",
        last_protocol.command_id, last_protocol.payload_size, random_token,
        last_protocol.final_checksum);

    // Jump the random token
    TPROTO_NEXT32_PAYLOAD_PTR(payloadPtr);

    // Read the payload parameters
    if ((last_protocol.payload_size > 4) &&
        (last_protocol.payload_size <= 20)) {
      DPRINTF("Payload D3: 0x%04X\n", TPROTO_GET_PAYLOAD_PARAM32(payloadPtr));
      TPROTO_NEXT32_PAYLOAD_PTR(payloadPtr);
    }
    if ((last_protocol.payload_size > 8) &&
        (last_protocol.payload_size <= 20)) {
      DPRINTF("Payload D4: 0x%04X\n", TPROTO_GET_PAYLOAD_PARAM32(payloadPtr));
      TPROTO_NEXT32_PAYLOAD_PTR(payloadPtr);
    }
    if ((last_protocol.payload_size > 12) &&
        (last_protocol.payload_size <= 20)) {
      DPRINTF("Payload D5: 0x%04X\n", TPROTO_GET_PAYLOAD_PARAM32(payloadPtr));
      TPROTO_NEXT32_PAYLOAD_PTR(payloadPtr);
    }
    if ((last_protocol.payload_size > 16) &&
        (last_protocol.payload_size <= 20)) {
      DPRINTF("Payload D6: 0x%04X\n", TPROTO_GET_PAYLOAD_PARAM32(payloadPtr));
      TPROTO_NEXT32_PAYLOAD_PTR(payloadPtr);
    }

    // Handle the command
    switch (last_protocol.command_id) {
      case APP_TERMINAL_START: {
        display_term_start(40, 25);
        term_clear_screen();
        term_print_string("Type 'help' for available commands.\n");
        term_input_char('\n');
        SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_TERM);
        DPRINTF("Send command to display: DISPLAY_COMMAND_TERM\n");
      } break;
      case APP_TERMINAL_KEYSTROKE: {
        uint16_t *payload = ((uint16_t *)(last_protocol).payload);
        // Jump the random token
        TPROTO_NEXT32_PAYLOAD_PTR(payload);
        // Extract the 32 bit payload
        uint32_t payload32 = TPROTO_GET_PAYLOAD_PARAM32(payload);
        // Extract the ascii code from the payload lower 8 bits
        char keystroke = (char)(payload32 & 0xFF);
        // Get the shift key status from the higher byte of the payload
        uint8_t shift_key = (payload32 & 0xFF000000) >> 24;
        // Get the keyboard scan code from the bits 16 to 23 of the payload
        uint8_t scan_code = (payload32 & 0xFF0000) >> 16;
        if (keystroke >= 0x20 && keystroke <= 0x7E) {
          // Print the keystroke and the shift key status
          DPRINTF("Keystroke: %c. Shift key: %d, Scan code: %d\n", keystroke,
                  shift_key, scan_code);
        } else {
          // Print the keystroke and the shift key status
          DPRINTF("Keystroke: %d. Shift key: %d, Scan code: %d\n", keystroke,
                  shift_key, scan_code);
        }
        term_input_char(keystroke);
        break;
      }
      default:
        // Unknown command
        DPRINTF("Unknown command\n");
        break;
    }
    if (memory_random_token_address != 0) {
      // Set the random token in the shared memory
      TPROTO_SET_RANDOM_TOKEN(memory_random_token_address, random_token);

      // Init the random token seed in the shared memory for the next command
      uint32_t new_random_seed_token =
          rand();  // Generate a new random 32-bit value
      TPROTO_SET_RANDOM_TOKEN(memory_random_token_seed_address,
                              new_random_seed_token);
    }
  }
  last_protocol_valid = false;
}

// Command handlers
void cmdHelp(const char *arg) {
  term_print_string("Available commands:\n");
  term_print_string(" General:\n");
  term_print_string("  clear   - Clear the terminal screen\n");
  term_print_string("  exit    - Exit the terminal\n");
  term_print_string("  help    - Show available commands\n");
  term_print_string("  network - Show network info\n");
  term_print_string("  reset   - Reset computer and device\n");
  term_print_string(" Settings:\n");
  term_print_string("  settings- Show settings\n");
  term_print_string("  save    - Save settings\n");
  term_print_string("  erase   - Erase settings\n");
  term_print_string("  get     - Get setting (requires key)\n");
  term_print_string("  put_int - Set integer (key and value)\n");
  term_print_string("  put_bool- Set boolean (key and value)\n");
  term_print_string("  put_str - Set string (key and value)\n");
}

void cmdClear(const char *arg) { term_clear_screen(); }

void cmdExit(const char *arg) {
  term_print_string("Exiting terminal...\n");
  // Send continue to desktop command
  SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_CONTINUE);
}

void cmdReset(const char *arg) {
  term_print_string("Resetting computer and device...\n");
  sleep_ms(1000);
  SEND_COMMAND_TO_DISPLAY(DISPLAY_COMMAND_RESET);
  reset_device();
}

void cmdSettings(const char *arg) {
  char *buffer = (char *)malloc(2048);
  if (buffer == NULL) {
    term_print_string("Error: Out of memory.\n");
    return;
  }
  settings_print(gconfig_getContext(), buffer);
  term_print_string(buffer);
  free(buffer);
}

void cmdSave(const char *arg) {
  settings_save(gconfig_getContext(), true);
  term_print_string("Settings saved.\n");
}

void cmdErase(const char *arg) {
  settings_erase(gconfig_getContext());
  term_print_string("Settings erased.\n");
}

void cmdGet(const char *arg) {
  if (arg && strlen(arg) > 0) {
    SettingsConfigEntry *entry =
        settings_find_entry(gconfig_getContext(), &arg[0]);
    if (entry != NULL) {
      TPRINTF("Key: %s\n", entry->key);
      TPRINTF("Type: ");
      switch (entry->dataType) {
        case SETTINGS_TYPE_INT:
          TPRINTF("INT");
          break;
        case SETTINGS_TYPE_STRING:
          TPRINTF("STRING");
          break;
        case SETTINGS_TYPE_BOOL:
          TPRINTF("BOOL");
          break;
        default:
          TPRINTF("UNKNOWN");
          break;
      }
      TPRINTF("\n");
      TPRINTF("Value: %s\n", entry->value);
    } else {
      TPRINTF("Key not found.\n");
    }
  } else {
    TPRINTF("No key provided for 'get' command.\n");
  }
}

void cmdPutInt(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};
  int value = 0;
  if (sscanf(arg, "%s %d", key, &value) == 2) {
    int err = settings_put_integer(gconfig_getContext(), key, value);
    if (err == 0) {
      TPRINTF("Key: %s\n", key);
      TPRINTF("Value: %d\n", value);
    }
  } else {
    TPRINTF("Invalid arguments for 'put_int' command.\n");
  }
}

void cmdPutBool(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};

  char valueStr[8] = {
      0};  // Small buffer to store the value string (e.g., "true", "false")
  bool value;

  // Scan the key and the value string
  if (sscanf(arg, "%s %7s", key, valueStr) == 2) {
    // Convert the value_str to lowercase for easier comparison
    for (int i = 0; valueStr[i]; i++) {
      valueStr[i] = tolower(valueStr[i]);
    }

    // Check if the value is true or false
    if (strcmp(valueStr, "true") == 0 || strcmp(valueStr, "t") == 0 ||
        strcmp(valueStr, "1") == 0) {
      value = true;
    } else if (strcmp(valueStr, "false") == 0 || strcmp(valueStr, "f") == 0 ||
               strcmp(valueStr, "0") == 0) {
      value = false;
    } else {
      TPRINTF(
          "Invalid boolean value. Use 'true', 'false', 't', 'f', '1', or "
          "'0'.\n");
      return;
    }

    // Store the boolean value
    int err = settings_put_bool(gconfig_getContext(), key, value);
    if (err == 0) {
      TPRINTF("Key: %s\n", key);
      TPRINTF("Value: %s\n", value ? "true" : "false");
    }
  } else {
    TPRINTF(
        "Invalid arguments for 'put_bool' command. Usage: put_bool <key> "
        "<true/false>\n");
  }
}

void cmdPutString(const char *arg) {
  char key[SETTINGS_MAX_KEY_LENGTH] = {0};

  // Scan the first word (key)
  if (sscanf(arg, "%s", key) == 1) {
    // Find the position after the key in the argument string
    const char *value = strchr(arg, ' ');
    if (value) {
      // Move past the space to the start of the value
      value++;

      // Check if the value is non-empty
      if (strlen(value) > 0) {
        int err = settings_put_string(gconfig_getContext(), key, value);
        if (err == 0) {
          TPRINTF("Key: %s\n", key);
          TPRINTF("Value: %s\n", value);
        }
      } else {
        int err = settings_put_string(gconfig_getContext(), key, value);
        if (err == 0) {
          TPRINTF("Key: %s\n", key);
          TPRINTF("Value: <EMPTY>\n");
        }
      }
    } else {
      int err = settings_put_string(gconfig_getContext(), key, value);
      if (err == 0) {
        TPRINTF("Key: %s\n", key);
        TPRINTF("Value: <EMPTY>\n");
      }
    }
  } else {
    TPRINTF("Invalid arguments for 'put_string' command.\n");
  }
}

void cmdNetwork(const char *arg) {
  ip4_addr_t ip = network_getCurrentIp();

  // Print network info
  TPRINTF("Network info:\n");

  SettingsConfigEntry *entry =
      settings_find_entry(gconfig_getContext(), PARAM_WIFI_SSID);
  if (entry != NULL) {
    TPRINTF("SSID: %s\n", entry->value);
  } else {
    TPRINTF("SSID: Not found\n");
  }
  TPRINTF("  IP: %s\n", ip4addr_ntoa(&ip));
  TPRINTF("  URL: http://%s\n", ip4addr_ntoa(&ip));
  // Print the hostname
  char *hostname =
      settings_find_entry(gconfig_getContext(), PARAM_HOSTNAME)->value;
  if ((hostname != NULL) && (strlen(hostname) > 0)) {
    TPRINTF("  Hostname: %s\n", hostname);
  } else {
    TPRINTF("  Hostname: sidecart\n");
  }
}

void cmdUnknown(const char *arg) {
  TPRINTF("Unknown command. Type 'help' for a list of commands.\n");
}
