# Editing the web UI

Pages live in `booster/src/fs/` as `.shtml`/`.html`/`.css` and are served from flash. Dynamic values come from lwIP SSI tags declared in `ssi_tags[]` in `mngr_httpd.c` (and its fabric counterpart), and the handler resolves them with a `switch (iIndex)` over **array position**.

- Tag names are capped at 8 characters by lwIP.
- Never insert or reorder entries in the middle of `ssi_tags[]` — every case index below the insertion point shifts and the whole page set silently renders the wrong values. Use one of the reserved `PLHLDRnn` slots or append at the end.
- Form endpoints are `tCGI` entries in `cgi_handlers[]`; the handler returns the URL to redirect to.
- After changing anything under `fs/`, rebuild — `fsdata_srv.c` is regenerated and the flash-usage check may now fail.
