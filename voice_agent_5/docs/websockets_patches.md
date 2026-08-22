# ArduinoWebsockets Library Patches

Upstream baseline: **v0.5.4** ([gilmaimon/ArduinoWebsockets](https://github.com/gilmaimon/ArduinoWebsockets)), GPL-3.0.
For the licensing side — what was modified and why, in full — see [VENDORED.md](../src/VENDORED.md).

## How patches are applied — vendored `src/` approach

The patched library source lives inside this sketch at `src/` (e.g. `src/ArduinoWebsockets.h`,
`src/tiny_websockets/`, `src/websockets_client.cpp`, etc.).

Both Arduino IDE and PlatformIO (with `src_dir = voice_agent_5` in `platformio.ini`) compile all `.cpp` files in a sketch's `src/` subdirectory and add `src/` to the
compiler include path. This means:

- **No global library installation needed** for ArduinoWebsockets — the local copy is used automatically.
- **No manual patching** — the patches are already applied to the files in `src/`.
- The sketch includes the library with a relative path to prevent Arduino from auto-detecting
  and pulling in the globally installed (unpatched) copy:
  ```cpp
  #include "src/ArduinoWebsockets.h"   // local patched copy
  ```

ArduinoJson and Adafruit NeoPixel are still installed the normal way (Arduino Library Manager, or ``lib_deps`` in ``platformio.ini``).

### What was changed from the upstream library

Beyond the two bug fixes below:

- All internal `#include <tiny_websockets/...>` angle-bracket includes were converted to relative
  `#include "..."` paths so the library is self-contained without needing its `src/` directory on
  the compiler include path.
- The non-ESP32 platform directories (ESP8266, Linux, Windows, Teensy41) were removed since this
  sketch only targets ESP32-S3.
- **The platform dispatch in `src/tiny_websockets/internals/ws_common.hpp` was reduced to ESP32
  only** — its `#ifdef ESP8266` / `#elif defined(ARDUINO_TEENSY41)` branches were deleted. This is
  not optional cleanup: those branches `#include` headers from the directories removed above, so
  skipping this step produces a copy that does not compile.

### Updating the library

If you need to update to a newer version of ArduinoWebsockets:

1. Download the new version.
2. Copy its `src/` contents over the `src/` directory in this sketch.
3. Re-apply the two bug fixes below.
4. Convert all `#include <tiny_websockets/...>` to relative `#include "..."` paths (see commit
   history for the full list of changes).
5. Remove the non-ESP32 platform directories, **and** re-trim the platform dispatch in
   `ws_common.hpp` to match.
6. Restore the GPL-3.0 change notices at the top of the three modified files, and update the version
   recorded in [VENDORED.md](../src/VENDORED.md). Keeping that disclosure accurate is a licence
   obligation, not bookkeeping.

To confirm you have not missed anything, diff the result against the upstream tag with `#include`
lines normalised to their basenames — only the three modified files should differ.

---

## Bug fix 1 — `esp32_tcp.hpp`: missing `setInsecure()` on `SecuredEsp32TcpClient`

**File:** `src/tiny_websockets/network/esp32/esp32_tcp.hpp`

**Problem:** `SecuredEsp32TcpClient` had no `setInsecure()` method. When
`upgradeToSecuredConnection()` tried to call it, the call was silently ignored, leaving
`WiFiClientSecure` in certificate-verification mode with no CA bundle — every WSS connection
failed.

**Fix:** Add the method to `SecuredEsp32TcpClient`:

```cpp
class SecuredEsp32TcpClient : public GenericEspTcpClient<WiFiClientSecure> {
public:
  void setInsecure() {       // ADD THIS
    this->client.setInsecure();
  }
  void setCACert(const char* ca_cert) { ... }
  // ... rest unchanged
```

---

## Bug fix 2 — `websockets_client.cpp`: `setInsecure()` never called on ESP32

**File:** `src/websockets_client.cpp`

**Problem:** `upgradeToSecuredConnection()` had an `else { client->setInsecure(); }` fallback for
ESP8266 but the `#elif defined(ESP32)` block was missing it. Without a CA cert configured, no
fallback to insecure mode was triggered, so WSS always failed silently.

**Fix:** Add the `else` branch in the `#elif defined(ESP32)` block:

```cpp
    #elif defined(ESP32)
        if(this->_optional_ssl_ca_cert) {
            client->setCACert(this->_optional_ssl_ca_cert);
        }
        if(this->_optional_ssl_client_ca) {
            client->setCertificate(this->_optional_ssl_client_ca);
        }
        if(this->_optional_ssl_private_key) {
            client->setPrivateKey(this->_optional_ssl_private_key);
        } else {
            client->setInsecure();   // ADD THIS
        }
    #endif
```

---

## Notes

- Tested with ArduinoWebsockets v0.5.4 on ESP32-S3.
- These are upstream bugs. Both are still present in v0.5.4 and on `master` (last upstream commit
  June 2024), so a version bump alone will not remove the need for these patches — check, but do not
  assume.
