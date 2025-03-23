
#include "appmngr.h"

static sdcard_info_t sdcard_info = {false, 0, 0, false};
static app_info_t app_info = {0};
static FIL file;
static download_status_t download_status = DOWNLOAD_STATUS_IDLE;
static download_err_t download_error = DOWNLOAD_OK;
static HTTPC_REQUEST_T request = {0};
static DIR s_dir;
static bool s_dir_opened = false;
static char s_folder[256];
static download_launch_err_t launch_status = DOWNLOAD_LAUNCHAPP_IDLE;
static char launch_app_uuid[37] = {0};

// Helper function to check if a 36-character string is a valid UUID4.
// A valid UUID4 is in the canonical format
// "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx" where x is a hexadecimal digit and y
// is one of [8, 9, A, B] (case-insensitive).
static int is_valid_uuid4(const char *uuid) {
  // Check length: assume uuid is exactly 36 characters (without a null
  // terminator). Check that the hyphens are in the correct positions.
  for (int i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (uuid[i] != '-') {
        return 0;
      }
    } else {
      if (!isxdigit((unsigned char)uuid[i])) {
        return 0;
      }
    }
  }
  // Check the version: the first character of the third group (index 14) must
  // be '4'
  if (uuid[14] != '4') {
    return 0;
  }
  // Check the variant: the first character of the fourth group (index 19)
  // must be one of '8', '9', 'A', or 'B' (case-insensitive)
  char variant = uuid[19];
  if (!(variant == '8' || variant == '9' || variant == 'A' || variant == 'B' ||
        variant == 'a' || variant == 'b')) {
    return 0;
  }
  return 1;
}

// Extract the components of the URL
static int parse_url(const char *url, url_components_t *components) {
  if (!url || !components) {
    return -1;  // Invalid arguments
  }

  // Initialize the components
  memset(components, 0, sizeof(url_components_t));

  // Find the protocol separator ("://")
  const char *protocol_end = strstr(url, "://");
  if (!protocol_end) {
    return -1;  // Invalid URL format
  }

  // Extract the protocol
  size_t protocol_len = protocol_end - url;
  if (protocol_len >= sizeof(components->protocol)) {
    return -1;  // Protocol too long
  }
  strncpy(components->protocol, url, protocol_len);
  components->protocol[protocol_len] = '\0';

  // Find the host start and URI start
  const char *host_start = protocol_end + 3;  // Skip "://"
  const char *uri_start = strchr(host_start, '/');
  size_t host_len;

  if (uri_start) {
    // URI exists
    host_len = uri_start - host_start;
    if (host_len >= sizeof(components->host)) {
      return -1;  // Host too long
    }
    strncpy(components->host, host_start, host_len);
    components->host[host_len] = '\0';

    // Copy the URI
    strncpy(components->uri, uri_start, sizeof(components->uri) - 1);
  } else {
    // No URI, only host
    strncpy(components->host, host_start, sizeof(components->host) - 1);
  }

  return 0;  // Success
}

// Write the sdcard_write_file function
static int sdcard_write_file(const char *filename, const char *data,
                             size_t size) {
  FIL file;
  FRESULT res;
  UINT bytes_written;

  res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
  if (res != FR_OK) {
    DPRINTF("Error opening file %s: %i\n", filename, res);
    return -1;
  }
  // Write the data to the file
  res = f_write(&file, data, size, &bytes_written);
  if (res != FR_OK) {
    DPRINTF("Error writing to file %s: %i\n", filename, res);
    f_close(&file);
    return -1;
  }
  // Close the file
  res = f_close(&file);
  if (res != FR_OK) {
    DPRINTF("Error closing file %s: %i\n", filename, res);
    return -1;
  }
  return 0;
}

static download_err_t set_app_info(const char *json_str) {
  cJSON *root = cJSON_Parse(json_str);
  if (root == NULL) {
    DPRINTF("Error parsing JSON\n");
    return DOWNLOAD_PARSEJSON_ERROR;  // Error parsing JSON
  }

  // Retrieve JSON fields
  cJSON *uuid = cJSON_GetObjectItem(root, "uuid");
  cJSON *name = cJSON_GetObjectItem(root, "name");
  cJSON *description = cJSON_GetObjectItem(root, "description");
  cJSON *image = cJSON_GetObjectItem(root, "image");
  cJSON *tags = cJSON_GetObjectItem(root, "tags");
  cJSON *devices = cJSON_GetObjectItem(root, "devices");
  cJSON *binary = cJSON_GetObjectItem(root, "binary");
  cJSON *md5 = cJSON_GetObjectItem(root, "md5");
  cJSON *version = cJSON_GetObjectItem(root, "version");

  // For each field, confirm it's a string before copying
  if (uuid && cJSON_IsString(uuid)) {
    snprintf(app_info.uuid, sizeof(app_info.uuid), "%s", uuid->valuestring);
  }
  if (name && cJSON_IsString(name)) {
    snprintf(app_info.name, sizeof(app_info.name), "%s", name->valuestring);
  }
  if (description && cJSON_IsString(description)) {
    snprintf(app_info.description, sizeof(app_info.description), "%s",
             description->valuestring);
  }
  if (image && cJSON_IsString(image)) {
    snprintf(app_info.image, sizeof(app_info.image), "%s", image->valuestring);
  }

  // tags is expected to be an array of strings
  if (tags && cJSON_IsArray(tags)) {
    cJSON *tag = NULL;
    int i = 0;
    cJSON_ArrayForEach(tag, tags) {
      if (cJSON_IsString(tag)) {
        snprintf(app_info.tags[i], sizeof(app_info.tags[i]), "%s",
                 tag->valuestring);
        i++;
        if (i >= MAX_TAGS) break;
      }
    }
    app_info.tags_count = i;
  } else {
    app_info.tags_count = 0;  // No valid tags array
  }

  // devices is expected to be an array of strings
  if (devices && cJSON_IsArray(devices)) {
    cJSON *device = NULL;
    int i = 0;
    cJSON_ArrayForEach(device, devices) {
      if (cJSON_IsString(device)) {
        snprintf(app_info.devices[i], sizeof(app_info.devices[i]), "%s",
                 device->valuestring);
        i++;
        if (i >= MAX_DEVICES) break;
      }
    }
    app_info.devices_count = i;
  } else {
    app_info.devices_count = 0;  // No valid devices array
  }

  if (binary && cJSON_IsString(binary)) {
    snprintf(app_info.binary, sizeof(app_info.binary), "%s",
             binary->valuestring);
  }
  if (md5 && cJSON_IsString(md5)) {
    // Convert MD5 string to 16-byte binary array
    const char *md5_str = md5->valuestring;
    size_t md5_len = strlen(md5_str);
    if (md5_len == 32)  // MD5 string should be 32 hex characters
    {
      for (int i = 0; i < 16; i++) {
        sscanf(md5_str + (i * 2), "%2hhx", &app_info.md5[i]);
      }
    } else {
      DPRINTF("Invalid MD5 length in JSON: %zu\n", md5_len);
      cJSON_Delete(root);
      return DOWNLOAD_PARSEMD5_ERROR;
    }
  }
  if (version && cJSON_IsString(version)) {
    snprintf(app_info.version, sizeof(app_info.version), "%s",
             version->valuestring);
  }

  // Always free the cJSON root object to avoid memory leaks
  cJSON_Delete(root);

  // Finally, copy the JSON string into the app_info struct in the json field
  snprintf(app_info.json, sizeof(app_info.json), "%s", json_str);

  DPRINTF("App info set:\n");
  DPRINTF("UUID: %s\n", app_info.uuid);
  DPRINTF("Name: %s\n", app_info.name);
  DPRINTF("Description: %s\n", app_info.description);
  DPRINTF("Image: %s\n", app_info.image);
  DPRINTF("Tags: ");
  for (int i = 0; i < app_info.tags_count; i++) {
    DPRINTF("+- %s\n", app_info.tags[i]);
  }
  DPRINTF("Devices: ");
  for (int i = 0; i < app_info.devices_count; i++) {
    DPRINTF("+- %s\n", app_info.devices[i]);
  }
  DPRINTF("Binary: %s\n", app_info.binary);
  char app_md5_str[2 * sizeof(app_info.md5) + 1];
  for (int i = 0; i < sizeof(app_info.md5); i++) {
    sprintf(&app_md5_str[i * 2], "%02X", app_info.md5[i]);
  }
  app_md5_str[2 * sizeof(app_info.md5)] = '\0';
  DPRINTF("MD5: %s\n", app_md5_str);

  DPRINTF("Version: %s\n", app_info.version);
  DPRINTF("JSON: %s\n", app_info.json);
  DPRINTF("App info set successfully\n");
  return DOWNLOAD_OK;
}

download_err_t appmngr_save_app_info(const char *json_str) {
  // Save the app info to the SD card as a JSON file

  int err = set_app_info(json_str);
  if (err == DOWNLOAD_PARSEJSON_ERROR) {
    DPRINTF("Error setting app info: %i\n", err);
    return DOWNLOAD_PARSEJSON_ERROR;
  }
  // We have the app info in the app_info struct, now save it to a temp file
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s/tmp.json",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value);

  // Let's try to delete first the temp file if it exists
  FRESULT ferr = f_unlink(filename);
  if (ferr == FR_NO_FILE) {
    // If the file doesn't exist, we ignore the error
    DPRINTF("Error deleting file. %s does not exist. Continuing...\n",
            filename);
  }

  // Now write the app info to the temp file
  DPRINTF("Saving app info to file: %s\n", filename);
  DPRINTF("JSON: %s\n", app_info.json);
  ferr = sdcard_write_file(filename, app_info.json, strlen(app_info.json));
  if (ferr != FR_OK) {
    DPRINTF("Error saving app info to file: %i\n", ferr);
  } else {
    DPRINTF("App info saved to file: %s\n", filename);
  }
  return DOWNLOAD_OK;
}

app_info_t *appmngr_get_app_info() { return &app_info; }

sdcard_info_t *appmngr_get_sdcard_info() { return &sdcard_info; }

int appmngr_delete_app_info() {
  // Delete the app info file from the SD card
  // The filename must be the UUID of the app with the .json extension
  char filename[256] = {0};
  snprintf(filename, sizeof(filename), "%s/%s.json",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value,
           app_info.uuid);
  DPRINTF("Deleting app info file: %s\n", filename);
  int err = f_unlink(filename);
  if (err != FR_OK) {
    DPRINTF("Error deleting app info file: %i\n", err);
  } else {
    DPRINTF("App info file deleted: %s\n", filename);
  }
  return err;
}

// Save body to file
err_t http_client_receive_file_fn(__unused void *arg,
                                  __unused struct altcp_pcb *conn,
                                  struct pbuf *p, err_t err) {
  // Check for null input or errors
  if (p == NULL) {
    DPRINTF("End of data or connection closed by the server.\n");
    download_status = DOWNLOAD_STATUS_COMPLETED;
    return ERR_OK;  // Signal the connection closure
  }

  if (err != ERR_OK) {
    DPRINTF("Error receiving file: %i\n", err);
    download_status = DOWNLOAD_STATUS_FAILED;
    return ERR_VAL;  // Invalid input or error occurred
  }

  // Allocate buffer to hold pbuf content
  char *buffc = malloc(p->tot_len);
  if (buffc == NULL) {
    DPRINTF("Error allocating memory\n");
    download_status = DOWNLOAD_STATUS_FAILED;
    return ERR_MEM;  // Memory allocation failed
  }

  // Use pbuf_copy_partial to copy the pbuf content to the buffer
  pbuf_copy_partial(p, buffc, p->tot_len, 0);

  // Write the buffer to the file. File descriptor is 'file'
  FRESULT res;
  UINT bytes_written;
  res = f_write(&file, buffc, p->tot_len, &bytes_written);

  // Free the allocated memory
  free(buffc);

  // Check for file write errors
  if (res != FR_OK || bytes_written != p->tot_len) {
    DPRINTF("Error writing to file: %i\n", res);
    download_status = DOWNLOAD_STATUS_FAILED;
    return ERR_ABRT;  // Abort on failure
  }

  // Acknowledge that we received the data
#if BOOSTER_DOWNLOAD_HTTPS == 1
  altcp_recved(conn, p->tot_len);
#else
  tcp_recved(conn, p->tot_len);
#endif

  // Free the pbuf
  pbuf_free(p);

  download_status = DOWNLOAD_STATUS_IN_PROGRESS;
  return ERR_OK;
}

// Function to parse headers and check Content-Length
err_t http_client_header_check_size_fn(__unused httpc_state_t *connection,
                                       __unused void *arg, struct pbuf *hdr,
                                       u16_t hdr_len,
                                       __unused u32_t content_len) {
  download_status = DOWNLOAD_STATUS_FAILED;
  const char *content_length_label = "Content-Length:";
  u16_t offset = 0;
  char *header_data = malloc(hdr_len + 1);

  if (header_data == NULL) {
    return ERR_MEM;  // Memory allocation failed
  }

  // Copy header data into a buffer for parsing
  pbuf_copy_partial(hdr, header_data, hdr_len, 0);
  header_data[hdr_len] = '\0';  // Null-terminate the string

  // Find the Content-Length header
  char *content_length_start = strstr(header_data, content_length_label);
  if (content_length_start != NULL) {
    content_length_start +=
        strlen(content_length_label);  // Move past "Content-Length:"

    // Skip leading spaces
    while (*content_length_start == ' ') {
      content_length_start++;
    }

    // Convert the Content-Length value to an integer
    size_t content_length = strtoul(content_length_start, NULL, 10);

    // Check if the size exceeds the maximum allowed
    if (content_length > MAXIMUM_APP_UF2_SIZE) {
      free(header_data);  // Free allocated memory
      return ERR_VAL;     // Return error for size exceeding limit
    }
  }

  free(header_data);  // Free allocated memory
  download_status = DOWNLOAD_STATUS_IN_PROGRESS;
  return ERR_OK;  // Header check passed
}

static void http_client_result_complete_fn(void *arg,
                                           httpc_result_t httpc_result,
                                           u32_t rx_content_len, u32_t srv_res,
                                           err_t err) {
  HTTPC_REQUEST_T *req = (HTTPC_REQUEST_T *)arg;
  DPRINTF("Requet complete: result %d len %u server_response %u err %d\n",
          httpc_result, rx_content_len, srv_res, err);
  req->complete = true;
  if (err == ERR_OK) {
    download_status = DOWNLOAD_STATUS_COMPLETED;
    download_error = DOWNLOAD_OK;
  } else {
    download_status = DOWNLOAD_STATUS_FAILED;
    download_error = DOWNLOAD_HTTP_ERROR;
  }
  if (srv_res != 200) {
    download_status = DOWNLOAD_STATUS_FAILED;
    download_error = DOWNLOAD_HTTP_ERROR;
  }
}

static void get_tmp_filename_path(char filename[256]) {
  snprintf(filename, 256, "%s/tmp.download",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value);
}

static bool find_next_json_file(char *json, size_t max_len) {
  // We'll read directory entries until we find a valid .json file
  // or until we exhaust all entries
  FRESULT res;
  FILINFO fno;
  while (true) {
    res = f_readdir(&s_dir, &fno);
    if (res != FR_OK || fno.fname[0] == '\0') {
      // Error or no more files in this directory
      return false;
    }

    // Skip "apps.json"
    if (strcmp(fno.fname, "apps.json") == 0) {
      continue;
    }

    // Check extension
    char *ext = strrchr(fno.fname, '.');
    if (!ext || strcasecmp(ext, ".json") != 0) {
      continue;  // Not a .json file
    }

    // Check if the base name (excluding extension) is 36 characters
    char file_base[64];
    strncpy(file_base, fno.fname, sizeof(file_base));
    file_base[sizeof(file_base) - 1] = '\0';  // Ensure null-termination

    char *dot = strrchr(file_base, '.');
    if (dot) {
      *dot = '\0';  // Remove the .json extension
    }

    // If length is not 36, skip (not a valid UUID)
    if (strlen(file_base) != 36) {
      continue;
    }

    // We have a .json file, let's open and read it
    FIL fil;
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", s_folder, fno.fname);
    DPRINTF("Found JSON file with valid UUID name: %s\n", filepath);

    res = f_open(&fil, filepath, FA_READ);
    if (res != FR_OK) {
      DPRINTF("Error opening file %s: %d\n", filepath, res);
      continue;  // Skip and read the next
    }

    // Read file contents into 'json' buffer
    // Assume json has space for at least max_len bytes
    UINT bytes_read = 0;
    res = f_read(&fil, json, max_len - 1, &bytes_read);
    f_close(&fil);

    if (res != FR_OK) {
      DPRINTF("Error reading file %s: %d\n", filepath, res);
      continue;  // skip this file
    }

    // Null-terminate
    json[bytes_read] = '\0';

    DPRINTF("JSON file contents: %s\n", json);

    // Found a valid file and read it successfully
    return true;
  }
}

// Go to the first UUID.json file in the apps folder
// Return false if no file is found.
// Return true if a file is found and the json parameter is filled with the
// contents of the file.
bool appmngr_ffirst(char *json) {
  // If we already have a directory open, close it first
  if (s_dir_opened) {
    f_closedir(&s_dir);
    s_dir_opened = false;
  }

  // Get the apps folder path
  snprintf(s_folder, sizeof(s_folder), "%s",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value);

  // Open the directory
  FRESULT res = f_opendir(&s_dir, s_folder);
  if (res != FR_OK) {
    DPRINTF("appmngr_ffirst: Error opening folder: %s (res=%d)\n", s_folder,
            res);
    return false;
  }
  s_dir_opened = true;

  // Find the first JSON file
  if (!find_next_json_file(json, MAXIMUM_APP_INFO_SIZE)) {
    // No valid .json found or error reading
    f_closedir(&s_dir);
    s_dir_opened = false;
    return false;
  }

  return true;
}

// Go to the next UUID.json file in the apps folder
// Return false if no more files are found.
// Return true if a file is found and the json parameter is filled with the
// contents of the file.
bool appmngr_fnext(char *json) {
  // Must have done ffirst first
  if (!s_dir_opened) {
    DPRINTF("appmngr_fnext called but directory not opened.\n");
    return false;
  }

  // Find the next JSON file
  if (!find_next_json_file(json, MAXIMUM_APP_INFO_SIZE)) {
    // No more .json files
    f_closedir(&s_dir);
    s_dir_opened = false;
    return false;
  }

  return true;
}

download_status_t appmngr_get_download_status() { return download_status; }

void appmngr_download_status(download_status_t status) {
  download_status = status;
}

download_err_t appmngr_get_download_error() { return download_error; }

void appmngr_download_error(download_err_t err) { download_error = err; }

const char *appmngr_get_download_error_str() {
  switch (download_error) {
    case DOWNLOAD_OK:
      return "No error";
    case DOWNLOAD_BASE64_ERROR:
      return "Error decoding base64";
    case DOWNLOAD_PARSEJSON_ERROR:
      return "Error parsing JSON";
    case DOWNLOAD_PARSEMD5_ERROR:
      return "Error parsing MD5";
    case DOWNLOAD_CANNOTOPENFILE_ERROR:
      return "Cannot open file";
    case DOWNLOAD_CANNOTCLOSEFILE_ERROR:
      return "Cannot close file";
    case DOWNLOAD_FORCEDABORT_ERROR:
      return "Download aborted";
    case DOWNLOAD_CANNOTSTARTDOWNLOAD_ERROR:
      return "Cannot start download";
    case DOWNLOAD_CANNOTREADFILE_ERROR:
      return "Cannot read file";
    case DOWNLOAD_CANNOTPARSEURL_ERROR:
      return "Cannot parse URL";
    case DOWNLOAD_MD5MISMATCH_ERROR:
      return "MD5 mismatch";
    case DOWNLOAD_CANNOTRENAMEFILE_ERROR:
      return "Cannot rename file";
    case DOWNLOAD_CANNOTCREATE_CONFIG:
      return "Cannot create configuration";
    case DOWNLOAD_CANNOTDELETECONFIGSECTOR_ERROR:
      return "Cannot delete configuration sector";
    case DOWNLOAD_HTTP_ERROR:
      return "HTTP error";
    default:
      return "Unknown error";
  }
}

static int8_t appmngr_delete_config_sector(uint8_t sector) {
  // Delete the sector in the flash memory
  uint32_t flash_start = (uint32_t)&_config_flash_start;
  uint32_t config_flash_length = (unsigned int)&_global_lookup_flash_start -
                                 (unsigned int)&_config_flash_start;

  // Calculate the number of 4KB sectors
  uint32_t num_sectors = config_flash_length / FLASH_SECTOR_SIZE;

  // Print the number of sectors
  DPRINTF("Config Flash start: 0x%X, length: %u bytes, sectors: %u\n",
          flash_start, config_flash_length, num_sectors);

  if (sector < 0 || sector >= num_sectors) {
    DPRINTF("Invalid sector %d\n", sector);
    return -1;  // Error
  }

  // Erase the sector
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_start - XIP_BASE, FLASH_SECTOR_SIZE);  // 4 Kbytes
  restore_interrupts(ints);

  return 0;  // Success
}

/**
 * Print the apps lookup table to stdout.
 *
 * @param table  Pointer to the lookup table data.
 * @param length Total number of bytes in the lookup table.
 */
void appmngr_print_apps_lookup_table(uint8_t *table, uint16_t length) {
  if (length % LOOKUP_ENTRY_SIZE != 0) {
    DPRINTF("Warning: table length (%u) is not a multiple of %d bytes.\n",
            length, LOOKUP_ENTRY_SIZE);
  }

  uint16_t num_entries = length / LOOKUP_ENTRY_SIZE;
  DPRINTF("Printing Apps Lookup Table (%u entries):\n", num_entries);

  for (uint16_t i = 0; i < num_entries; i++) {
    uint8_t *entry = table + i * LOOKUP_ENTRY_SIZE;

    // Copy the first 36 bytes into a temporary buffer and null-terminate it.
    char uuid[37];  // 36 chars + 1 for the null terminator.
    memcpy(uuid, entry, 36);
    uuid[36] = '\0';

    // The next 2 bytes represent the sector as a 16-bit integer.
    // Assuming little-endian order.
    uint16_t sector = entry[36] | (entry[37] << 8);

    // Print the entry.
    DPRINTF("Entry %u: UUID = %s, Sector = %u\n", i, uuid, sector);
  }
}

void appmngr_load_apps_lookup_table(uint8_t *table, uint16_t *length) {
  // Calculate flash region start and total lookup region length
  uint32_t flash_start = (uint32_t)&_global_lookup_flash_start;
  uint32_t lookup_flash_length = (uint32_t)&_global_config_flash_start -
                                 (uint32_t)&_global_lookup_flash_start;

  // Determine the maximum number of entries stored in flash
  uint32_t num_entries = lookup_flash_length / LOOKUP_ENTRY_SIZE;
  DPRINTF("Lookup Flash start: 0x%X, length: %u bytes, entries: %u\n",
          flash_start, lookup_flash_length, num_entries);

  uint16_t total_bytes_read = 0;
  uint8_t *dest = table;

  // Read each entry from flash memory
  for (uint32_t i = 0; i < num_entries; i++) {
    uint32_t entry_addr = flash_start + (i * LOOKUP_ENTRY_SIZE);
    uint8_t *entry_ptr = (uint8_t *)entry_addr;

    // Check if this entry is valid by verifying that the first byte of the UUID
    // is nonzero. If it's zero, we assume there's no more valid data.
    if (entry_ptr[0] == 0) {
      break;
    }

    if (!is_valid_uuid4((const char *)entry_ptr)) {
      // Invalid UUID format: assume no further valid data.
      break;
    }

    // Copy the 38-byte entry into the provided table buffer.
    memcpy(dest, entry_ptr, LOOKUP_ENTRY_SIZE);
    dest += LOOKUP_ENTRY_SIZE;
    total_bytes_read += LOOKUP_ENTRY_SIZE;
  }

  // Return the total number of bytes read in the lookup table via the length
  // pointer.
  *length = total_bytes_read;
}

/**
 * Searches the lookup table for an entry matching the given UUID.
 *
 * @param uuid   A pointer to the 36-character UUID string to search for.
 * @param table  A pointer to the lookup table data in RAM.
 * @param length Total number of bytes stored in the lookup table.
 *
 * @return The page number (as a 16-bit integer) if found, or -1 if not found.
 */
static int16_t appmngr_get_page_number_for_uuid(const char *uuid,
                                                uint8_t *table,
                                                uint16_t length) {
  // Determine the number of entries in the table.
  uint16_t num_entries = length / LOOKUP_ENTRY_SIZE;

  for (uint16_t i = 0; i < num_entries; i++) {
    uint8_t *entry = table + (i * LOOKUP_ENTRY_SIZE);

    // If the first byte is zero, assume no further valid entries.
    if (entry[0] == 0) {
      break;
    }

    if (!is_valid_uuid4((const char *)entry)) {
      // Invalid UUID format: assume no further valid data.
      break;
    }

    // Compare the first 36 bytes with the given UUID.
    if (memcmp(entry, uuid, 36) == 0) {
      // Extract the page number from the two bytes following the UUID.
      // Assuming little-endian format.
      uint16_t page = entry[36] | (entry[37] << 8);
      return page;
    }
  }

  // UUID not found in the lookup table.
  return -1;
}

/**
 * Append or overwrite an entry in the lookup table.
 *
 * This function searches the lookup table for an existing entry with the given
 * UUID.
 * - If found, it overwrites the sector value with the new value.
 * - If not found, it searches for an empty slot (an entry whose first byte is
 * 0) and appends the new entry.
 *
 * @param uuid         A pointer to a 36-character UUID string.
 * @param sector       The 16-bit sector value to store.
 * @param table        Pointer to the lookup table data in memory.
 * @param table_length Total length in bytes of the lookup table.
 *
 * @return 0 on success, or -1 if the table is full or the UUID is invalid.
 */
static int appmngr_update_lookup_table(const char *uuid, uint16_t sector,
                                       uint8_t *table, uint16_t *table_length) {
  // Validate that the UUID is exactly 36 characters
  if (strlen(uuid) != 36) {
    DPRINTF("Invalid UUID length: %u. Expected 36 characters.\n",
            (unsigned)strlen(uuid));
    return -1;
  }

  if (!is_valid_uuid4(uuid)) {
    DPRINTF("Invalid UUID format: %s\n", uuid);
    return -2;
  }

  uint16_t num_entries = *table_length / LOOKUP_ENTRY_SIZE;
  int indexFound = -1;
  int indexEmpty = num_entries;

  // Iterate through each entry in the table.
  for (uint16_t i = 0; i < num_entries; i++) {
    uint8_t *entry = table + (i * LOOKUP_ENTRY_SIZE);
    // If the entry is empty (first byte is 0), record the index if we haven't
    // found an empty slot yet.
    if (entry[0] == 0) {
      if (indexEmpty == -1) {
        indexEmpty = i;
      }
    } else {
      // Otherwise, compare the stored UUID (first 36 bytes) with the provided
      // UUID.
      if (memcmp(entry, uuid, 36) == 0) {
        indexFound = i;
        break;
      }
    }
  }

  if (indexFound != -1) {
    // Entry found: overwrite the sector value.
    uint8_t *entry = table + (indexFound * LOOKUP_ENTRY_SIZE);
    entry[36] = sector & 0xFF;
    entry[37] = (sector >> 8) & 0xFF;
    DPRINTF("Updated entry at index %d with sector %u.\n", indexFound, sector);
    return 0;
  } else if (indexEmpty != -1) {
    // No matching entry found, but there is an empty slot: append new entry.
    uint8_t *entry = table + (indexEmpty * LOOKUP_ENTRY_SIZE);
    memcpy(entry, uuid, 36);  // Copy the UUID exactly (36 bytes)
    entry[36] = sector & 0xFF;
    entry[37] = (sector >> 8) & 0xFF;
    *table_length += LOOKUP_ENTRY_SIZE;
    DPRINTF("Appended new entry at index %d: UUID = %.*s, sector = %u.\n",
            indexEmpty, 36, uuid, sector);
    return 0;
  } else {
    // No free slot available in the lookup table.
    DPRINTF("Lookup table is full; cannot append new entry.\n");
    return -1;
  }
}

/**
 * Delete an entry from the lookup table given its UUID.
 *
 * Each entry in the lookup table is 38 bytes:
 * - The first 36 bytes store the UUID (exactly as a 36-character string)
 * - The next 2 bytes store the sector (page number) in little-endian format.
 *
 * The table is stored as a contiguous block of memory. Valid entries are
 * assumed to be contiguous from the beginning of the table, and an entry is
 * considered empty if its first byte is zero.
 *
 * @param uuid         A pointer to the 36-character UUID string to delete.
 * @param table        Pointer to the lookup table data in memory.
 * @param table_length Total length (in bytes) of the lookup table.
 *
 * @return 0 on success, or -1 if the UUID is not found or if the UUID length is
 * invalid.
 */
static int appmngr_delete_lookup_entry(const char *uuid, uint8_t *table,
                                       uint16_t *table_length) {
  // Validate that the UUID is exactly 36 characters.
  if (strlen(uuid) != 36) {
    DPRINTF("Invalid UUID length: %u. Expected 36 characters.\n",
            (unsigned)strlen(uuid));
    return -1;
  }
  if (!is_valid_uuid4(uuid)) {
    DPRINTF("Invalid UUID format: %s\n", uuid);
    return -2;
  }

  // Calculate the total number of entries in the table.
  uint16_t num_entries = *table_length / LOOKUP_ENTRY_SIZE;
  int found_index = -1;

  // Determine how many valid (non-empty) entries are in the table.
  uint16_t valid_entries = 0;
  for (uint16_t i = 0; i < num_entries; i++) {
    uint8_t *entry = table + i * LOOKUP_ENTRY_SIZE;
    if (entry[0] == 0) {
      // Assume the table ends when an empty entry is encountered.
      break;
    }
    if (!is_valid_uuid4((const char *)entry)) {
      // Invalid UUID format: assume no further valid data.
      break;
    }
    valid_entries++;
  }

  // Search for the entry with the given UUID among the valid entries.
  for (uint16_t i = 0; i < valid_entries; i++) {
    uint8_t *entry = table + i * LOOKUP_ENTRY_SIZE;
    if (memcmp(entry, uuid, 36) == 0) {
      found_index = i;
      break;
    }
  }

  if (found_index == -1) {
    // UUID not found in the lookup table.
    DPRINTF("UUID not found in lookup table.\n");
    return -1;
  }

  // If the found entry is not the last one, shift subsequent entries upward to
  // compact the table.
  if (found_index < valid_entries - 1) {
    memmove(table + found_index * LOOKUP_ENTRY_SIZE,  // Destination: position
                                                      // of the deleted entry
            table + (found_index + 1) *
                        LOOKUP_ENTRY_SIZE,  // Source: next entry in the table
            (valid_entries - found_index - 1) *
                LOOKUP_ENTRY_SIZE  // Number of bytes to move
    );
  }

  // Clear the now-last valid entry.
  uint8_t *last_entry = table + ((valid_entries - 1) * LOOKUP_ENTRY_SIZE);
  memset(last_entry, 0, LOOKUP_ENTRY_SIZE);

  *table_length -= LOOKUP_ENTRY_SIZE;

  DPRINTF("Deleted entry for UUID: %s\n", uuid);
  return 0;
}

/**
 * Finds the first empty configuration sector from the lookup table provided.
 *
 * @param table  Pointer to the lookup table data (in memory).
 * @param length Pointer to the total length (in bytes) of the lookup table.
 *
 * @return The lowest available sector number (16-bit value) if found,
 *         or 0 if no free sector is available.
 */
static int16_t appmngr_find_first_empty_config_sector(uint8_t *table,
                                                      uint16_t *length) {
  // Calculate the total number of entries in the provided table.
  uint16_t total_entries = *length / LOOKUP_ENTRY_SIZE;
  uint16_t valid_entries = 0;

  // Count valid entries until an empty entry is encountered.
  for (uint16_t i = 0; i < total_entries; i++) {
    uint8_t *entry = table + (i * LOOKUP_ENTRY_SIZE);
    if (entry[0] == 0) {  // An entry with first byte 0 is considered empty.
      break;
    }
    if (!is_valid_uuid4((const char *)entry)) {
      // Invalid UUID format: assume no further valid data.
      break;
    }
    valid_entries++;
  }

  // Iterate candidate sector numbers from 0 to total_entries
  for (uint16_t candidate = 0; candidate <= total_entries; candidate++) {
    bool used = false;
    for (uint16_t i = 0; i < valid_entries; i++) {
      uint8_t *entry = table + (i * LOOKUP_ENTRY_SIZE);
      // Extract the sector number stored in the entry (little-endian)
      uint16_t sector = entry[36] | (((uint16_t)entry[37]) << 8);
      if (sector == candidate) {
        used = true;
        break;
      }
    }
    if (!used) {
      return candidate;
    }
  }

  // If all candidate sectors appear to be used (unlikely), return 0.
  return 0;
}

static int8_t appmngr_persist_app_lookup_table(const uint8_t *table,
                                               uint16_t table_length) {
  // Persist the table in the flash memory
  uint32_t flash_start = (uint32_t)&_global_lookup_flash_start;
  uint32_t config_flash_length = (unsigned int)&_global_config_flash_start -
                                 (unsigned int)&_global_lookup_flash_start;

  // Calculate the number of 4KB sectors
  uint32_t num_sectors = config_flash_length / FLASH_SECTOR_SIZE;

  // Print the number of sectors
  DPRINTF(
      "Persist App Lookup Table at Flash start: 0x%X, length: %u bytes, "
      "sectors: %u\n",
      flash_start, config_flash_length, num_sectors);

  // Erase the sector
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_start - XIP_BASE, FLASH_SECTOR_SIZE);  // 4 Kbytes

  // Round the table_length to the higher 256 bytes chunk value
  uint16_t roundup_table_length =
      ((table_length + FLASH_PAGE_SIZE) / FLASH_PAGE_SIZE) * FLASH_PAGE_SIZE;

  // Transfer app lookup table to FLASH
  flash_range_program(flash_start - XIP_BASE, table, roundup_table_length);

  restore_interrupts(ints);

  DPRINTF("Persisted.\n");

  return 0;
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

static FRESULT __not_in_flash_func(storeUF2FileToFlash)(const char *filename,
                                                        uint32_t flashAddress,
                                                        uint32_t flashSize,
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

int8_t appmngr_erase_app_lookup_table() {
  // Delete whole app lookup table
  uint32_t flash_start = (uint32_t)&_global_lookup_flash_start;
  uint32_t flash_length = (unsigned int)&_global_config_flash_start -
                          (unsigned int)&_global_lookup_flash_start;

  // Calculate the number of 4KB sectors
  uint32_t num_sectors = flash_length / FLASH_SECTOR_SIZE;

  // Print the number of sectors
  DPRINTF("Config Flash start: 0x%X, length: %u bytes, sectors: %u\n",
          flash_start, flash_length, num_sectors);

  // Erase the sector
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_start - XIP_BASE, flash_length);  // 4 Kbytes
  restore_interrupts(ints);

  return 0;  // Success
}

download_delete_err_t appmngr_delete_app(const char *uuid) {
  // Delete the app with the uuid given.
  DPRINTF("Deleting app with UUID: %s\n", uuid);

  // First, check that the uuid is a valid uuid
  if (strlen(uuid) != 36) {
    DPRINTF("Invalid UUID: %s\n", uuid);
    return DOWNLOAD_DELETEAPP_NOTUUID_ERROR;
  }
  // Now, check that the sd card is ready
  if (!sdcard_info.ready) {
    DPRINTF("SD card not ready\n");
    return DOWNLOAD_DELETEAPP_SDCARDNOTREADY_ERROR;
  }

  // Get the filename of the app info json
  char json_filename[256] = {0};
  snprintf(json_filename, sizeof(json_filename), "%s/%s.json",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value,
           uuid);

  // Get the filename of the app binary in uf2 format
  char binary_filename[256] = {0};
  snprintf(binary_filename, sizeof(binary_filename), "%s/%s.uf2",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value,
           uuid);

  DPRINTF("Deleting files %s and %s\n", json_filename, binary_filename);
  // Try to delete the files if they exist
  f_unlink(json_filename);
  f_unlink(binary_filename);

  // Now delete the entry in the app lookup table
  uint8_t *table = malloc(FLASH_SECTOR_SIZE);
  if (table == NULL) {
    return DOWNLOAD_CANNOTCREATE_CONFIG;
  }
  uint16_t table_length = 0;
  // Read the table from flash memory
  appmngr_load_apps_lookup_table(table, &table_length);

  // Delete and update entry in the table
  int res = appmngr_delete_lookup_entry(uuid, table, &table_length);

  // Persist the table if ok
  if (res == -1) {
    DPRINTF("Something went wrong updating the app lookup table.\n");
  } else {
    res = appmngr_persist_app_lookup_table(table, table_length);
    if (res == -1) {
      DPRINTF("There was something wrong persisting the app lookup table.\n");
    } else {
      DPRINTF("App lookup table pesisted!\n");
    }
  }
  free(table);
  DPRINTF("App deleted from the app lookup table\n");

  DPRINTF("App deleted\n");
  return DOWNLOAD_DELETEAPP_OK;
}

download_launch_err_t appmngr_get_launch_status(void) { return launch_status; }

void appmngr_set_launch_status(download_launch_err_t status) {
  launch_status = status;
}

void appmngr_schedule_launch_app(const char *uuid) {
  if (uuid) {
    launch_status = DOWNLOAD_LAUNCHAPP_SCHEDULED;
    strncpy(launch_app_uuid, uuid, sizeof(launch_app_uuid));
  }
}

download_launch_err_t appmngr_launch_app() {
  // Launch the app with the uuid given.
  DPRINTF("Launch app with UUID: %s\n", launch_app_uuid);

  // First, check that the uuid is a valid uuid
  if (!is_valid_uuid4(launch_app_uuid)) {
    DPRINTF("Invalid UUID: %s\n", launch_app_uuid);
    return DOWNLOAD_LAUNCHAPP_NOTUUID_ERROR;
  }

  // Now, check that the sd card is ready
  if (!sdcard_info.ready) {
    DPRINTF("SD card not ready\n");
    return DOWNLOAD_LAUNCHAPP_SDCARDNOTREADY_ERROR;
  }

  // Get the filename of the app info json
  char json_filename[256] = {0};
  snprintf(json_filename, sizeof(json_filename), "%s/%s.json",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value,
           launch_app_uuid);

  // Get the filename of the app binary in uf2 format
  char binary_filename[256] = {0};
  snprintf(binary_filename, sizeof(binary_filename), "%s/%s.uf2",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value,
           launch_app_uuid);

  // We have a magic UUIDv4 for the development app
  // the magic UUIDv4 is 44444444-4444-4444-8444-444444444444
  // if the UUID is the magic UUID, we don't store the UF2 file to the flash
  // memory.
  //
  // This means that after launching the app with the magic UUID, the
  // developer must manually copy the UF2 file to the flash memory or
  // launch from a development environment.

  if (strcmp(launch_app_uuid, "44444444-4444-4444-8444-444444444444") != 0) {
    // Copy the app binary to the flash memory
    DPRINTF("Copying app binary to flash memory\n");
    int res = storeUF2FileToFlash(
        binary_filename, (uint32_t)&_storage_flash_start,
        (uint32_t)&__flash_binary_start - (uint32_t)&_storage_flash_start,
        FLASH_BLOCK_SIZE);
  } else {
    DPRINTF("Development app launched\n");
  }

  // After copying the executable from the SD card to the flash memory, we need
  // to update the boot feature to the UUID of the app we want to launch
  settings_put_string(gconfig_getContext(), PARAM_BOOT_FEATURE,
                      launch_app_uuid);
  settings_save(gconfig_getContext(), true);

  DPRINTF("App launched\n");

  // After returning from this function, the system should reboot and the app
  // will be launched
  return DOWNLOAD_LAUNCHAPP_OK;
}

download_err_t appmngr_start_download_app() {
  // Download the app binary from the URL in the app_info struct
  // The binary is saved to the SD card in the apps folder
  // The filename must be the UUID of the app with the .bin extension
  // The binary is downloaded using the HTTP client
  // The MD5 hash is checked after the download
  // The binary is saved to the SD card
  // The MD5 hash is checked again after saving
  // If the MD5 hash is correct, the app info is saved to the SD card
  // If the MD5 hash is incorrect, the binary is deleted from the SD card
  // The app info is not saved
  // The function returns 0 on success, -1 on error

  // Open the file for writing to the folder of the apps to the tmp.download
  // file
  char filename[256] = {0};
  get_tmp_filename_path(filename);
  DPRINTF("Downloading app binary to file: %s\n", filename);
  FRESULT res;

  // Close any previously open handle
  f_close(&file);

  // Clear read-only attribute if necessary
  f_chmod(filename, 0, AM_RDO);

  // Let's try to remove the file if it exists
  res = f_unlink(filename);
  if (res == FR_NO_FILE) {
    // Ignore the error if the file doesn't exist
    DPRINTF("Error deleting file. %s does not exists: %i. Continuing.\n",
            filename, res);
  }

  // Open file for writing or create if it doesn't exist
  res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
  if (res == FR_LOCKED) {
    DPRINTF("File is locked. Attempting to resolve...\n");

    // Try to remove the file and create it again
    res = f_unlink(filename);
    if (res == FR_OK || res == FR_NO_FILE) {
      res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
    }
  }

  if (res != FR_OK) {
    DPRINTF("Error opening file %s: %i\n", filename, res);
    return DOWNLOAD_CANNOTOPENFILE_ERROR;
  }

  download_status = DOWNLOAD_STATUS_STARTED;
  download_error = DOWNLOAD_OK;
  // Get the components of a url
  url_components_t components;
  if (parse_url(app_info.binary, &components) != 0) {
    DPRINTF("Error parsing URL\n");
    return DOWNLOAD_CANNOTPARSEURL_ERROR;
  }
  // request.hostname = "tosemulator.sidecartridge.com";
  // request.url = "/sidecartridge-tos-emutos-192k-v2.1.0.uf2";
  request.url = components.uri;
  request.hostname = components.host;
  DPRINTF("HOST: %s. URI: %s\n", components.host, components.uri);
  request.headers_fn = http_client_header_check_size_fn;
  request.recv_fn = http_client_receive_file_fn;
  request.result_fn = http_client_result_complete_fn;
  DPRINTF("Downloading app binary: %s\n", request.url);
#if BOOSTER_DOWNLOAD_HTTPS == 1
  request.tls_config = altcp_tls_create_config_client(NULL, 0);  // https
  DPRINTF("Download with HTTPS\n");
#else
  DPRINTF("Download with HTTP\n");
#endif
  int result = http_client_request_async(cyw43_arch_async_context(), &request);
  if (result != 0) {
    DPRINTF("Error initializing the download app binary: %i\n", result);
    res = f_close(&file);
    if (res != FR_OK) {
      DPRINTF("Error closing file %s: %i\n", filename, res);
    }
    return DOWNLOAD_CANNOTSTARTDOWNLOAD_ERROR;
  }
  return DOWNLOAD_OK;
}

download_poll_t appmngr_poll_download_app() {
  if (!request.complete) {
    async_context_poll(cyw43_arch_async_context());
    async_context_wait_for_work_ms(cyw43_arch_async_context(), 100);
    return DOWNLOAD_POLL_CONTINUE;
  }
  return DOWNLOAD_POLL_COMPLETED;
}

static download_err_t calculate_md5_of_tmp_file(MD5Context *md5_ctx) {
  FRESULT res;
  FIL md5_file;
  char filename[256] = {0};
  get_tmp_filename_path(filename);

  res = f_open(&md5_file, filename, FA_READ);
  if (res != FR_OK) {
    DPRINTF("Error opening tmp file for MD5 calculation: %i\n", res);
    return DOWNLOAD_CANNOTOPENFILE_ERROR;
  }

  md5Init(md5_ctx);
  char *buffer = malloc(4096);
  if (buffer == NULL) {
    DPRINTF("Error allocating buffer for MD5 calculation\n");
    f_close(&md5_file);
    return DOWNLOAD_CANNOTCREATE_CONFIG;
  }
  memset(buffer, 0, 4096);
  UINT bytes_read;

  do {
    res = f_read(&md5_file, buffer, sizeof(buffer), &bytes_read);
    if (res != FR_OK) {
      DPRINTF("Error reading file %s for MD5 calculation: %i\n", filename, res);
      f_close(&md5_file);
      free(buffer);
      return DOWNLOAD_CANNOTREADFILE_ERROR;
    }
    md5Update(md5_ctx, buffer, bytes_read);
  } while (bytes_read > 0);

  md5Finalize(md5_ctx);
  f_close(&md5_file);
  free(buffer);
  DPRINTF("MD5 hash calculated\n");
  return DOWNLOAD_OK;
}

download_err_t appmngr_finish_download_app() {
  // Close the file
  int res = f_close(&file);
  if (res != FR_OK) {
    DPRINTF("Error closing tmp file %s: %i\n", res);
    return DOWNLOAD_CANNOTCLOSEFILE_ERROR;
  }
  DPRINTF("Downloaded.\n");

#if BOOSTER_DOWNLOAD_HTTPS == 1
  altcp_tls_free_config(request.tls_config);
#endif

  if (download_status != DOWNLOAD_STATUS_COMPLETED) {
    DPRINTF("Error downloading app binary: %i\n", download_status);
    return DOWNLOAD_FORCEDABORT_ERROR;
  }
  DPRINTF("App binary downloaded\n");

  // Debugging lines of strange compiling errors in Linux
  // Remove when figure out what is going on
  char app_md5_str[2 * sizeof(app_info.md5) + 1];
  for (int i = 0; i < sizeof(app_info.md5); i++) {
    sprintf(&app_md5_str[i * 2], "%02X", app_info.md5[i]);
  }
  app_md5_str[2 * sizeof(app_info.md5)] = '\0';
  DPRINTF("MD5 hash of app info: %s\n", app_md5_str);

  MD5Context md5_ctx;
  download_err_t md5_ret = calculate_md5_of_tmp_file(&md5_ctx);
  if (md5_ret != DOWNLOAD_OK) {
    DPRINTF("Error calculating MD5 of tmp file: %d\n", md5_ret);
    return md5_ret;
  }

  // Debugging lines of strange compiling errors in Linux
  // Remove when figure out what is going on
  memset(app_md5_str, 0, sizeof(app_md5_str));
  for (int i = 0; i < sizeof(app_info.md5); i++) {
    sprintf(&app_md5_str[i * 2], "%02X", app_info.md5[i]);
  }
  app_md5_str[2 * sizeof(app_info.md5)] = '\0';
  DPRINTF("MD5 hash of app info: %s\n", app_md5_str);

  memcpy(app_info.file_md5_digest, md5_ctx.digest,
         sizeof(app_info.file_md5_digest));
  char file_md5_str[2 * sizeof(app_info.file_md5_digest) + 1];

  for (int i = 0; i < sizeof(app_info.file_md5_digest); i++) {
    sprintf(&file_md5_str[i * 2], "%02X", app_info.file_md5_digest[i]);
  }
  file_md5_str[2 * sizeof(app_info.file_md5_digest)] = '\0';
  DPRINTF("MD5 hash of downloaded file: %s\n", file_md5_str);

  memset(app_md5_str, 0, sizeof(app_md5_str));
  for (int i = 0; i < sizeof(app_info.md5); i++) {
    sprintf(&app_md5_str[i * 2], "%02X", app_info.md5[i]);
  }
  app_md5_str[2 * sizeof(app_info.md5)] = '\0';
  DPRINTF("MD5 hash of app info: %s\n", app_md5_str);

  // Compare the MD5 hash with the one in the app info
  if (memcmp(app_info.md5, app_info.file_md5_digest, sizeof(app_info.md5)) !=
      0) {
    DPRINTF("MD5 hash mismatch\n");
    return DOWNLOAD_MD5MISMATCH_ERROR;
  } else {
    DPRINTF("MD5 hash match\n");
  }

  // We have to add this new downloaded file to the app lookup table
  uint8_t *table = malloc(FLASH_SECTOR_SIZE);
  if (table == NULL) {
    return DOWNLOAD_CANNOTCREATE_CONFIG;
  }
  uint16_t table_length = 0;
  // Read the table from flash memory
  appmngr_load_apps_lookup_table(table, &table_length);

  // Update the table
  uint16_t new_sector =
      appmngr_find_first_empty_config_sector(table, &table_length);
  res = appmngr_update_lookup_table(app_info.uuid, new_sector, table,
                                    &table_length);

  // Persist the table if ok
  if (res == -1) {
    DPRINTF("Something went wrong updating the app lookup table.\n");
  } else {
    res = appmngr_persist_app_lookup_table(table, table_length);
    if (res == -1) {
      DPRINTF("There was something wrong persisting the app lookup table.\n");
    } else {
      DPRINTF("App lookup table pesisted!\n");
    }
  }
  free(table);

  // Now delete the config sector of the app
  res = appmngr_delete_config_sector(new_sector);
  if (res != 0) {
    DPRINTF("Error deleting config sector: %i\n", res);
    return DOWNLOAD_CANNOTDELETECONFIGSECTOR_ERROR;
  }
  DPRINTF("Config sector %d deleted\n", new_sector);
  return DOWNLOAD_OK;
}

download_err_t appmngr_confirm_download_app() {
  // First, get the uuid of the app
  char uuid[64] = {0};
  snprintf(uuid, sizeof(uuid), "%s", app_info.uuid);

  // Get the filename of the app info json
  char json_filename[256] = {0};
  snprintf(json_filename, sizeof(json_filename), "%s/%s.json",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value,
           uuid);

  // Get the filename of the app binary in uf2 format
  char binary_filename[256] = {0};
  snprintf(binary_filename, sizeof(binary_filename), "%s/%s.uf2",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value,
           uuid);

  DPRINTF("Writing files %s and %s\n", json_filename, binary_filename);
  // Try to delete the files if they exist
  f_unlink(json_filename);
  f_unlink(binary_filename);

  // Now rename the tmp files to the final filenames
  char tmp_json_filename[256] = {0};
  snprintf(tmp_json_filename, sizeof(tmp_json_filename), "%s/tmp.json",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value);
  char tmp_binary_filename[256] = {0};
  snprintf(tmp_binary_filename, sizeof(tmp_binary_filename), "%s/tmp.download",
           settings_find_entry(gconfig_getContext(), PARAM_APPS_FOLDER)->value);
  // Rename the json file to the final filename
  FRESULT res = f_rename(tmp_json_filename, json_filename);
  if (res != FR_OK) {
    DPRINTF("Error renaming json file: %i\n", res);
    return DOWNLOAD_CANNOTRENAMEFILE_ERROR;
  }
  // Rename the binary file to the final filename
  res = f_rename(tmp_binary_filename, binary_filename);
  if (res != FR_OK) {
    DPRINTF("Error renaming binary file: %i\n", res);
    return DOWNLOAD_CANNOTRENAMEFILE_ERROR;
  }
  DPRINTF("Written files %s and %s\n", json_filename, binary_filename);
  return DOWNLOAD_OK;
}

download_err_t appmngr_confirm_failed_download_app() {
  // // Delete tmp files due to failed download
  // char tmp_json_filename[256] = {0};
  // snprintf(tmp_json_filename, sizeof(tmp_json_filename), "%s/tmp.json",
  //          settings_find_entry(gconfig_getContext(),
  //          PARAM_APPS_FOLDER)->value);
  // char tmp_binary_filename[256] = {0};
  // snprintf(tmp_binary_filename, sizeof(tmp_binary_filename),
  // "%s/tmp.download",
  //          settings_find_entry(gconfig_getContext(),
  //          PARAM_APPS_FOLDER)->value);

  // // Try to delete the files if they exist
  // f_unlink(tmp_json_filename);
  // f_unlink(tmp_binary_filename);

  // DPRINTF("Deleted files %s and %s\n", tmp_json_filename,
  // tmp_binary_filename);
  return DOWNLOAD_STATUS_FAILED;
}

void appmngr_init() {
  sdcard_info = (sdcard_info_t){false, 0, 0, false};
  app_info = (app_info_t){0};
  download_status = DOWNLOAD_STATUS_IDLE;
  request = (HTTPC_REQUEST_T){0};
}

void appmngr_deinit(void) {
  if (file.obj.fs) {
    f_close(&file);
  }

  sdcard_info = (sdcard_info_t){false, 0, 0, false};
  app_info = (app_info_t){0};
  download_status = DOWNLOAD_STATUS_IDLE;
  request = (HTTPC_REQUEST_T){0};
}