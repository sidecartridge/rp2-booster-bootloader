#include "version.h"

#include <ctype.h>

typedef struct {
  char protocol[16];
  char host[128];
  char uri[256];
} url_components_t;

download_version_t status = DOWNLOAD_VERSION_IDLE;
static char version_string[64] = {0};
static size_t version_buf_used = 0;  // <— track appended bytes

// Persistent storage for async HTTP request strings to avoid dangling pointers
static char request_host_buf[256];
static char request_uri_buf[512];
static HTTPC_REQUEST_T request = {0};

// ------------------- helpers -------------------
static int strncasecmp_ascii(const char *a, const char *b, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    unsigned char ca = (unsigned char)a[i];
    unsigned char cb = (unsigned char)b[i];
    if (!ca || !cb) return tolower(ca) - tolower(cb);
    int d = tolower(ca) - tolower(cb);
    if (d) return d;
  }
  return 0;
}

// Extract the components of the URL (http/https://host[/uri])
static int parse_url(const char *url, url_components_t *components) {
  if (!url || !components) return -1;
  memset(components, 0, sizeof(*components));

  const char *protocol_end = strstr(url, "://");
  if (!protocol_end) return -1;

  size_t protocol_len = (size_t)(protocol_end - url);
  if (protocol_len == 0 || protocol_len >= sizeof(components->protocol))
    return -1;
  memcpy(components->protocol, url, protocol_len);
  components->protocol[protocol_len] = '\0';

  const char *host_start = protocol_end + 3;  // skip ://
  const char *uri_start = strchr(host_start, '/');
  if (uri_start) {
    size_t host_len = (size_t)(uri_start - host_start);
    if (host_len == 0 || host_len >= sizeof(components->host)) return -1;
    memcpy(components->host, host_start, host_len);
    components->host[host_len] = '\0';

    // Copy URI (ensure at least "/")
    if (strlen(uri_start) >= sizeof(components->uri)) return -1;
    strcpy(components->uri, uri_start);
  } else {
    // No path → set host and default uri "/"
    if (strlen(host_start) == 0 ||
        strlen(host_start) >= sizeof(components->host))
      return -1;
    strcpy(components->host, host_start);
    components->uri[0] = '/';
    components->uri[1] = '\0';
  }
  return 0;
}

// ------------------- HTTP callbacks -------------------

// Receive body → append to version_string (no malloc; safe on multiple chunks)
static err_t http_client_receive_version_fn(__unused void *arg,
                                            __unused struct altcp_pcb *conn,
                                            struct pbuf *p, err_t err) {
  if (p == NULL) {
    // remote end closed; result callback will follow
    return ERR_OK;
  }
  if (err != ERR_OK) {
    DPRINTF("Error receiving data: %d\n", err);
    status = DOWNLOAD_VERSION_FAILED;
    pbuf_free(p);
    return err;
  }

  // How many bytes can we still store (reserve 1 byte for NUL)
  size_t cap = sizeof(version_string) - 1u - version_buf_used;
  if (cap > 0) {
    // Copy up to 'cap' bytes from the pbuf chain
    u16_t to_copy = (u16_t)((p->tot_len < cap) ? p->tot_len : cap);
    if (to_copy) {
      pbuf_copy_partial(p, version_string + version_buf_used, to_copy, 0);
      version_buf_used += to_copy;
      version_string[version_buf_used] = '\0';
    }
  }
  // Acknowledge all received bytes even if we truncated locally
#if BOOSTER_DOWNLOAD_HTTPS == 1
  altcp_recved(conn, p->tot_len);
#else
  tcp_recved(conn, p->tot_len);
#endif
  pbuf_free(p);

  // Still in progress until result callback says done
  if (status == DOWNLOAD_VERSION_START || status == DOWNLOAD_VERSION_IDLE) {
    status = DOWNLOAD_VERSION_IN_PROGRESS;
  }
  return ERR_OK;
}

// Parse headers → enforce a small Content-Length that fits our buffer
static err_t http_client_header_check_size_fn(
    __unused httpc_state_t *connection, __unused void *arg, struct pbuf *hdr,
    u16_t hdr_len, __unused u32_t content_len) {
  // Copy header into a small stack buffer if possible; otherwise clamp
  char buf[512];
  u16_t copy = hdr_len < sizeof(buf) - 1 ? hdr_len : (sizeof(buf) - 1);
  pbuf_copy_partial(hdr, buf, copy, 0);
  buf[copy] = '\0';

  // Case-insensitive search for "Content-Length:"
  const char *label = "Content-Length:";
  char *p = buf;
  size_t max_len = sizeof(version_string) - 1;

  while (p && *p) {
    // find end of line
    char *eol = strstr(p, "\r\n");
    size_t line_len = eol ? (size_t)(eol - p) : strlen(p);

    if (line_len >= strlen(label) &&
        strncasecmp_ascii(p, label, strlen(label)) == 0) {
      const char *num = p + strlen(label);
      while (*num == ' ' || *num == '\t') ++num;
      unsigned long cl = strtoul(num, NULL, 10);
      if (cl > max_len) {
        DPRINTF("Content-Length too large: %lu > %u\n", cl, (unsigned)max_len);
        status = DOWNLOAD_VERSION_FAILED;
        return ERR_VAL;  // refuse large bodies
      }
      break;  // ok
    }
    p = eol ? (eol + 2) : NULL;
  }

  // Mark we’re ready to receive body
  if (status == DOWNLOAD_VERSION_START) status = DOWNLOAD_VERSION_IN_PROGRESS;
  return ERR_OK;
}

static void http_client_result_complete_fn(void *arg,
                                           httpc_result_t httpc_result,
                                           u32_t rx_content_len, u32_t srv_res,
                                           err_t err) {
  HTTPC_REQUEST_T *req = (HTTPC_REQUEST_T *)arg;
  DPRINTF("Request complete: result %d len %u server_response %u err %d\n",
          httpc_result, rx_content_len, srv_res, err);
  req->complete = true;

#if BOOSTER_DOWNLOAD_HTTPS == 1
  if (req->tls_config) {  // free here too to avoid leaks on error
    altcp_tls_free_config(req->tls_config);
    req->tls_config = NULL;
  }
#endif

  if (err == ERR_OK && srv_res == 200) {
    status = DOWNLOAD_VERSION_COMPLETED;
  } else {
    status = DOWNLOAD_VERSION_FAILED;
  }
}

// ------------------- driver code -------------------

static download_version_t version_start_download(const char *url) {
  // reset accumulation buffer
  version_buf_used = 0;
  version_string[0] = '\0';

  url_components_t components;

  if (!url || !*url) {
    // No URL: use built-in version
    snprintf(version_string, sizeof(version_string), "%s", RELEASE_VERSION);
    status = DOWNLOAD_VERSION_DEFAULT;
    return status;
  }
  if (parse_url(url, &components) != 0) {
    DPRINTF("Error parsing URL: %s\n", url ? url : "(null)");
    snprintf(version_string, sizeof(version_string), "%s", RELEASE_VERSION);
    status = DOWNLOAD_VERSION_CANNOTPARSEURL;
    return status;
  }
  status = DOWNLOAD_VERSION_START;

  // Persist host and URI (components are on stack)
  snprintf(request_host_buf, sizeof(request_host_buf), "%s", components.host);
  snprintf(request_uri_buf, sizeof(request_uri_buf), "%s", components.uri);
  request.hostname = request_host_buf;
  request.url = request_uri_buf;
  request.complete = false;

  request.headers_fn = http_client_header_check_size_fn;
  request.recv_fn = http_client_receive_version_fn;
  request.result_fn = http_client_result_complete_fn;

#if BOOSTER_DOWNLOAD_HTTPS == 1
  request.tls_config = altcp_tls_create_config_client(NULL, 0);
  DPRINTF("Download with HTTPS\n");
#else
  DPRINTF("Download with HTTP\n");
#endif

  int rc = http_client_request_async(cyw43_arch_async_context(), &request);
  if (rc != 0) {
    DPRINTF("http_client_request_async failed: %d\n", rc);
#if BOOSTER_DOWNLOAD_HTTPS == 1
    if (request.tls_config) {
      altcp_tls_free_config(request.tls_config);
      request.tls_config = NULL;
    }
#endif
    status = DOWNLOAD_VERSION_FAILED;
    return status;
  }
  status = DOWNLOAD_VERSION_IN_PROGRESS;
  return status;
}

static download_version_poll_t version_poll_download(void) {
  if (!request.complete) {
    async_context_poll(cyw43_arch_async_context());
    async_context_wait_for_work_ms(cyw43_arch_async_context(),
                                   VERSION_POLL_INTERVAL_MS);
    return DOWNLOAD_VERSION_POLL_CONTINUE;
  }
  return DOWNLOAD_VERSION_POLL_COMPLETED;
}

static download_version_t version_finish_download(void) {
  DPRINTF("Download finished with status=%d, content=\"%s\"\n", status,
          version_string);
  return status;
}

void version_loop(void) {
  status = DOWNLOAD_VERSION_IDLE;
  request = (HTTPC_REQUEST_T){0};
  version_buf_used = 0;
  version_string[0] = '\0';

  download_version_t start_status = version_start_download(VERSION_URL);
  if (start_status == DOWNLOAD_VERSION_DEFAULT ||
      start_status == DOWNLOAD_VERSION_CANNOTPARSEURL ||
      start_status == DOWNLOAD_VERSION_FAILED) {
    return;
  }

  uint32_t waited_ms = 0;
  while (version_poll_download() == DOWNLOAD_VERSION_POLL_CONTINUE) {
    if (status == DOWNLOAD_VERSION_FAILED) break;
    waited_ms += VERSION_POLL_INTERVAL_MS;
    if (waited_ms >= VERSION_DOWNLOAD_TIMEOUT_MS) {
      DPRINTF("VERSION download timed out after %u ms\n", waited_ms);
      status = DOWNLOAD_VERSION_FAILED;
      break;
    }
  }

  (void)version_finish_download();
  request = (HTTPC_REQUEST_T){0};
}

download_version_t version_get_status(void) { return status; }

const char *version_get_string(void) {
  return (version_string[0] != '\0') ? version_string : RELEASE_VERSION;
}

// Parse up to three numeric components from a version string (e.g.,
// "2.0.5alpha" -> {2,0,5})
static void parse_version3_numbers(const char *str, unsigned int out[3]) {
  out[0] = out[1] = out[2] = 0U;
  if (!str) return;
  const char *p = str;
  for (int k = 0; k < 3 && *p; ++k) {
    while (*p && (*p < '0' || *p > '9')) ++p;
    unsigned v = 0U;
    while (*p >= '0' && *p <= '9') {
      v = v * 10u + (unsigned)(*p - '0');
      ++p;
    }
    out[k] = v;
  }
}

bool version_isNewer(void) {
  unsigned cur[3] = {0}, rel[3] = {0};
  parse_version3_numbers(version_string, cur);
  parse_version3_numbers(RELEASE_VERSION, rel);
  for (int i = 0; i < 3; ++i) {
    if (cur[i] != rel[i]) return cur[i] > rel[i];
  }
  return false;
}
