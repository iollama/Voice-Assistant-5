#pragma once
#include <stdint.h>
#include <string.h>

// =====================================================================
// SAVED WI-FI NETWORKS — record types + pure store logic
// =====================================================================
// Up to WIFI_MAX_NETWORKS credential pairs, persisted as ONE blob under the
// key "nets" in the "wifi-creds" NVS namespace (see wifi_store_load /
// wifi_store_persist in wifi.ino). All six records share a single key so that
// an eviction — drop the LRU slot, write the new entry, bump next_seq — is a
// single atomic write, and so next_seq can never drift out of step with the
// use_seq values it hands out.
//
// This header holds only pure logic: no NVS, no WiFi, no Preferences, no
// Arduino String. It is a real header (not an .ino) so the Arduino
// auto-prototyper never sees these types — same reason ring_buffer.h is one.

#define WIFI_MAX_NETWORKS   6
#define WIFI_SSID_MAX      33     // 32 chars + NUL
#define WIFI_PASS_MAX      64     // 63 chars + NUL
// Bumping this obliges you to write a migration. wifi_store_is_valid() rejects
// any version it doesn't recognise, so a bare bump makes every existing device
// fail validation and boot with no saved networks. Read the old version and
// convert it — wifi_store_migrate_legacy() in wifi.ino shows the shape.
#define WIFI_STORE_VERSION  1     // 2 will be the encrypted blob

struct WifiNet {
    char     ssid[WIFI_SSID_MAX];
    char     pass[WIFI_PASS_MAX];
    uint32_t use_seq;      // 0 = never connected; higher = more recently used
    uint32_t last_epoch;   // wall clock of last successful connect; display only (0 = unknown)
};

struct WifiStore {
    uint8_t  version;
    uint8_t  count;
    uint32_t next_seq;     // monotonic; pre-incremented on each successful connect
    WifiNet  nets[WIFI_MAX_NETWORKS];
};

// Ordering runs on use_seq, not on last_epoch: NTP has not synced when the boot
// cascade picks a network (syncTime() runs after the connect), so wall clock is
// unavailable and, once it is, subject to jumps. last_epoch exists purely so the
// portal can render "last used <date>".

static inline void wifi_store_init(WifiStore& s) {
    memset(&s, 0, sizeof(s));
    s.version  = WIFI_STORE_VERSION;
    s.count    = 0;
    s.next_seq = 1;
}

// Copy src into dst[0..cap-1], always NUL-terminated, truncating if needed.
static inline void wifi_copy_field(char* dst, size_t cap, const char* src) {
    if (cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = '\0';
}

// Index of the entry with this SSID, or -1. SSIDs are case-sensitive, as 802.11 has them.
static inline int wifi_store_find(const WifiStore& s, const char* ssid) {
    if (!ssid) return -1;
    for (int i = 0; i < s.count; i++) {
        if (strcmp(s.nets[i].ssid, ssid) == 0) return i;
    }
    return -1;
}

// Index of the least-recently-used entry — the one eviction drops. A never-used
// entry has use_seq == 0, which is lower than any used entry, so preloads that
// have not connected yet go first. Ties break toward the lower index (the older
// slot). Returns -1 for an empty store.
static inline int wifi_store_lru_index(const WifiStore& s) {
    if (s.count == 0) return -1;
    int best = 0;
    for (int i = 1; i < s.count; i++) {
        if (s.nets[i].use_seq < s.nets[best].use_seq) best = i;
    }
    return best;
}

// Add a network, or update the password of one already stored.
//
// Returns the slot index, or -1 if the SSID is empty. When the store is full and
// the SSID is new, the LRU entry is evicted silently; its SSID is copied into
// evicted_out (which must hold WIFI_SSID_MAX bytes) so the portal can report
// after the fact what was dropped. evicted_out is set to "" when nothing was
// evicted, and may be null if the caller does not care.
//
// An update to an existing entry preserves its use_seq/last_epoch — changing a
// password is not a use, and must not reshuffle the boot pick order.
static inline int wifi_store_upsert(WifiStore& s, const char* ssid, const char* pass,
                                    char* evicted_out) {
    if (evicted_out) evicted_out[0] = '\0';
    if (!ssid || ssid[0] == '\0') return -1;

    int idx = wifi_store_find(s, ssid);
    if (idx >= 0) {
        wifi_copy_field(s.nets[idx].pass, WIFI_PASS_MAX, pass);
        return idx;
    }

    if (s.count < WIFI_MAX_NETWORKS) {
        idx = s.count;
        s.count++;
    } else {
        idx = wifi_store_lru_index(s);
        if (evicted_out) wifi_copy_field(evicted_out, WIFI_SSID_MAX, s.nets[idx].ssid);
    }

    memset(&s.nets[idx], 0, sizeof(WifiNet));
    wifi_copy_field(s.nets[idx].ssid, WIFI_SSID_MAX, ssid);
    wifi_copy_field(s.nets[idx].pass, WIFI_PASS_MAX, pass);
    s.nets[idx].use_seq    = 0;   // never used yet
    s.nets[idx].last_epoch = 0;
    return idx;
}

// Remove an entry by SSID, compacting the array. Returns true if one was removed.
static inline bool wifi_store_remove(WifiStore& s, const char* ssid) {
    int idx = wifi_store_find(s, ssid);
    if (idx < 0) return false;
    for (int i = idx; i < s.count - 1; i++) {
        s.nets[i] = s.nets[i + 1];
    }
    memset(&s.nets[s.count - 1], 0, sizeof(WifiNet));
    s.count--;
    return true;
}

// Stamp a successful connection. epoch may be 0 when NTP has not synced yet;
// the caller re-stamps it after syncTime() succeeds.
static inline void wifi_store_mark_used(WifiStore& s, int idx, uint32_t epoch) {
    if (idx < 0 || idx >= s.count) return;
    s.nets[idx].use_seq = s.next_seq++;
    if (epoch > 0) s.nets[idx].last_epoch = epoch;
}

// Fill out[] with entry indices ordered most-recently-used first. Returns the
// count written. Insertion sort — six elements, no allocation.
static inline int wifi_store_order_by_recency(const WifiStore& s, uint8_t* out) {
    int n = s.count;
    for (int i = 0; i < n; i++) out[i] = (uint8_t)i;
    for (int i = 1; i < n; i++) {
        uint8_t key = out[i];
        int j = i - 1;
        while (j >= 0 && s.nets[out[j]].use_seq < s.nets[key].use_seq) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    return n;
}

// Repair a store whose next_seq has fallen behind the use_seq values it already
// handed out (only reachable via a corrupt or hand-edited blob). Left alone, the
// next successful connect would be stamped at or below an older entry and the
// boot pick order would be quietly wrong — a miserable thing to diagnose from a
// device that "sometimes joins the wrong network".
static inline void wifi_store_normalize(WifiStore& s) {
    uint32_t max_seq = 0;
    for (int i = 0; i < s.count; i++) {
        if (s.nets[i].use_seq > max_seq) max_seq = s.nets[i].use_seq;
    }
    if (s.next_seq <= max_seq) s.next_seq = max_seq + 1;
}

// Sanity-check a blob read back from NVS. A store that fails this is discarded
// and re-initialised rather than trusted — a corrupt count would index past the
// array in every loop above.
static inline bool wifi_store_is_valid(const WifiStore& s) {
    if (s.version != WIFI_STORE_VERSION) return false;
    if (s.count > WIFI_MAX_NETWORKS) return false;
    for (int i = 0; i < s.count; i++) {
        if (s.nets[i].ssid[WIFI_SSID_MAX - 1] != '\0') return false;
        if (s.nets[i].pass[WIFI_PASS_MAX - 1] != '\0') return false;
        if (s.nets[i].ssid[0] == '\0') return false;
    }
    return true;
}
