#ifndef GENMON_DASHBOARD_H
#define GENMON_DASHBOARD_H

#include <Arduino.h>
#include "lgfx_cyd.h"

// Runtime config loaded/saved via WiFiManager portal.
struct DashboardConfig
{
  char wifiSsid[64] = "";
  char wifiPass[64] = "";
  char genmonHost[64] = "";
  int  genmonPort = 8000;
};

struct GenMonData
{
  bool online = false;
  String error;

  // Status
  String engineState = "--";
  String switchState = "--";
  String generatorStatus = "--";
  String batteryVoltage = "--";
  String rpm = "--";
  String frequency = "--";

  // Output / power
  String outputVoltage = "--";
  String outputCurrent = "--";
  String outputPower = "--";

  // Maintenance
  String runHours = "--";
  String fuelLevel = "--";
  String serviceDue = "--";

  // Outage
  String utilityVoltage = "--";
  String outageStatus = "--";

  String lastUpdate = "--";
};

// Load saved config. Returns true if valid config exists.
bool dashboardLoadConfig(DashboardConfig& cfg);

// Access the active runtime config (used for GenMon host/port).
const DashboardConfig& dashboardGetConfig();

// Start the Wi-Fi connection and show progress on the display.
void dashboardConnectWiFi(LGFX* disp);

// Fetch fresh data from GenMon. Returns true if data was updated successfully.
bool dashboardFetchData(GenMonData& data);

// Render the full dashboard using the supplied data.
void dashboardRender(LGFX* disp, const GenMonData& data);

// Render a different page of the dashboard. page 0 = summary, page 1 = gauges.
void dashboardRenderPage(LGFX* disp, const GenMonData& data, uint8_t page);

#endif // GENMON_DASHBOARD_H
