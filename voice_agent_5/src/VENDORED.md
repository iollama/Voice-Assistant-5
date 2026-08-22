# Vendored: ArduinoWebsockets

This directory is **not** original VA5 code. It is a modified copy of a third-party library,
redistributed under its own licence.

| | |
|---|---|
| **Upstream** | https://github.com/gilmaimon/ArduinoWebsockets |
| **Upstream version** | **v0.5.4** |
| **Upstream author** | Gil Maimon |
| **Upstream licence** | **GPL-3.0** — full text in [`LICENSE`](LICENSE) beside this file |
| **Modified by** | Udi Tirosh, 2026-04-02 |
| **Upstream contribution** | [gilmaimon/ArduinoWebsockets#175](https://github.com/gilmaimon/ArduinoWebsockets/pull/175) — both fixes, offered back |

Because this library is GPL-3.0 and VA5 links it into the firmware, **VA5 as a whole is GPL-3.0**.
That is the reason for the project's licence, not an incidental choice.

## Why it is vendored at all

ArduinoWebsockets cannot open a WSS connection on ESP32. Two separate defects combine so that every
`connectSecure()` call fails **silently** — the call returns as though it proceeded, and then nothing
arrives. `WiFiClientSecure` is left in certificate-verification mode with no CA bundle, so the TLS
handshake dies with no callback and no error.

It presents as a network or credentials fault rather than a library bug, which is what makes it
dangerous: the obvious "fix" is to reinstall the stock library, which reintroduces it. The sketch
therefore includes the local copy by relative path (`#include "src/ArduinoWebsockets.h"`) specifically
to defeat Arduino's auto-detection of a globally installed, unpatched copy.

## What was changed

Verified mechanically against upstream v0.5.4: every file in this directory was diffed against its
upstream counterpart with `#include` lines normalised to their basenames, so the include-style
rewrite below cancels out and only real content drift remains. **19 of 22 files are byte-identical
to upstream once includes are normalised.**

Five changes in total: three that alter specific files (1-3 below), and two that are mechanical and
tree-wide (4-5). There are no others.

### 1. `setInsecure()` added to `SecuredEsp32TcpClient` — functional fix

`tiny_websockets/network/esp32/esp32_tcp.hpp`

The class had no `setInsecure()` at all, so `upgradeToSecuredConnection()`'s attempt to call it
could never do anything. Added the method, forwarding to the underlying `WiFiClientSecure`.

### 2. ESP32 `setInsecure()` fallback added — functional fix

`websockets_client.cpp`

`upgradeToSecuredConnection()` has an `else { client->setInsecure(); }` fallback in its ESP8266
branch, but the `#elif defined(ESP32)` branch was missing it. With no certificate configured,
nothing disabled verification. Added the `else` branch so the two platforms behave alike.

### 3. Platform dispatch reduced to ESP32 only — portability trim

`tiny_websockets/internals/ws_common.hpp`

The `#ifdef ESP8266 / #elif defined(ESP32) / #elif defined(ARDUINO_TEENSY41)` chain was reduced to a
plain `#ifdef ESP32`. This is the counterpart to the directory removal below — without it the file
still `#include`s headers for platforms whose directories are gone, and the sketch will not compile.

### 4. Includes made relative — mechanical, whole tree

Every internal `#include <tiny_websockets/...>` became a relative `#include "..."`, so the library is
self-contained without needing `src/` on the compiler include path. Purely a path rewrite; no logic
was touched. This is the change the verification above normalises away.

### 5. Non-ESP32 platform directories removed — mechanical

The `esp8266/`, `linux/`, `windows/` and `teensy41/` subdirectories were deleted. VA5 targets
ESP32-S3 only.

## What was *not* changed

No reformatting, no re-indentation, no refactoring, and no upstream copyright notice removed.
Upstream's own source files carry no per-file copyright headers. The two third-party notices that do
exist inside the library are both retained verbatim:

- `tiny_websockets/internals/wscrypto/base64.hpp` — © 2004-2008 René Nyffenegger, zlib-style licence.
  Its own clause 3 forbids removing or altering the notice.
- `tiny_websockets/internals/wscrypto/sha1.hpp` — credits https://github.com/983/SHA1.

## Why it is not fixed upstream

Both defects are still present in v0.5.4 and on `master`. Upstream has open issues describing the
symptom without identifying the cause — [#120](https://github.com/gilmaimon/ArduinoWebsockets/issues/120),
[#101](https://github.com/gilmaimon/ArduinoWebsockets/issues/101) and
[#152](https://github.com/gilmaimon/ArduinoWebsockets/issues/152) — and until PR #175 below, no open
pull request addressed either one. The repository has had no commits since June 2024.

Both fixes have been offered back upstream as
[PR #175](https://github.com/gilmaimon/ArduinoWebsockets/pull/175) (+6 lines, no deletions). Until
it is merged and released, the vendored copy stays — otherwise every builder would have to patch the
library by hand before the firmware would talk to anything.

## Updating to a newer upstream release

See [../docs/websockets_patches.md](../docs/websockets_patches.md) for the exact patches and the
step-by-step procedure. Re-apply **all five** changes above — dropping number 3 produces a copy that
does not compile.

To re-verify the baseline after an update: fetch the upstream tag's `src/`, normalise every
`#include` line to its basename so the path rewrite cancels out, then diff file-by-file. Anything
still differing is a real modification and needs a change notice under GPL-3.0 §5(a).
