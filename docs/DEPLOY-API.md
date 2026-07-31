# Developer deploy API

Push a microfirmware `.uf2` to a Booster device over WiFi and run it, without a debug probe
and without the USB/BOOTSEL dance.

This exists for people writing microfirmwares. If you are not writing one, you do not need
it, and you should leave it switched off.

## Read this first

**The API is not authenticated.** While it is enabled, anyone who can reach the device on
the network can install and run code on it, and that code runs on a cartridge plugged into
your Atari's bus. The web UI is served over plain `http`, so there is nothing to protect
the connection either.

What the gate below gives you is that switching it on is deliberate, obvious, and easy to
undo. It is not a security boundary. Enable it on a network you trust, and switch it off
when you are done.

## Turning it on

Two conditions must both hold. They are checked by the firmware on every request.

1. **Install the DEV APP.** Go to Config, set the release catalog to
   `Development - Local development only`, then install `DEV APP` from the Apps page like
   any other microfirmware.
2. **Stay on the Development channel.** The catalog setting must remain the development
   catalog while you are using the API.

When both hold, every Manager page shows a red **DEVELOPMENT MODE** banner. That banner is
the ground truth: if it is not there, the API is not answering.

### Turning it off

Either switch the release catalog to something other than Development, or delete the DEV
APP from the Apps page. Either one closes the gate immediately.

> **If your uploads suddenly stop working, check the channel.** Switching to Stable or Beta
> to look something up disables the API until you switch back. This is deliberate, and it
> is the most likely reason a command that worked five minutes ago now fails.

## The two calls

### Upload

```
POST /dev_upload.cgi
Content-Type: application/octet-stream
Body: the raw .uf2
```

```sh
curl --fail --data-binary @build/myapp.uf2 \
     -H "Content-Type: application/octet-stream" \
     "http://sidecart.local/dev_upload.cgi"
```

The body is written to the SD card as the development app's `.uf2`. Nothing is flashed yet.

Send a real `Content-Length` — `curl --data-binary` does. The firmware compares it against
what actually arrives and **refuses to commit a short transfer**, so a dropped connection
leaves your previous binary intact rather than a truncated one that would brick the
microfirmware slot on the next launch.

The upload replaces the previous one. There is no versioning and no undo.

### Launch

```
GET /mngr_launchapp.cgi?uuid=44444444-4444-4444-8444-444444444444
```

```sh
curl --fail "http://sidecart.local/mngr_launchapp.cgi?uuid=44444444-4444-4444-8444-444444444444"
```

This programs the uploaded `.uf2` into the microfirmware slot and reboots into it. The
device drops off the network while it does so; that is the microfirmware taking over, not a
failure.

`44444444-4444-4444-8444-444444444444` is the fixed UUID of the development app. It is not
a placeholder for you to change.

### Getting back to Booster

Press the **SELECT** button on the device and follow the instructions on the Atari screen.
The deploy API belongs to Booster, so it is not reachable while your microfirmware is
running.

## Makefile

```make
DEVICE ?= sidecart.local
DEV_UUID = 44444444-4444-4444-8444-444444444444
UF2      = build/myapp.uf2

.PHONY: deploy upload launch

deploy: upload launch

upload: $(UF2)
	@echo "Uploading $(UF2) to $(DEVICE)..."
	@curl --fail --silent --show-error \
	      --data-binary @$(UF2) \
	      -H "Content-Type: application/octet-stream" \
	      "http://$(DEVICE)/dev_upload.cgi"

launch:
	@echo "Launching on $(DEVICE)..."
	@curl --fail --silent --show-error \
	      "http://$(DEVICE)/mngr_launchapp.cgi?uuid=$(DEV_UUID)"
```

Then `make deploy` after a build. Override the host with `make deploy DEVICE=192.168.1.50`
if mDNS is not working on your network.

`--fail` matters: without it `curl` exits 0 on an HTTP error, and `make deploy` would
cheerfully launch a binary that never uploaded.

## When it does not work

| Symptom | Cause |
| --- | --- |
| Upload refused, no banner on the web pages | The gate is shut. Check both conditions: DEV APP installed, Development channel selected |
| Worked before, fails now | You almost certainly changed the release catalog. Switch it back |
| Upload refused immediately on a large file | The `.uf2` is bigger than the microfirmware slot allows |
| Upload appears to work, launch runs the old binary | The transfer was short and was rejected rather than committed. Check `curl` exited 0 |
| Nothing responds at `sidecart.local` | mDNS. Use the IP address shown on the Atari screen |

## What this does not do

- **No integrity check.** The MD5 in the catalog describes the placeholder binary you
  replaced, so there is nothing to verify an uploaded file against. Structure is checked
  when flashing, contents are not.
- **No arbitrary UUIDs.** Only the development app can be replaced this way. Installing
  real microfirmwares still goes through the catalog.
- **No rollback.** Recover a bad upload by uploading a good one, or install any
  microfirmware from the Apps page to overwrite the slot.
