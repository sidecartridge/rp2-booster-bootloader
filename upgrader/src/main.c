/**
 * File: main.c
 * Author: Diego Parrilla Santamaría
 * Date: Novemeber 2023
 * Copyright: 2023 - GOODDATA LABS SL
 * Description: C file that launches the ROM emulator
 */

#include "constants.h"
#include "debug.h"
#include "gconfig.h"
#include "sd_card.h"

#define UF2_BLOCK_SIZE 512

enum {
  SETTINGS_ADDRESS_OFFSET = 0x1FF000,
  BUFFER_SIZE = 4096,
  MAGIC_NUMBER = 0x1234,
  VERSION_NUMBER = 0x0001
};

static inline void jump_to_booster_app() {
  // This code jumps to the Booster application at the top of the flash memory
  // The reason to perform this jump is for performance reasons
  // This code should be placed at the beginning of the main function, and
  // should be executed if the SELECT signal or the BOOSTER app is selected Set
  // VTOR register, set stack pointer, and jump to reset
  __asm__ __volatile__(
      "mov r0, %[start]\n"
      "ldr r1, =%[vtable]\n"
      "str r0, [r1]\n"
      "ldmia r0, {r0, r1}\n"
      "msr msp, r0\n"
      "bx r1\n"
      :
      : [start] "r"((unsigned int)&_booster_app_flash_start + 256),
        [vtable] "X"(PPB_BASE + M0PLUS_VTOR_OFFSET)
      :);
  DPRINTF("You should never reach this point\n");
}

/*
  Reads a UF2 file from the microSD card, ignores each block’s targetAddr,
  and writes each block’s payload linearly into flash starting at flashAddress.
  Before writing, it erases [flashAddress..(flashAddress + flashSize)).

  We no longer write one UF2 block at a time. Instead, we accumulate multiple
  payloads from consecutive UF2 blocks into a dynamic buffer of size
  'userPageSize' until that buffer is full or we run out of data.
  Then we flush that buffer to the flash.

  If the leftover region in flash is smaller than the userPageSize,
  we do partial writes, ensuring we never write beyond the allocated region.

  Parameters:
    - filename: Path to the UF2 file on SD card.
    - flashAddress: Start address in RP2040 flash memory.
    - flashSize: Size of the region to erase/write (must be a multiple of 4096).
    - userPageSize: The chunk size for flash writes (must be a multiple of 256).

  Returns FRESULT error codes (FR_OK if successful, etc.).

  Steps:
    1) Verify userPageSize is multiple of 256.
    2) Open UF2 file.
    3) Erase [flashAddress..flashAddress+flashSize).
    4) Repeatedly read 512-byte UF2 blocks from file, parse out the payload,
       and accumulate that payload in a dynamic buffer up to userPageSize.
    5) Once the buffer is full (or no more data), write it to flash.
    6) If flash region remains but we have partial leftover data < userPageSize,
       do a final partial write.

  NOTE:
    - We do partial writes if leftover flash size is smaller than userPageSize.
    - The user must ensure enough RAM is available to allocate userPageSize
  bytes.
*/

static inline FRESULT __not_in_flash_func(storeUF2FileToFlash)(
    const char *filename, uint32_t flashAddress, uint32_t flashSize,
    uint32_t userPageSize) {
  FIL file;
  FRESULT res;
  FSIZE_t uf2FileSize;
  UINT bytesRead;

  // Check page size
  if (userPageSize == 0 || (userPageSize % FLASH_PAGE_SIZE) != 0) {
    DPRINTF("Error: userPageSize (%u) is not a multiple of %u.\n", userPageSize,
            FLASH_PAGE_SIZE);
    return FR_INVALID_PARAMETER;
  }

  // Open file
  res = f_open(&file, filename, FA_READ);
  if (res != FR_OK) {
    DPRINTF("Error opening file %s: %d\n", filename, res);
    return res;
  }

  // Erase region
  uint32_t offset = flashAddress - XIP_BASE;
  if (flashSize == 0) {
    // If user passed 0, nothing to do?
    f_close(&file);
    return FR_INVALID_PARAMETER;
  }

  DPRINTF("Erasing %u bytes of flash at offset 0x%X\n", flashSize, offset);
  {
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(offset, flashSize);
    restore_interrupts(ints);
  }

  uf2FileSize = f_size(&file);
  DPRINTF("UF2 file size: %u bytes\n", (unsigned int)uf2FileSize);
  DPRINTF("Erased %u bytes of flash at offset 0x%X\n", (unsigned int)flashSize,
          offset);

  // We allocate a dynamic buffer of size userPageSize.
  // We'll fill it with multiple UF2 block payloads.
  uint8_t *accumBuf = (uint8_t *)malloc(userPageSize);
  if (!accumBuf) {
    DPRINTF("Error: Unable to allocate %u bytes for accumBuf.\n", userPageSize);
    f_close(&file);
    return FR_INT_ERR;
  }

  // We'll keep track of how many bytes we have in accumBuf.
  uint32_t accumUsed = 0;

  // We'll read the file block by block (512 bytes each) and parse payload.
  uint32_t currentOffset = offset;
  bool done = false;

  while (!done) {
    // Attempt to read 512 bytes for a UF2 block.
    uint8_t uf2block[UF2_BLOCK_SIZE];
    res = f_read(&file, uf2block, sizeof(uf2block), &bytesRead);

    if (res != FR_OK) {
      DPRINTF("Error reading file: %d\n", res);
      break;  // error
    }
    if (bytesRead == 0) {
      // End of file.
      done = true;
    } else if (bytesRead < UF2_BLOCK_SIZE) {
      DPRINTF("Warning: incomplete UF2 block (%u bytes). Ignored.\n",
              bytesRead);
      done = true;
    } else {
      // parse the block
      const uint32_t UF2_MAGIC_START0 = 0x0A324655;
      const uint32_t UF2_MAGIC_START1 = 0x9E5D5157;
      uint32_t magic0 = ((uint32_t *)uf2block)[0];
      uint32_t magic1 = ((uint32_t *)uf2block)[1];

      if (magic0 != UF2_MAGIC_START0 || magic1 != UF2_MAGIC_START1) {
        DPRINTF("Invalid UF2 magic. Skipping block.\n");
        continue;  // skip
      }

      uint32_t payloadSize = ((uint32_t *)uf2block)[4];
      if (payloadSize == 0 || payloadSize > 476) {
        DPRINTF("Invalid UF2 payload size: %u. Skipping.\n", payloadSize);
        continue;
      }

      // The payload is located at offset 32 in the block.
      uint8_t *payload = uf2block + 32;

      // Now we have 'payloadSize' bytes to accumulate in accumBuf.
      // We might have to break it up if there's not enough space left.
      uint32_t payloadPos = 0;

      while (payloadPos < payloadSize) {
        uint32_t leftover = payloadSize - payloadPos;
        uint32_t spaceInBuf = userPageSize - accumUsed;
        uint32_t toCopy = (leftover < spaceInBuf) ? leftover : spaceInBuf;

        // Copy to the accumulation buffer.
        memcpy(accumBuf + accumUsed, payload + payloadPos, toCopy);
        accumUsed += toCopy;
        payloadPos += toCopy;

        // If accumBuf is full, flush it to flash.
        if (accumUsed == userPageSize) {
          // Check leftover region.
          uint32_t leftoverRegion = (offset + flashSize) - currentOffset;
          if (leftoverRegion == 0) {
            // No more space in flash.
            done = true;
            break;
          }

          // We'll write either userPageSize or leftoverRegion if smaller.
          uint32_t chunk =
              (userPageSize <= leftoverRegion) ? userPageSize : leftoverRegion;

          // Program the flash with 'chunk' bytes.
          {
            uint32_t ints = save_and_disable_interrupts();
            flash_range_program(currentOffset, accumBuf, chunk);
            restore_interrupts(ints);
          }

          currentOffset += chunk;
          accumUsed = 0;  // reset for next round

          // If leftoverRegion < userPageSize, we wrote partially.
          if (chunk < userPageSize) {
            done = true;
            break;
          }
        }

        if (done) break;
      }
    }

    if (done) {
      // either end of file or no more flash space
      break;
    }
  }

  // If we have leftover data in accumBuf, flush it partially.
  if (accumUsed > 0 && !res) {
    // leftover region check
    uint32_t leftoverRegion = (offset + flashSize) - currentOffset;
    if (leftoverRegion > 0) {
      uint32_t chunk =
          (accumUsed <= leftoverRegion) ? accumUsed : leftoverRegion;
      // Program partial.
      {
        uint32_t ints = save_and_disable_interrupts();
        flash_range_program(currentOffset, accumBuf, chunk);
        restore_interrupts(ints);
      }
      currentOffset += chunk;
    }
  }

  free(accumBuf);
  f_close(&file);

  return FR_OK;
}

int main() {
#if defined(_DEBUG) && (_DEBUG != 0)
  // Initialize chosen serial port
  stdio_init_all();
  setvbuf(stdout, NULL, _IONBF,
          1);  // specify that the stream should be unbuffered

  // Only startup information to display
  DPRINTF(
      "\n\nUpgrader: upgrade the firmware in the device. %s (%s). %s mode.\n\n",
      RELEASE_VERSION, RELEASE_DATE, _DEBUG ? "DEBUG" : "RELEASE");

  // Show information about the frequency and voltage
  int current_clock_frequency_khz = RP2040_CLOCK_FREQ_KHZ;
  const char *current_voltage = VOLTAGE_VALUES[RP2040_VOLTAGE];
  DPRINTF("Clock frequency: %i KHz\n", current_clock_frequency_khz);
  DPRINTF("Voltage: %s\n", current_voltage);
  DPRINTF("PICO_FLASH_SIZE_BYTES: %i\n", PICO_FLASH_SIZE_BYTES);

  unsigned int flash_length = (unsigned int)&_booster_app_flash_start -
                              (unsigned int)&__flash_binary_start;
  unsigned int booster_flash_length = (unsigned int)&_config_flash_start -
                                      (unsigned int)&_booster_app_flash_start;
  unsigned int config_flash_length = (unsigned int)&_global_lookup_flash_start -
                                     (unsigned int)&_config_flash_start;
  unsigned int global_lookup_flash_length = FLASH_SECTOR_SIZE;
  unsigned int global_config_flash_length = FLASH_SECTOR_SIZE;

  DPRINTF("Flash start: 0x%X, length: %u bytes\n",
          (unsigned int)&__flash_binary_start, flash_length);
  DPRINTF("Booster Flash start: 0x%X, length: %u bytes\n",
          (unsigned int)&_booster_app_flash_start, booster_flash_length);
  DPRINTF("Config Flash start: 0x%X, length: %u bytes\n",
          (unsigned int)&_config_flash_start, config_flash_length);
  DPRINTF("Global Lookup Flash start: 0x%X, length: %u bytes\n",
          (unsigned int)&_global_lookup_flash_start,
          global_lookup_flash_length);
  DPRINTF("Global Config Flash start: 0x%X, length: %u bytes\n",
          (unsigned int)&_global_config_flash_start,
          global_config_flash_length);

#endif

  // We don't want to initialized any settings in the global config
  // We only want to read the global config to get the magic number and version
  // number So we don't pass any entries to the settings_init function
  int err = gconfig_init(CURRENT_APP_UUID_KEY);

  if (err < 0) {
    DPRINTF("Settings not initialized. Jump to Booster application\n");
    // jump_to_booster_app();
  }

  DPRINTF("Start the upgrade here\n");

  // Create the temporary name of the file downloaded
  char tmp_binary_filename[256] = {0};
  snprintf(tmp_binary_filename, sizeof(tmp_binary_filename), "%s/upgrade.bin",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value);

  // We must initialize the FatFS file system
  FATFS fs;
  FRESULT res = f_mount(&fs, "", 1);
  if (res != FR_OK) {
    DPRINTF("Failed to mount FatFS: %i\n", res);
    jump_to_booster_app();
  }

  // Now the upgrade.bin file is the firmware binary, it must be copied to
  // the start of the flash
  DPRINTF("Writing firmware binary to flash: %s\n", tmp_binary_filename);
  res = storeUF2FileToFlash(
      tmp_binary_filename, (uint32_t)&__flash_binary_start,
      (unsigned int)&_config_flash_start - (uint32_t)&__flash_binary_start,
      FLASH_BLOCK_SIZE);

  DPRINTF("Launching firmware binary\n");
  jump_to_booster_app();
  DPRINTF("You should never reach this point\n");

  while (1) {
    // Do something
  }
}
