#ifndef CONFIG_H
#define CONFIG_H

// CYD Display Configuration
#define CYD_PANEL_W 240
#define CYD_PANEL_H 320

// Audio output
#define CYD_SPEAKER_PIN 26
#define CYD_SPEAKER_PWM_CHANNEL 0

// ---------------------------------------------------------------------------
// Default values (used the first time the device boots or after reset).
// After first boot these are overridden by values entered in the WiFi setup
// portal and saved to flash.
// ---------------------------------------------------------------------------
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define GENMON_HOST ""
#define GENMON_PORT 8000

// How often to refresh the dashboard from GenMon (milliseconds).
#define GENMON_POLL_MS 5000

// Timezone offset from UTC in hours (e.g., -5 for Eastern Standard Time, -4 for EDT).
// This is applied to the NTP-synchronized clock so timestamps display in local time.
#define TIMEZONE_OFFSET_HOURS -4

// If your GenMon web UI has HTTP basic auth enabled, set these.
// Leave user empty ("") to disable auth.
#define GENMON_AUTH_USER ""
#define GENMON_AUTH_PASS ""

#endif // CONFIG_H
