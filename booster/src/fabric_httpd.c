/**
 * File: fabric_httpd.c
 * Author: Diego Parrilla Santamaría
 * Date: December 2024
 * Copyright: 2024 - GOODDATA LABS SL
 * Description: HTTPD server functions for fabric httpd
 */

#include "fabric_httpd.h"

#define WIFI_PASS_BUFSIZE 64
static char *ssid = NULL;
static char pass[WIFI_PASS_BUFSIZE];
static int auth = -1;
static void *current_connection;
static void *valid_connection;
static fabric_httpd_callback_t set_config_callback;
static fabric_httpd_served_callback_t set_served_callback = NULL;

// Decodes a URI-escaped string (percent-encoded) into its original value.
// E.g., "My%20SSID%21" -> "My SSID!"
// Returns true on success. Always null-terminates output.
// Output buffer must be at least outLen bytes.
// Returns false if input is NULL, output is NULL, or output buffer is too
// small.
static bool url_decode(const char *in, char *out, size_t outLen) {
  if (!in || !out || outLen == 0) return false;

  DPRINTF("Decoding '%s'. Length=%d\n", in, outLen);
  size_t o = 0;
  for (size_t i = 0; in[i] && o < outLen - 1; ++i) {
    if (in[i] == '%' && isxdigit((unsigned char)in[i + 1]) &&
        isxdigit((unsigned char)in[i + 2])) {
      // Decode %xx
      char hi = in[i + 1], lo = in[i + 2];
      int high = (hi >= 'A' ? (toupper(hi) - 'A' + 10) : (hi - '0'));
      int low = (lo >= 'A' ? (toupper(lo) - 'A' + 10) : (lo - '0'));
      out[o++] = (char)((high << 4) | low);
      i += 2;
    } else if (in[i] == '+') {
      // Optionally decode + as space (common in x-www-form-urlencoded)
      out[o++] = ' ';
    } else {
      out[o++] = in[i];
    }
  }
  out[o] = '\0';
  DPRINTF("Decoded to '%s'. Size: %d\n", out, o + 1);
  return true;
}

/**
 * @brief Array of SSI tags for the HTTP server.
 *
 * This array contains the SSI tags used by the HTTP server to dynamically
 * insert content into web pages. Each tag corresponds to a specific piece of
 * information that can be updated or retrieved from the server.
 */
const char *ssi_tags[] = {
    "HOMEPAGE",  // 0
    "WIFINETS",  // 1
    "WIFISSID",  // 2
    "WIFIAUTH",  // 3
    "TITLEHDR",  // 4
};

/**
 * @brief
 *
 * @param iIndex The index of the CGI handler.
 * @param iNumParams The number of parameters passed to the CGI handler.
 * @param pcParam An array of parameter names.
 * @param pcValue An array of parameter values.
 * @return The URL of the page to redirect to after the floppy disk image is
 * selected.
 */
const char *cgi_test(int iIndex, int iNumParams, char *pcParam[],
                     char *pcValue[]) {
  DPRINTF("TEST CGI handler called with index %d\n", iIndex);
  return "/test.shtml";
}

static const char *cgi_network_select(int iIndex, int iNumParams,
                                      char *pcParam[], char *pcValue[]) {
  DPRINTF("cgi_network_select called with index %d\n", iIndex);
  for (size_t i = 0; i < iNumParams; i++) {
    /* check if parameter is "id" */
    if (strcmp(pcParam[i], "id") == 0) {
      DPRINTF("Network selected: %s\n", pcValue[i]);
      wifi_scan_data_t *wifi_nets = network_getFoundNetworks();
      ssid = wifi_nets->networks[atoi(pcValue[i])].ssid;
      auth = wifi_nets->networks[atoi(pcValue[i])].auth_mode;
      DPRINTF("Network name: %s. Auth: %i\n", ssid, auth);
    }
  }
  return "/ap_step2.shtml";
}

/**
 * @brief Array of CGI handlers for floppy select and eject operations.
 *
 * This array contains the mappings between the CGI paths and the corresponding
 * handler functions for selecting and ejecting floppy disk images for drive A
 * and drive B.
 */
static const tCGI cgi_handlers[] = {
    {"/test.cgi", cgi_test}, {"/cgi_network_select.cgi", cgi_network_select}};

/**
 * @brief Initializes the HTTP server with optional SSI tags, CGI handlers, and
 * an SSI handler function.
 *
 * This function initializes the HTTP server and sets up the provided Server
 * Side Include (SSI) tags, Common Gateway Interface (CGI) handlers, and SSI
 * handler function. It first calls the httpd_init() function to initialize the
 * HTTP server.
 *
 * The filesystem for the HTTP server is in the 'fs' directory in the project
 * root.
 *
 * @param ssi_tags An array of strings representing the SSI tags to be used in
 * the server-side includes.
 * @param num_tags The number of SSI tags in the ssi_tags array.
 * @param ssi_handler_func A pointer to the function that handles SSI tags.
 * @param cgi_handlers An array of tCGI structures representing the CGI handlers
 * to be used.
 * @param num_cgi_handlers The number of CGI handlers in the cgi_handlers array.
 */
void httpd_server_init(const char *ssi_tags[], size_t num_tags,
                       tSSIHandler ssi_handler_func, const tCGI *cgi_handlers,
                       size_t num_cgi_handlers) {
  httpd_init();

  // SSI Initialization
  if (num_tags > 0) {
    for (size_t i = 0; i < num_tags; i++) {
      LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN",
                  strlen(ssi_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
    }
    http_set_ssi_handler(ssi_handler_func, ssi_tags, num_tags);
  } else {
    DPRINTF("No SSI tags defined.\n");
  }

  // CGI Initialization
  if (num_cgi_handlers > 0) {
    http_set_cgi_handlers(cgi_handlers, num_cgi_handlers);
  } else {
    DPRINTF("No CGI handlers defined.\n");
  }

  DPRINTF("HTTP server initialized.\n");
}

/**
 * @brief Server Side Include (SSI) handler for the HTTPD server.
 *
 * This function is called when the server needs to dynamically insert content
 * into web pages using SSI tags. It handles different SSI tags and generates
 * the corresponding content to be inserted into the web page.
 *
 * @param iIndex The index of the SSI handler.
 * @param pcInsert A pointer to the buffer where the generated content should be
 * inserted.
 * @param iInsertLen The length of the buffer.
 * @param current_tag_part The current part of the SSI tag being processed (used
 * for multipart SSI tags).
 * @param next_tag_part A pointer to the next part of the SSI tag to be
 * processed (used for multipart SSI tags).
 * @return The length of the generated content.
 */
u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen
#if LWIP_HTTPD_SSI_MULTIPART
                  ,
                  u16_t current_tag_part, u16_t *next_tag_part
#endif /* LWIP_HTTPD_SSI_MULTIPART */
) {
  DPRINTF("SSI handler called with index %d\n", iIndex);
  size_t printed;
  switch (iIndex) {
    case 0: /* "HOMEPAGE" */
      // Always to the first step of the configuration
      printed = snprintf(
          pcInsert, iInsertLen, "%s",
          "<meta http-equiv='refresh' content='0;url=/ap_step1.shtml'>");
      break;
    case 1: /* "WIFINETS" */
    {
      // Iterate over the floppy catalog an create a large string with the HTML
      // code as follows <option value="floppy1.st">floppy1.st</option> <option
      // value="floppy2.st">floppy2.st</option>
      // ...
      wifi_scan_data_t *wifi_nets = network_getFoundNetworks();
      DPRINTF("Current tag part: %d\n", current_tag_part);
      if ((wifi_nets != NULL) && (wifi_nets->count > 0)) {
        // Concatenate the network information in a single string
        char buffer[256];
        snprintf(buffer, 256, "%s (%d dBm)\n",
                 wifi_nets->networks[current_tag_part].ssid,
                 wifi_nets->networks[current_tag_part].rssi);
        printed = snprintf(
            pcInsert, iInsertLen,
            "<div><a href='/cgi_network_select.cgi?id=%d'>%s</a></div>\n",
            current_tag_part, buffer);
        DPRINTF("Printed: %d\n", printed);

        if (current_tag_part < (wifi_nets->count - 1)) {
          *next_tag_part = current_tag_part + 1;
        }
      } else {
        printed = snprintf(pcInsert, iInsertLen, "NO NETWORKS FOUND");
      }
      break;
    }
    case 2: /* WIFISSID */
    {
      if (ssid != NULL) {
        printed = snprintf(pcInsert, iInsertLen, "%s", ssid);
      } else {
        printed = snprintf(pcInsert, iInsertLen, "No network selected");
      }
      break;
    }
    case 3: /* WIFIAUTH */
    {
      if (auth != -1) {
        printed = snprintf(pcInsert, iInsertLen, "%s",
                           network_getAuthTypeString(auth));
      } else {
        printed = snprintf(pcInsert, iInsertLen, "No network selected");
      }
      break;
    }
    case 4: /* TITLEHDR */
    {
#if _DEBUG == 0
      printed = snprintf(pcInsert, iInsertLen, "%s (%s)", BOOSTER_TITLE,
                         RELEASE_VERSION);
#else
      printed = snprintf(pcInsert, iInsertLen, "%s (%s-%s)", BOOSTER_TITLE,
                         RELEASE_VERSION, RELEASE_DATE);
#endif
      break;
    }
    default: /* unknown tag */
      printed = 0;
      break;
  }
  if (set_served_callback != NULL) {
    set_served_callback();
  }
  LWIP_ASSERT("sane length", printed <= 0xFFFF);
  return (u16_t)printed;
}

err_t httpd_post_begin(void *connection, const char *uri,
                       const char *http_request, u16_t http_request_len,
                       int content_len, char *response_uri,
                       u16_t response_uri_len, u8_t *post_auto_wnd) {
  LWIP_UNUSED_ARG(connection);
  LWIP_UNUSED_ARG(http_request);
  LWIP_UNUSED_ARG(http_request_len);
  LWIP_UNUSED_ARG(content_len);
  LWIP_UNUSED_ARG(post_auto_wnd);
  DPRINTF("POST request for URI: %s\n", uri);
  if (!memcmp(uri, "/ap_pass.cgi", 11)) {
    DPRINTF("POST request for ap_pass.cgi\n");
    if (current_connection != connection) {
      DPRINTF("First POST connection. OK\n");
      current_connection = connection;
      valid_connection = NULL;
      snprintf(response_uri, response_uri_len, "/ap_step2.shtml");
      *post_auto_wnd = 1;
      return ERR_OK;
    }
    DPRINTF(
        "POST request for ap_pass.cgi. Connection already in progress. Not "
        "OK.\n");
  }
  return ERR_VAL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
  if (current_connection == connection) {
    DPRINTF("POST data received\n");
    u16_t token_pass = pbuf_memfind(p, "pass=", 5, 0);
    if (token_pass != 0xFFFF) {
      u16_t value_pass = token_pass + 5;
      u16_t len_pass = 0;
      u16_t tmp;
      /* find pass len */
      tmp = pbuf_memfind(p, "&", 1, value_pass);
      if (tmp != 0xFFFF) {
        len_pass = tmp - value_pass;
      } else {
        len_pass = p->tot_len - value_pass;
      }
      if ((len_pass > 0) && (len_pass < WIFI_PASS_BUFSIZE)) {
        /* provide contiguous storage if p is a chained pbuf */
        char buf_pass[WIFI_PASS_BUFSIZE];
        char *rawPass = (char *)pbuf_get_contiguous(
            p, buf_pass, sizeof(buf_pass), len_pass, value_pass);
        rawPass[len_pass] = '\0';
        DPRINTF("Password raw: %s\n", rawPass);
        if (rawPass) {
          bool valid = url_decode(rawPass, pass, len_pass + 1);
          DPRINTF("Bad encoding: %s\n", valid ? "No" : "Yes");
          valid_connection = connection;
          DPRINTF("Password: %s\n", pass);
        }
      }
    }
    return ERR_OK;
  }
  DPRINTF("POST data received for invalid connection\n");
  return ERR_VAL;
}

void httpd_post_finished(void *connection, char *response_uri,
                         u16_t response_uri_len) {
  snprintf(response_uri, response_uri_len, "/ap_step2.shtml");
  if (current_connection == connection) {
    if (valid_connection == connection) {
      DPRINTF("POST finished. Connection is valid\n");
      snprintf(response_uri, response_uri_len, "/ap_step3.shtml");
      if (set_config_callback) {
        set_config_callback(ssid, pass, auth, true);
      }
    } else {
      DPRINTF("POST finished. Connection is invalid\n");
    }
    current_connection = NULL;
    valid_connection = NULL;
  } else {
    DPRINTF("POST finished for invalid connection\n");
  }
}

// The main function should be as follows:
void fabric_httpd_start(fabric_httpd_callback_t callback,
                        fabric_httpd_served_callback_t served_callback) {
  // Set the callback function to be called when the configuration is set
  set_config_callback = callback;

  // Set the callback function to be called when the a page is served
  set_served_callback = served_callback;

  // Initialize the HTTP server with SSI tags and CGI handlers
  httpd_server_init(ssi_tags, LWIP_ARRAYSIZE(ssi_tags), ssi_handler,
                    cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
}