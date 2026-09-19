// Deskbuddy V.8
// Nav: Home / Weather / Notes / Status
// Full version
// - KP dots replaced with Low / Medium / High / Extreme text
// - KP level text uses same small font as wind direction and stays inside the box
// - Wind + direction added to Weather page
// - Wind direction uses Accent color
// - Weather sun event field automatically shows Sunrise or Sunset, whichever is next
// - Uptime added to Status page

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DNSServer.h>
#include <Update.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

SET_LOOP_TASK_STACK_SIZE(32768);

void setBacklight(int value);
void setWifiEnabled(bool enabled);
void beginWiFiConnect();
void startSetupPortal();
void stopSetupPortal();

// =========================================================
// WIFI
// =========================================================
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";
const char* AP_SSID = "Deskbuddy";

String wifiSsid = WIFI_SSID;
String wifiPass = WIFI_PASS;
bool wifiApMode = false;
DNSServer dnsServer;

// =========================================================
// DISPLAY / TOUCH
// =========================================================
TFT_eSPI tft;

const int ROT = 2;
const bool INV = false;

#define TOUCH_CS  33
#define TOUCH_IRQ 36

static const int T_SCK  = 25;
static const int T_MISO = 39;
static const int T_MOSI = 32;

SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS);

static const int TOUCH_X_MIN = 562;
static const int TOUCH_X_MAX = 3604;
static const int TOUCH_Y_MIN = 544;
static const int TOUCH_Y_MAX = 3720;

static const bool TOUCH_SWAP_XY = false;
static const bool TOUCH_FLIP_X  = false;
static const bool TOUCH_FLIP_Y  = false;

// =========================================================
// WEB / STORAGE
// =========================================================
WebServer server(80);
Preferences prefs;

// =========================================================
// SPRITES
// =========================================================
TFT_eSprite sprClock = TFT_eSprite(&tft);
TFT_eSprite sprSmall = TFT_eSprite(&tft);

// =========================================================
// LOCATION
// =========================================================
float LAT = 52.5200f;
float LNG = 13.4050f;
String locationName = "Berlin";

// =========================================================
// THEME
// =========================================================
uint16_t COL_BG        = 0x08A3;
uint16_t COL_PANEL     = 0x1106;
uint16_t COL_PANEL_ALT = 0x18C7;
uint16_t COL_STROKE    = 0x31EC;
uint16_t COL_TEXT      = 0xEF7D;
uint16_t COL_DIM       = 0x94B2;
uint16_t COL_ACCENT    = 0x5EFA;

const uint16_t COL_GREEN  = TFT_GREEN;
const uint16_t COL_YELLOW = 0xFFE0;
const uint16_t COL_RED    = TFT_RED;
const uint16_t COL_BLUE   = 0x041F;

String textColorKey = "standard";
String unitKey = "metric"; // metric = C/mm, imperial = F/in
String regionFormatKey = "europe"; // europe = 24h + dd.mm.yyyy, us = 12h + mm/dd/yyyy
String timezoneKey = "europe_central";
String timezoneIana = "";
String timezonePosixCustom = "";
String langKey = "es";

// =========================================================
// LAYOUT
// =========================================================
const int SCREEN_W = 240;
const int SCREEN_H = 320;
const int TOPBAR_H = 34;
const int NAV_H    = 44;

const int HOME_GRID_Y1 = 120;
const int HOME_GRID_Y2 = 198;
const int HOME_WIDGET_H = 70;

const int HOME_TIMER_X = 124;
const int HOME_TIMER_Y = HOME_GRID_Y1;
const int HOME_TIMER_W = 108;
const int HOME_TIMER_H = HOME_WIDGET_H;
const int TIMER_MENU_X = 20;
const int TIMER_MENU_Y = 68;
const int TIMER_MENU_W = 200;
const int TIMER_MENU_H = 194;
const int TIMER_DONE_X = 26;
const int TIMER_DONE_Y = 92;
const int TIMER_DONE_W = 188;
const int TIMER_DONE_H = 108;
const int PAGE_ROW1_Y = 42;
const int PAGE_ROW2_Y = 120;
const int PAGE_ROW3_Y = 198;
const int PAGE_WIDGET_H = HOME_WIDGET_H;

String buddyNickname = "";

enum HomeWidgetType {
  HOME_WIDGET_WEEK = 0,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_RAIN,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_KP,
  HOME_WIDGET_UV,
  HOME_WIDGET_WIND,
  HOME_WIDGET_SUN,
  HOME_WIDGET_AQI,
  HOME_WIDGET_FORECAST,
  HOME_WIDGET_MINMAX
};

const int HOME_SLOT_COUNT = 4;
HomeWidgetType homeWidgetSlots[HOME_SLOT_COUNT] = {
  HOME_WIDGET_WEEK,
  HOME_WIDGET_TIMER,
  HOME_WIDGET_OUTDOOR,
  HOME_WIDGET_AQI
};

String cacheHomeSlots[HOME_SLOT_COUNT];

// =========================================================
// STATE
// =========================================================
enum Page {
  PAGE_HOME = 0,
  PAGE_WEATHER = 1,
  PAGE_POMO = 2,
  PAGE_STATUS = 3
};

Page currentPage = PAGE_HOME;
Page lastDrawnPage = (Page)-1;

unsigned long lastClockTick = 0;
unsigned long lastDataTick  = 0;

const unsigned long CLOCK_TICK_MS = 1000;
const unsigned long DATA_TICK_MS  = 30UL * 1000UL;

bool pageDirty = true;
bool dataDirty = true;

// cache
String cacheClock = "";
String cacheTemp = "";
String cacheRain = "";
String cacheWeek = "";
String cacheHomeEmpty1 = "";
String cacheHomeEmpty2 = "";
String cacheFocusTimer = "";
String cacheTimerMenu = "";
String cacheTimerDone = "";
String cacheTimerDoneCountdown = "";
String cacheTimerDoneFlash = "";

String lastWifiText = "";
String lastSignalText = "";
String lastIpText = "";
String lastUptimeText = "";
String lastTempText = "";
String lastRainText = "";
String lastUvText = "";
String lastUvLevelText = "";
String lastKpText = "";
String lastKpLevelText = "";
String lastWindText = "";
String lastWindDirText = "";
String lastNextSunLabel = "";
String lastNextSunTime = "";
String lastNetworkToggleText = "";
String lastWeatherPageCache = "";
String lastPomoPageCache = "";
String lastForecastText = "";

const char* homeWidgetKey(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:     return "week";
    case HOME_WIDGET_TIMER:    return "timer";
    case HOME_WIDGET_RAIN:     return "rain";
    case HOME_WIDGET_OUTDOOR:  return "outdoor";
    case HOME_WIDGET_KP:       return "kp";
    case HOME_WIDGET_UV:       return "uv";
    case HOME_WIDGET_WIND:     return "wind";
    case HOME_WIDGET_SUN:      return "sun";
    case HOME_WIDGET_AQI:      return "aqi";
    case HOME_WIDGET_FORECAST: return "forecast";
    case HOME_WIDGET_MINMAX:   return "minmax";
    default:                   return "week";
  }
}

static bool useEn() { return langKey == "en"; }
static const char* tr(const char* es, const char* en) { return useEn() ? en : es; }

const char* homeWidgetLabel(HomeWidgetType type) {
  switch (type) {
    case HOME_WIDGET_WEEK:     return tr("Semana", "Week");
    case HOME_WIDGET_TIMER:    return tr("Timer", "Timer");
    case HOME_WIDGET_RAIN:     return tr("Lluvia", "Rain");
    case HOME_WIDGET_OUTDOOR:  return tr("Exterior", "Outdoor");
    case HOME_WIDGET_KP:       return tr("Indice KP", "KP index");
    case HOME_WIDGET_UV:       return tr("Indice UV", "UV index");
    case HOME_WIDGET_WIND:     return tr("Viento", "Wind");
    case HOME_WIDGET_SUN:      return tr("Sol", "Sun");
    case HOME_WIDGET_AQI:      return tr("Calidad aire", "Air quality");
    case HOME_WIDGET_FORECAST: return tr("Pronostico", "Forecast");
    case HOME_WIDGET_MINMAX:   return tr("Max / Min", "Max / Min");
    default:                   return tr("Semana", "Week");
  }
}

HomeWidgetType homeWidgetFromKey(const String& key) {
  if (key == "week") return HOME_WIDGET_WEEK;
  if (key == "timer") return HOME_WIDGET_TIMER;
  if (key == "rain") return HOME_WIDGET_RAIN;
  if (key == "outdoor") return HOME_WIDGET_OUTDOOR;
  if (key == "kp") return HOME_WIDGET_KP;
  if (key == "uv") return HOME_WIDGET_UV;
  if (key == "wind") return HOME_WIDGET_WIND;
  if (key == "sun") return HOME_WIDGET_SUN;
  if (key == "aqi") return HOME_WIDGET_AQI;
  if (key == "forecast") return HOME_WIDGET_FORECAST;
  if (key == "minmax") return HOME_WIDGET_MINMAX;
  return HOME_WIDGET_WEEK;
}

const char* homeSlotLabel(int slot) {
  switch (slot) {
    case 0: return tr("Arriba izquierda", "Top left");
    case 1: return tr("Arriba derecha", "Top right");
    case 2: return tr("Abajo izquierda", "Bottom left");
    case 3: return tr("Abajo derecha", "Bottom right");
    default: return tr("Hueco", "Slot");
  }
}

struct GeoHit {
  String name;
  String admin1;
  String country;
  String countryCode;
  String timezone;
  float lat;
  float lng;
};

static const int GEO_MAX = 8;
GeoHit geoHits[GEO_MAX];
int geoHitCount = 0;
int geoSelected = -1;
String geoLastQuery = "";
bool geoSearchFailed = false;

const char* timezonePosixByKey(const String& key) {
  if (key == "utc") return "UTC0";
  if (key == "atlantic_azores") return "AZOT1AZOST,M3.5.0/0,M10.5.0/1";
  if (key == "europe_west") return "WET0WEST,M3.5.0/1,M10.5.0";
  if (key == "uk") return "GMT0BST,M3.5.0/1,M10.5.0";
  if (key == "europe_central") return "CET-1CEST,M3.5.0/2,M10.5.0/3";
  if (key == "europe_east") return "EET-2EEST,M3.5.0/3,M10.5.0/4";
  if (key == "africa_south") return "SAST-2";
  if (key == "middle_east_gulf") return "GST-4";
  if (key == "india") return "IST-5:30";
  if (key == "thailand") return "ICT-7";
  if (key == "china") return "CST-8";
  if (key == "us_eastern") return "EST5EDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_central") return "CST6CDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_mountain") return "MST7MDT,M3.2.0/2,M11.1.0/2";
  if (key == "us_arizona") return "MST7";
  if (key == "us_pacific") return "PST8PDT,M3.2.0/2,M11.1.0/2";
  if (key == "alaska") return "AKST9AKDT,M3.2.0/2,M11.1.0/2";
  if (key == "hawaii") return "HST10";
  if (key == "canada_atlantic") return "AST4ADT,M3.2.0/2,M11.1.0/2";
  if (key == "brazil_east") return "BRT3";
  if (key == "argentina") return "ART3";
  if (key == "asia_tokyo") return "JST-9";
  if (key == "korea") return "KST-9";
  if (key == "australia_perth") return "AWST-8";
  if (key == "australia_darwin") return "ACST-9:30";
  if (key == "australia_sydney") return "AEST-10AEDT,M10.1.0,M4.1.0/3";
  if (key == "new_zealand") return "NZST-12NZDT,M9.5.0/2,M4.1.0/3";
  return "CET-1CEST,M3.5.0/2,M10.5.0/3";
}

const char* timezoneLabelByKey(const String& key) {
  if (key == "auto") return tr("Ciudad", "City");
  if (key == "utc") return "UTC";
  if (key == "atlantic_azores") return "Azores";
  if (key == "europe_west") return tr("Europa occidental", "Western Europe");
  if (key == "uk") return tr("Reino Unido", "United Kingdom");
  if (key == "europe_central") return tr("Europa central", "Central Europe");
  if (key == "europe_east") return tr("Europa oriental", "Eastern Europe");
  if (key == "africa_south") return tr("Sudafrica", "South Africa");
  if (key == "middle_east_gulf") return tr("Golfo / EAU", "Gulf / UAE");
  if (key == "india") return tr("India", "India");
  if (key == "thailand") return tr("Tailandia / ICT", "Thailand / ICT");
  if (key == "china") return tr("China", "China");
  if (key == "us_eastern") return tr("EE.UU. Este", "US Eastern");
  if (key == "us_central") return tr("EE.UU. Central", "US Central");
  if (key == "us_mountain") return tr("EE.UU. Montana", "US Mountain");
  if (key == "us_arizona") return tr("EE.UU. Arizona", "US Arizona");
  if (key == "us_pacific") return tr("EE.UU. Pacifico", "US Pacific");
  if (key == "alaska") return "Alaska";
  if (key == "hawaii") return tr("Hawai", "Hawaii");
  if (key == "canada_atlantic") return tr("Canada Atlantico", "Canada Atlantic");
  if (key == "brazil_east") return tr("Brasil Este", "Brazil East");
  if (key == "argentina") return "Argentina";
  if (key == "asia_tokyo") return tr("Japon", "Japan");
  if (key == "korea") return tr("Corea del Sur", "South Korea");
  if (key == "australia_perth") return tr("Australia Oeste", "Australia West");
  if (key == "australia_darwin") return tr("Australia Central", "Australia Central");
  if (key == "australia_sydney") return tr("Australia Este", "Australia East");
  if (key == "new_zealand") return tr("Nueva Zelanda", "New Zealand");
  return tr("Europa central", "Central Europe");
}

String timezoneKeyFromIana(const String& iana) {
  if (!iana.length()) return "";
  if (iana == "UTC" || iana.startsWith("Etc/UTC") || iana == "Etc/GMT") return "utc";
  if (iana == "Atlantic/Azores") return "atlantic_azores";
  if (iana == "Europe/London" || iana == "Europe/Dublin") return "uk";
  if (iana == "Europe/Lisbon" || iana.startsWith("Atlantic/Canary") || iana == "Atlantic/Madeira") return "europe_west";
  if (iana.startsWith("Europe/")) {
    if (iana.indexOf("Athens") >= 0 || iana.indexOf("Helsinki") >= 0 || iana.indexOf("Bucharest") >= 0 ||
        iana.indexOf("Kyiv") >= 0 || iana.indexOf("Kiev") >= 0 || iana.indexOf("Sofia") >= 0 ||
        iana.indexOf("Istanbul") >= 0 || iana.indexOf("Riga") >= 0 || iana.indexOf("Tallinn") >= 0 ||
        iana.indexOf("Vilnius") >= 0 || iana.indexOf("Chisinau") >= 0) {
      return "europe_east";
    }
    return "europe_central";
  }
  if (iana == "Africa/Johannesburg") return "africa_south";
  if (iana == "Asia/Dubai" || iana == "Asia/Muscat" || iana == "Asia/Qatar" || iana == "Asia/Bahrain") return "middle_east_gulf";
  if (iana == "Asia/Kolkata" || iana == "Asia/Calcutta") return "india";
  if (iana == "Asia/Bangkok" || iana == "Asia/Jakarta" || iana == "Asia/Ho_Chi_Minh") return "thailand";
  if (iana == "Asia/Shanghai" || iana == "Asia/Hong_Kong" || iana == "Asia/Singapore" || iana == "Asia/Taipei" || iana == "Asia/Manila") return "china";
  if (iana == "Asia/Tokyo") return "asia_tokyo";
  if (iana == "Asia/Seoul") return "korea";
  if (iana == "America/Phoenix") return "us_arizona";
  if (iana == "America/New_York" || iana == "America/Toronto" || iana == "America/Detroit" || iana.indexOf("Indiana") >= 0) return "us_eastern";
  if (iana == "America/Chicago" || iana == "America/Winnipeg" || iana == "America/Mexico_City") return "us_central";
  if (iana == "America/Denver" || iana == "America/Edmonton" || iana == "America/Boise") return "us_mountain";
  if (iana == "America/Los_Angeles" || iana == "America/Vancouver" || iana == "America/Tijuana") return "us_pacific";
  if (iana == "America/Anchorage") return "alaska";
  if (iana == "Pacific/Honolulu") return "hawaii";
  if (iana == "America/Halifax") return "canada_atlantic";
  if (iana == "America/Sao_Paulo") return "brazil_east";
  if (iana.startsWith("America/Argentina") || iana == "America/Buenos_Aires") return "argentina";
  if (iana == "Australia/Perth") return "australia_perth";
  if (iana == "Australia/Darwin") return "australia_darwin";
  if (iana == "Australia/Sydney" || iana == "Australia/Melbourne" || iana == "Australia/Brisbane" || iana == "Australia/Hobart") return "australia_sydney";
  if (iana == "Pacific/Auckland") return "new_zealand";
  return "";
}

const char* posixFromIana(const String& iana) {
  String key = timezoneKeyFromIana(iana);
  if (key.length()) return timezonePosixByKey(key);
  if (iana == "America/Santiago") return "CLT4CLST,M9.1.0/0,M4.1.0/0";
  if (iana == "America/Bogota") return "COT5";
  if (iana == "America/Lima") return "PET5";
  if (iana == "America/Caracas") return "VET4";
  if (iana == "America/Montevideo") return "UYT3";
  if (iana == "Europe/Moscow") return "MSK-3";
  if (iana == "Africa/Cairo") return "EET-2";
  if (iana == "Africa/Lagos") return "WAT-1";
  if (iana == "Asia/Riyadh") return "AST-3";
  if (iana == "Asia/Tehran") return "IRST-3:30";
  if (iana == "Asia/Karachi") return "PKT-5";
  if (iana == "Pacific/Fiji") return "FJT-12";
  return nullptr;
}

void applyTimezoneFromIana(const String& iana) {
  if (!iana.length()) return;
  timezoneIana = iana;
  String key = timezoneKeyFromIana(iana);
  const char* posix = posixFromIana(iana);
  if (key.length()) {
    timezonePosixCustom = "";
    timezoneKey = key;
    setenv("TZ", timezonePosixByKey(timezoneKey), 1);
    tzset();
    return;
  }
  if (posix) {
    timezoneKey = "auto";
    timezonePosixCustom = posix;
    setenv("TZ", posix, 1);
    tzset();
  }
}

String sanitizeTimezoneKey(const String& key) {
  const char* supported[] = {
    "auto", "utc", "atlantic_azores", "europe_west", "uk", "europe_central", "europe_east",
    "africa_south", "middle_east_gulf", "india", "thailand", "china",
    "us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific",
    "alaska", "hawaii", "canada_atlantic", "brazil_east", "argentina",
    "asia_tokyo", "korea", "australia_perth", "australia_darwin",
    "australia_sydney", "new_zealand"
  };
  for (const char* supportedKey : supported) {
    if (key == supportedKey) return key;
  }
  return "europe_central";
}

void applyDeviceTimezoneByKey(const String& key) {
  timezoneKey = sanitizeTimezoneKey(key);
  if (timezoneKey != "auto") timezonePosixCustom = "";
  const char* posix = (timezoneKey == "auto" && timezonePosixCustom.length())
    ? timezonePosixCustom.c_str()
    : timezonePosixByKey(timezoneKey == "auto" ? "europe_central" : timezoneKey);
  setenv("TZ", posix, 1);
  tzset();
}

void appendTimezoneOptions(String& page, const String& selectedKey) {
  if (selectedKey == "auto") {
    page += "<option value='auto' selected>";
    page += timezoneIana.length() ? timezoneIana : String(tr("Zona de la ciudad", "City timezone"));
    page += "</option>";
  }

  struct TimezoneGroup {
    const char* label;
    const char* keys[8];
    int count;
  };

  const TimezoneGroup groups[] = {
    {tr("Global", "Global"), {"utc"}, 1},
    {tr("Europa", "Europe"), {"atlantic_azores", "europe_west", "uk", "europe_central", "europe_east"}, 5},
    {tr("Africa y Oriente Medio", "Africa and Middle East"), {"africa_south", "middle_east_gulf"}, 2},
    {tr("Asia", "Asia"), {"india", "thailand", "china", "asia_tokyo", "korea"}, 5},
    {tr("Norteamerica", "North America"), {"us_eastern", "us_central", "us_mountain", "us_arizona", "us_pacific", "alaska", "hawaii", "canada_atlantic"}, 8},
    {tr("Sudamerica", "South America"), {"brazil_east", "argentina"}, 2},
    {tr("Australia y Oceania", "Australia and Oceania"), {"australia_perth", "australia_darwin", "australia_sydney", "new_zealand"}, 4}
  };

  for (const TimezoneGroup& group : groups) {
    page += "<optgroup label='";
    page += group.label;
    page += "'>";
    for (int i = 0; i < group.count; i++) {
      const char* key = group.keys[i];
      page += "<option value='";
      page += key;
      page += "'";
      if (selectedKey == key) page += " selected";
      page += ">";
      page += timezoneLabelByKey(key);
      page += "</option>";
    }
    page += "</optgroup>";
  }
}

void getHomeSlotRect(int slot, int& x, int& y, int& w, int& h) {
  const int xs[HOME_SLOT_COUNT] = {8, 124, 8, 124};
  const int ys[HOME_SLOT_COUNT] = {HOME_GRID_Y1, HOME_GRID_Y1, HOME_GRID_Y2, HOME_GRID_Y2};
  x = xs[slot];
  y = ys[slot];
  w = 108;
  h = HOME_WIDGET_H;
}

void appendHomeWidgetOptions(String& page, const String& selectedKey) {
  const HomeWidgetType types[] = {
    HOME_WIDGET_WEEK,
    HOME_WIDGET_TIMER,
    HOME_WIDGET_RAIN,
    HOME_WIDGET_OUTDOOR,
    HOME_WIDGET_MINMAX,
    HOME_WIDGET_AQI,
    HOME_WIDGET_FORECAST,
    HOME_WIDGET_KP,
    HOME_WIDGET_UV,
    HOME_WIDGET_WIND,
    HOME_WIDGET_SUN
  };

  for (HomeWidgetType type : types) {
    const char* key = homeWidgetKey(type);
    page += "<option value='";
    page += key;
    page += "'";
    if (selectedKey == key) page += " selected";
    page += ">";
    page += homeWidgetLabel(type);
    page += "</option>";
  }
}

void clearHomeSlotCaches() {
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }
}

// Focus / Pomodoro timer (one engine)
enum TimerKind { TIMER_SIMPLE = 0, TIMER_POMO };
enum PomoPhase { POMO_WORK = 0, POMO_SHORT, POMO_LONG };

bool focusMenuOpen = false;
bool focusTimerRunning = false;
bool focusTimerFinished = false;
bool focusTimerPaused = false;
unsigned long focusEndMs = 0;
unsigned long focusDurationSec = 0;
unsigned long focusRemainingSec = 0;
TimerKind timerKind = TIMER_SIMPLE;
PomoPhase pomoPhase = POMO_WORK;
int pomoWorksDone = 0;
int pomoWorkMin = 25;
int pomoShortMin = 5;
int pomoLongMin = 15;
PomoPhase pomoEndedPhase = POMO_WORK;
bool timerDoneDialogOpen = false;
unsigned long timerDoneDialogStartedMs = 0;
const unsigned long TIMER_DONE_DIALOG_MS = 60UL * 1000UL;
bool flashModeEnabled = false;
int timerPresetMin[6] = {1, 5, 10, 15, 25, 30};
bool wifiEnabled = true;
bool wifiConnectInProgress = false;
unsigned long wifiConnectStartedMs = 0;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 25000UL;

// Weather
static float tempC = NAN;
static float tempMinC = NAN;
static float tempMaxC = NAN;
static float precipMm = NAN;
static float windSpeedMs = NAN;
static float windDirectionDeg = NAN;
static float uvIndex = NAN;
static time_t lastWeatherFetch = 0;
static const uint32_t WEATHER_INTERVAL_SEC = 10 * 60;

static const int FORECAST_DAYS = 6;
struct ForecastDay {
  float tmax;
  float tmin;
  float precipMm;
  float precipProb;
  int weatherCode;
  int wday;
};
static ForecastDay forecastDays[FORECAST_DAYS];
static int forecastCount = 0;

static float aqiEuropean = NAN;
static float aqiUs = NAN;
static time_t lastAqiFetch = 0;

// KP-index
static float kpIndex = NAN;
static time_t lastKpFetch = 0;
static const uint32_t KP_INTERVAL_SEC = 10 * 60;

// Sunrise / Sunset
static int sunriseMin = -1;
static int sunsetMin  = -1;
static int lastSunYmd = -1;
static time_t lastSyncTime = 0;

// =========================================================
// SLEEP / BACKLIGHT
// =========================================================
const int BACKLIGHT_PIN = 21;

bool sleepDimmed = false;
bool sleepOff = false;
bool manualDimMode = false;

unsigned long lastInteractionMs = 0;

int sleepIntervalMin = 10;
int sleepOffDelaySec = 60;

const int BL_FULL = 255;
const int BL_DIM  = 18;
const int BL_OFF  = 0;
const int FLASH_BL_LOW = 20;
const int FLASH_BL_HIGH = 255;

void wakeDisplay(bool clearManualMode = true);

int sanitizeTimerMinutes(int value);

// =========================================================
// HELPERS
// =========================================================
static int ymdFromLocal(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return (tmLocal.tm_year + 1900) * 10000 + (tmLocal.tm_mon + 1) * 100 + tmLocal.tm_mday;
}

static int minutesFromLocalEpoch(time_t t) {
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  return tmLocal.tm_hour * 60 + tmLocal.tm_min;
}

static int minutesNowLocal() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  return tmNow.tm_hour * 60 + tmNow.tm_min;
}

static String wifiStatusText() {
  if (!wifiEnabled) return tr("Apagado", "Off");
  if (wifiApMode) return tr("Modo setup", "Setup mode");
  return WiFi.status() == WL_CONNECTED ? tr("En linea", "Online") : tr("Sin red", "No network");
}

static String signalText() {
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-- dBm";
  return String(WiFi.RSSI()) + " dBm";
}

static String ipText() {
  if (wifiApMode) return WiFi.softAPIP().toString();
  if (!wifiEnabled || WiFi.status() != WL_CONNECTED) return "-";
  return WiFi.localIP().toString();
}

static bool useUsRegionFormat() {
  return regionFormatKey == "us";
}

static String formatClockParts(const struct tm& tmValue, bool withSeconds) {
  char buf[20];
  const char* pattern = useUsRegionFormat()
    ? (withSeconds ? "%I:%M:%S %p" : "%I:%M %p")
    : (withSeconds ? "%H:%M:%S" : "%H:%M");
  strftime(buf, sizeof(buf), pattern, &tmValue);
  return String(buf);
}

static String formatDateParts(const struct tm& tmValue) {
  static const char* daysEs[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
  static const char* daysEn[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char** days = useEn() ? daysEn : daysEs;
  char buf[32];
  snprintf(buf, sizeof(buf), useUsRegionFormat() ? "%s %02d/%02d/%04d" : "%s %02d.%02d.%04d",
           days[tmValue.tm_wday % 7],
           useUsRegionFormat() ? tmValue.tm_mon + 1 : tmValue.tm_mday,
           useUsRegionFormat() ? tmValue.tm_mday : tmValue.tm_mon + 1,
           tmValue.tm_year + 1900);
  return String(buf);
}

static String formatMinuteOfDay(int minOfDay) {
  if (minOfDay < 0) return "--:--";
  if (useUsRegionFormat()) {
    int hour24 = minOfDay / 60;
    int minute = minOfDay % 60;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "PM" : "AM");
    return String(buf);
  }
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", minOfDay / 60, minOfDay % 60);
  return String(buf);
}

static String weatherCityShort() {
  String name = locationName;
  int comma = name.indexOf(',');
  if (comma > 0) name = name.substring(0, comma);
  name.trim();
  if (!name.length()) name = tr("Exterior", "Outdoor");
  if (name.length() > 14) {
    name = name.substring(0, 13);
    name += ".";
  }
  return name;
}

static String tempText() {
  if (isnan(tempC)) return unitKey == "imperial" ? "--.-F" : "--.-C";

  if (unitKey == "imperial") {
    float f = tempC * 9.0f / 5.0f + 32.0f;
    return String(f, 1) + "F";
  }

  return String(tempC, 1) + "C";
}

static String formatDisplayTemp(float value) {
  if (isnan(value)) return "--";

  if (unitKey == "imperial") {
    float f = value * 9.0f / 5.0f + 32.0f;
    return String((int)roundf(f)) + "F";
  }

  return String((int)roundf(value)) + "C";
}

static String tempRangeText() {
  return String(tr("Max:", "Max:")) + formatDisplayTemp(tempMaxC) + "  " + tr("Min:", "Min:") + formatDisplayTemp(tempMinC);
}

static String formatTempNumber(float value) {
  if (isnan(value)) return "--";
  if (unitKey == "imperial") return String((int)roundf(value * 9.0f / 5.0f + 32.0f));
  return String((int)roundf(value));
}

static String minMaxValueText() {
  return formatDisplayTemp(tempMaxC);
}

static String minMaxDetailText() {
  return String(tr("Min ", "Min ")) + formatDisplayTemp(tempMinC);
}

static const char* weekdayShort(int wday) {
  static const char* daysEs[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
  static const char* daysEn[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (wday < 0 || wday > 6) return "--";
  return useEn() ? daysEn[wday] : daysEs[wday];
}

static const char* weatherCodeShort(int code) {
  if (code < 0) return "--";
  if (code <= 1) return tr("Sol", "Sun");
  if (code <= 3) return tr("Nub", "Cld");
  if (code <= 48) return tr("Nie", "Fog");
  if (code <= 67) return tr("Llu", "Rn");
  if (code <= 77) return tr("Nev", "Sn");
  if (code <= 82) return tr("Chu", "Shw");
  return tr("Tru", "Thn");
}

static String aqiValueText() {
  if (!isnan(aqiEuropean)) return String((int)roundf(aqiEuropean));
  if (!isnan(aqiUs)) return String((int)roundf(aqiUs));
  return "--";
}

static String aqiLevelText() {
  float v = !isnan(aqiEuropean) ? aqiEuropean : aqiUs;
  if (isnan(v)) return "--";
  if (v < 40.0f) return tr("Buena", "Good");
  if (v < 80.0f) return tr("Moderada", "Moderate");
  return tr("Mala", "Poor");
}

static String forecastSnippetText() {
  if (forecastCount < 2) return "--";
  return formatTempNumber(forecastDays[1].tmax) + "/" + formatTempNumber(forecastDays[1].tmin);
}

static String forecastSnippetDetail() {
  if (forecastCount < 2) return tr("Manana", "Tomorrow");
  int prob = isnan(forecastDays[1].precipProb) ? 0 : (int)roundf(forecastDays[1].precipProb);
  return String(tr("Manana ", "Tmrw ")) + String(prob) + "%";
}

static String rainText() {
  if (isnan(precipMm)) return unitKey == "imperial" ? "--.--in" : "--.-mm";

  if (unitKey == "imperial") {
    float inches = precipMm / 25.4f;
    return String(inches, 2) + "in";
  }

  return String(precipMm, 1) + "mm";
}

static String windText() {
  if (isnan(windSpeedMs)) return unitKey == "imperial" ? "--.-mph" : "--.-m/s";

  if (unitKey == "imperial") {
    float mph = windSpeedMs * 2.236936f;
    return String(mph, 1) + "mph";
  }

  return String(windSpeedMs, 1) + "m/s";
}

static String windDirectionText() {
  if (isnan(windDirectionDeg)) return "--";

  const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = (int)roundf(windDirectionDeg / 45.0f) % 8;
  return String(dirs[idx]) + " " + String((int)roundf(windDirectionDeg)) + "deg";
}

static String kpText() {
  return isnan(kpIndex) ? "Kp --" : "Kp " + String(kpIndex, 1);
}

static String kpLevelText() {
  if (isnan(kpIndex)) return "--";
  if (kpIndex < 3.0f) return tr("Bajo", "Low");
  if (kpIndex < 5.0f) return tr("Medio", "Medium");
  if (kpIndex < 7.0f) return tr("Alto", "High");
  return tr("Extremo", "Extreme");
}

static String uvText() {
  return isnan(uvIndex) ? "UV --" : "UV " + String(uvIndex, 1);
}

static String uvLevelText() {
  if (isnan(uvIndex)) return "--";
  if (uvIndex < 3.0f) return tr("Bajo", "Low");
  if (uvIndex < 6.0f) return tr("Moderado", "Moderate");
  if (uvIndex < 8.0f) return tr("Alto", "High");
  if (uvIndex < 11.0f) return tr("Muy alto", "Very high");
  return tr("Extremo", "Extreme");
}

static uint16_t statusColor() {
  if (textColorKey != "standard") return COL_TEXT;
  if (!wifiEnabled) return COL_YELLOW;
  if (wifiApMode) return COL_YELLOW;
  return WiFi.status() == WL_CONNECTED ? COL_GREEN : COL_RED;
}

static String uptimeText() {
  unsigned long seconds = millis() / 1000UL;
  unsigned long days = seconds / 86400UL;
  seconds %= 86400UL;
  unsigned long hours = seconds / 3600UL;
  seconds %= 3600UL;
  unsigned long minutes = seconds / 60UL;

  if (days > 0) return String(days) + "d " + String(hours) + "h";
  if (hours > 0) return String(hours) + "h " + String(minutes) + "m";
  return String(minutes) + "m";
}

static String nextSunLabel() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return tr("Sol", "Sun");
  if (nowMin < sunriseMin) return tr("Amanecer", "Sunrise");
  if (nowMin < sunsetMin) return tr("Atardecer", "Sunset");
  return tr("Amanecer", "Sunrise");
}

static String nextSunTimeText() {
  int nowMin = minutesNowLocal();
  if (sunriseMin < 0 || sunsetMin < 0) return "--:--";
  if (nowMin < sunriseMin) return formatMinuteOfDay(sunriseMin);
  if (nowMin < sunsetMin) return formatMinuteOfDay(sunsetMin);
  return formatMinuteOfDay(sunriseMin);
}

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else if (c == '"') out += "&quot;";
    else out += c;
  }
  return out;
}

static String urlEncodeQuery(const String& s) {
  String out;
  out.reserve(s.length() * 3);
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else if (c == ' ') {
      out += "%20";
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

static String cssColorFrom565(uint16_t color) {
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

static String accentPreviewCss(const String& key) {
  if (key == "standard") return cssColorFrom565(0xEF7D);
  if (key == "ice")      return cssColorFrom565(0xEFFF);
  if (key == "white")    return cssColorFrom565(TFT_WHITE);
  if (key == "cyan")     return cssColorFrom565(0x5EFA);
  if (key == "mint")     return cssColorFrom565(0x07F0);
  if (key == "green")    return cssColorFrom565(TFT_GREEN);
  if (key == "blue")     return cssColorFrom565(0x3D9F);
  if (key == "purple")   return cssColorFrom565(0xA2F5);
  if (key == "pink")     return cssColorFrom565(0xF97F);
  if (key == "orange")   return cssColorFrom565(0xFD20);
  if (key == "amber")    return cssColorFrom565(0xFEA0);
  if (key == "red")      return cssColorFrom565(TFT_RED);
  return cssColorFrom565(0xEF7D);
}

static String themePreviewCss(const String& key) {
  if (key == "slate")    return cssColorFrom565(0x08A3);
  if (key == "deep")     return cssColorFrom565(0x0000);
  if (key == "nordic")   return cssColorFrom565(0x0864);
  if (key == "forest")   return cssColorFrom565(0x0208);
  if (key == "coffee")   return cssColorFrom565(0x18A3);
  if (key == "soft")     return cssColorFrom565(0x10A2);
  if (key == "midnight") return cssColorFrom565(0x0008);
  if (key == "graphite") return cssColorFrom565(0x1082);
  if (key == "garnet")   return cssColorFrom565(0x1004);
  if (key == "ochre")    return cssColorFrom565(0x20E1);
  return cssColorFrom565(0x08A3);
}

static String formatTimerClock(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  unsigned long seconds = totalSec % 60UL;

  char buf[10];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", minutes, seconds);
  return String(buf);
}

static const char* pomoPhaseLabel() {
  if (pomoPhase == POMO_SHORT) return tr("Descanso", "Break");
  if (pomoPhase == POMO_LONG) return tr("Largo", "Long break");
  return tr("Foco", "Focus");
}

static int pomoMinutesFor(PomoPhase phase) {
  if (phase == POMO_SHORT) return pomoShortMin;
  if (phase == POMO_LONG) return pomoLongMin;
  return pomoWorkMin;
}

static String pomoCycleText() {
  int shown = constrain(pomoWorksDone, 0, 4);
  return String(shown) + "/4";
}

static String focusHintText() {
  if (timerKind == TIMER_POMO) {
    String hint = String(pomoPhaseLabel()) + " " + pomoCycleText();
    if (focusTimerFinished) return tr("Listo", "Done");
    if (focusTimerPaused) return String(tr("Pausa ", "Paused ")) + hint;
    if (focusTimerRunning) return hint;
    return hint;
  }
  if (focusTimerFinished) return tr("Toca para reset", "Tap to reset");
  if (focusTimerPaused) return tr("En pausa", "Paused");
  if (focusTimerRunning) return String((focusDurationSec / 60UL)) + " min";
  return tr("Toca para iniciar", "Tap to start");
}

static String formatElapsedText(unsigned long totalSec) {
  unsigned long minutes = totalSec / 60UL;
  if (minutes == 0) return tr("< 1 min transcurrido", "< 1 min elapsed");
  if (minutes == 1) return tr("1 min transcurrido", "1 min elapsed");
  return String(minutes) + tr(" min transcurridos", " min elapsed");
}

static String lastSyncText() {
  if (lastSyncTime <= 0) return "Sync --:--";

  struct tm tmSync;
  localtime_r(&lastSyncTime, &tmSync);
  return "Sync " + formatClockParts(tmSync, false);
}

static String weekNumberText() {
  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);
  char buf[4];
  strftime(buf, sizeof(buf), "%V", &tmNow);
  return String(buf);
}

static String timerDoneCountdownText() {
  if (!timerDoneDialogOpen) return "";

  unsigned long elapsedMs = millis() - timerDoneDialogStartedMs;
  unsigned long remainingMs = (elapsedMs >= TIMER_DONE_DIALOG_MS) ? 0 : (TIMER_DONE_DIALOG_MS - elapsedMs);
  unsigned long remainingSec = (remainingMs + 999UL) / 1000UL;
  return String(tr("Cierra en ", "Closes in ")) + String(remainingSec) + "s";
}

static String homeTitleText() {
  return buddyNickname.length() > 0 ? buddyNickname : "Deskbuddy";
}

int sanitizeTimerMinutes(int value) {
  return constrain(value, 1, 180);
}

int sanitizePomoWorkMin(int value) { return constrain(value, 5, 90); }
int sanitizePomoShortMin(int value) { return constrain(value, 1, 30); }
int sanitizePomoLongMin(int value) { return constrain(value, 5, 45); }

void resetFocusTimer() {
  focusTimerRunning = false;
  focusTimerFinished = false;
  focusTimerPaused = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  focusEndMs = 0;
  focusDurationSec = 0;
  focusRemainingSec = 0;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  lastPomoPageCache = "";
}

void armPomoPhase() {
  timerKind = TIMER_POMO;
  focusDurationSec = (unsigned long)pomoMinutesFor(pomoPhase) * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = 0;
  focusTimerRunning = false;
  focusTimerPaused = false;
  focusTimerFinished = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  lastPomoPageCache = "";
}

void advancePomoPhase() {
  if (pomoPhase == POMO_WORK) {
    pomoWorksDone++;
    pomoPhase = (pomoWorksDone >= 4) ? POMO_LONG : POMO_SHORT;
  } else {
    if (pomoPhase == POMO_LONG) pomoWorksDone = 0;
    pomoPhase = POMO_WORK;
  }
}

void resetPomoSession() {
  timerKind = TIMER_POMO;
  pomoPhase = POMO_WORK;
  pomoWorksDone = 0;
  resetFocusTimer();
  armPomoPhase();
}

void startFocusTimer(unsigned long minutes, TimerKind kind = TIMER_SIMPLE) {
  timerKind = kind;
  focusTimerPaused = false;
  focusDurationSec = minutes * 60UL;
  focusRemainingSec = focusDurationSec;
  focusEndMs = millis() + (focusDurationSec * 1000UL);
  focusTimerRunning = true;
  focusTimerFinished = false;
  focusMenuOpen = false;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  lastPomoPageCache = "";
}

void pauseFocusTimer() {
  if (!focusTimerRunning) return;
  unsigned long now = millis();
  if ((long)(focusEndMs - now) > 0) {
    focusRemainingSec = (focusEndMs - now + 999UL) / 1000UL;
  } else {
    focusRemainingSec = 0;
  }
  focusTimerRunning = false;
  focusTimerPaused = true;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  lastPomoPageCache = "";
}

void resumeFocusTimer() {
  if (focusRemainingSec == 0) return;
  focusEndMs = millis() + (focusRemainingSec * 1000UL);
  focusTimerRunning = true;
  focusTimerPaused = false;
  focusTimerFinished = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  lastPomoPageCache = "";
}

void stopFocusTimer() {
  focusTimerRunning = false;
  focusTimerPaused = false;
  focusTimerFinished = false;
  focusEndMs = 0;
  timerDoneDialogOpen = false;
  cacheFocusTimer = "";
  clearHomeSlotCaches();
  cacheTimerMenu = "";
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  lastPomoPageCache = "";
  if (timerKind == TIMER_POMO) {
    armPomoPhase();
  } else {
    focusDurationSec = 0;
    focusRemainingSec = 0;
  }
}

void skipPomoPhase() {
  timerKind = TIMER_POMO;
  timerDoneDialogOpen = false;
  pomoEndedPhase = pomoPhase;
  advancePomoPhase();
  armPomoPhase();
}

void dismissTimerDoneDialog() {
  if (!timerDoneDialogOpen) return;
  timerDoneDialogOpen = false;
  timerDoneDialogStartedMs = 0;
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  cacheTimerDoneFlash = "";
  if (timerKind == TIMER_POMO) {
    focusTimerFinished = false;
    armPomoPhase();
  }
  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);
  pageDirty = true;
}

void openTimerDoneDialog() {
  timerDoneDialogOpen = true;
  timerDoneDialogStartedMs = millis();
  cacheTimerDone = "";
  cacheTimerDoneCountdown = "";
  wakeDisplay();
}

void updateFocusTimerState() {
  if (!focusTimerRunning) return;

  unsigned long now = millis();
  if ((long)(focusEndMs - now) <= 0) {
    focusRemainingSec = 0;
    focusTimerRunning = false;
    focusTimerPaused = false;
    focusTimerFinished = true;
    focusMenuOpen = false;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
    cacheTimerMenu = "";
    lastPomoPageCache = "";
    if (timerKind == TIMER_POMO) {
      pomoEndedPhase = pomoPhase;
      advancePomoPhase();
    }
    openTimerDoneDialog();
    return;
  }

  unsigned long remainingMs = focusEndMs - now;
  unsigned long nextRemainingSec = (remainingMs + 999UL) / 1000UL;
  if (nextRemainingSec != focusRemainingSec) {
    focusRemainingSec = nextRemainingSec;
    cacheFocusTimer = "";
    clearHomeSlotCaches();
  }
}

void updateTimerDoneDialogState() {
  if (!timerDoneDialogOpen) return;
  if (millis() - timerDoneDialogStartedMs >= TIMER_DONE_DIALOG_MS) {
    dismissTimerDoneDialog();
  }
}

void setBacklight(int value) {
  value = constrain(value, 0, 255);
  analogWrite(BACKLIGHT_PIN, value);
}

void wakeDisplay(bool clearManualMode) {
  sleepDimmed = false;
  sleepOff = false;
  if (clearManualMode) manualDimMode = false;
  lastInteractionMs = millis();
  setBacklight(BL_FULL);
  pageDirty = true;
}

void enterSleepDim() {
  if (sleepDimmed || sleepOff || manualDimMode) return;
  sleepDimmed = true;
  setBacklight(BL_DIM);
}

void enterSleepOff() {
  if (sleepOff) return;
  sleepOff = true;
  sleepDimmed = true;
  setBacklight(BL_OFF);
  pageDirty = true;
}

void toggleSleepMode() {
  if (manualDimMode) {
    wakeDisplay();
    return;
  }

  manualDimMode = true;
  sleepDimmed = true;
  sleepOff = false;
  setBacklight(BL_DIM);
  pageDirty = true;
}

void handleAutoSleep() {
  if (focusMenuOpen || timerDoneDialogOpen) return;
  if (sleepIntervalMin <= 0) return;

  unsigned long now = millis();
  unsigned long dimAfterMs = (unsigned long)sleepIntervalMin * 60UL * 1000UL;
  unsigned long offAfterMs = dimAfterMs + ((unsigned long)sleepOffDelaySec * 1000UL);

  if (!sleepDimmed && !sleepOff && now - lastInteractionMs > dimAfterMs) {
    enterSleepDim();
  }

  if (sleepDimmed && !sleepOff && now - lastInteractionMs > offAfterMs) {
    enterSleepOff();
  }
}

// =========================================================
// THEME / SETTINGS
// =========================================================
void applyThemeByKey(const String& accentKey, const String& bgKey) {
  if (accentKey == "standard")    COL_ACCENT = 0xEF7D;
  else if (accentKey == "cyan")   COL_ACCENT = 0x5EFA;
  else if (accentKey == "ice")    COL_ACCENT = 0xEFFF;
  else if (accentKey == "white")  COL_ACCENT = TFT_WHITE;
  else if (accentKey == "mint")   COL_ACCENT = 0x07F0;
  else if (accentKey == "green")  COL_ACCENT = TFT_GREEN;
  else if (accentKey == "blue")   COL_ACCENT = 0x3D9F;
  else if (accentKey == "purple") COL_ACCENT = 0xA2F5;
  else if (accentKey == "pink")   COL_ACCENT = 0xF97F;
  else if (accentKey == "orange") COL_ACCENT = 0xFD20;
  else if (accentKey == "amber")  COL_ACCENT = 0xFEA0;
  else if (accentKey == "red")    COL_ACCENT = TFT_RED;
  else                            COL_ACCENT = 0x5EFA;

  if (bgKey == "slate") {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  } else if (bgKey == "deep") {
    COL_BG = 0x0000; COL_PANEL = 0x0841; COL_PANEL_ALT = 0x1082; COL_STROKE = 0x2945;
  } else if (bgKey == "nordic") {
    COL_BG = 0x0864; COL_PANEL = 0x10C6; COL_PANEL_ALT = 0x1908; COL_STROKE = 0x3A2D;
  } else if (bgKey == "forest") {
    COL_BG = 0x0208; COL_PANEL = 0x0ACB; COL_PANEL_ALT = 0x134D; COL_STROKE = 0x2D72;
  } else if (bgKey == "coffee") {
    COL_BG = 0x18A3; COL_PANEL = 0x2945; COL_PANEL_ALT = 0x39C7; COL_STROKE = 0x5A89;
  } else if (bgKey == "soft") {
    COL_BG = 0x10A2; COL_PANEL = 0x1924; COL_PANEL_ALT = 0x2145; COL_STROKE = 0x3A49;
  } else if (bgKey == "midnight") {
    COL_BG = 0x0008; COL_PANEL = 0x0011; COL_PANEL_ALT = 0x0018; COL_STROKE = 0x3A7F;
  } else if (bgKey == "graphite") {
    COL_BG = 0x1082; COL_PANEL = 0x18C3; COL_PANEL_ALT = 0x2104; COL_STROKE = 0x4208;
  } else if (bgKey == "garnet") {
    COL_BG = 0x1004; COL_PANEL = 0x1886; COL_PANEL_ALT = 0x20E8; COL_STROKE = 0x41AC;
  } else if (bgKey == "ochre") {
    COL_BG = 0x20E1; COL_PANEL = 0x3184; COL_PANEL_ALT = 0x4226; COL_STROKE = 0x632B;
  } else {
    COL_BG = 0x08A3; COL_PANEL = 0x1106; COL_PANEL_ALT = 0x18C7; COL_STROKE = 0x31EC;
  }
}

void applyTextColorByKey(const String& key) {
  textColorKey = key;

  if (key == "standard") {
    COL_TEXT = 0xEF7D; COL_DIM  = 0x94B2;
  } else if (key == "white") {
    COL_TEXT = TFT_WHITE; COL_DIM = 0xBDF7;
  } else if (key == "ice") {
    COL_TEXT = 0xEFFF; COL_DIM = 0x9D7F;
  } else if (key == "mint") {
    COL_TEXT = 0x07F0; COL_DIM = 0x05EC;
  } else if (key == "orange") {
    COL_TEXT = 0xFD20; COL_DIM = 0xBA26;
  } else if (key == "amber") {
    COL_TEXT = 0xFEA0; COL_DIM = 0xBCE0;
  } else if (key == "green") {
    COL_TEXT = TFT_GREEN; COL_DIM = 0x86E8;
  } else if (key == "cyan") {
    COL_TEXT = 0x5EFA; COL_DIM = 0x3D96;
  } else if (key == "blue") {
    COL_TEXT = 0x3D9F; COL_DIM = 0x22B1;
  } else if (key == "purple") {
    COL_TEXT = 0xA2F5; COL_DIM = 0x79ED;
  } else if (key == "red") {
    COL_TEXT = TFT_RED; COL_DIM = 0xB9E7;
  } else if (key == "pink") {
    COL_TEXT = 0xF97F; COL_DIM = 0xC2F1;
  } else {
    COL_TEXT = 0xEF7D;
    COL_DIM  = 0x94B2;
    textColorKey = "standard";
  }
}

void loadStoredSettings() {
  prefs.begin("deskbuddy", false);

  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");

  buddyNickname    = prefs.getString("nickname", "");
  locationName     = prefs.getString("locname", "Berlin");
  LAT              = prefs.getFloat("lat", 52.5200f);
  LNG              = prefs.getFloat("lng", 13.4050f);
  sleepIntervalMin = prefs.getInt("sleepMin", 10);
  unitKey          = prefs.getString("units", "metric");
  regionFormatKey  = prefs.getString("region", "europe");
  timezoneKey      = sanitizeTimezoneKey(prefs.getString("tz", "europe_central"));
  timezoneIana     = prefs.getString("tziana", "");
  timezonePosixCustom = prefs.getString("tzposix", "");
  langKey          = prefs.getString("lang", "es");
  if (langKey != "en") langKey = "es";
  flashModeEnabled = prefs.getBool("flashMode", false);
  wifiEnabled      = prefs.getBool("wifiEnabled", true);
  wifiSsid         = prefs.getString("wifiSsid", "");
  wifiPass         = prefs.getString("wifiPass", "");
  if (!wifiSsid.length()) {
    wifiSsid = WIFI_SSID;
    wifiPass = WIFI_PASS;
  }

  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    homeWidgetSlots[i] = homeWidgetFromKey(prefs.getString(key.c_str(), homeWidgetKey(homeWidgetSlots[i])));
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    timerPresetMin[i] = sanitizeTimerMinutes(prefs.getInt(key.c_str(), timerPresetMin[i]));
  }

  pomoWorkMin  = sanitizePomoWorkMin(prefs.getInt("pomoWork", 25));
  pomoShortMin = sanitizePomoShortMin(prefs.getInt("pomoShort", 5));
  pomoLongMin  = sanitizePomoLongMin(prefs.getInt("pomoLong", 15));

  if (unitKey != "metric" && unitKey != "imperial") unitKey = "metric";
  if (regionFormatKey != "europe" && regionFormatKey != "us") regionFormatKey = "europe";
  buddyNickname.trim();
  applyThemeByKey(accent, bg);
  applyTextColorByKey(txt);
  applyDeviceTimezoneByKey(timezoneKey);
}

void resetDataCaches() {
  tempC = NAN;
  precipMm = NAN;
  windSpeedMs = NAN;
  windDirectionDeg = NAN;
  kpIndex = NAN;
  aqiEuropean = NAN;
  aqiUs = NAN;
  forecastCount = 0;
  sunriseMin = -1;
  sunsetMin = -1;
  lastSunYmd = -1;
  lastWeatherFetch = 0;
  lastAqiFetch = 0;
  lastKpFetch = 0;
  lastWeatherPageCache = "";
  lastForecastText = "";
  dataDirty = true;
  pageDirty = true;
}

// =========================================================
// TOUCH
// =========================================================
bool readTouchXY(int& sx, int& sy) {
  if (!ts.touched()) return false;

  TS_Point p = ts.getPoint();
  if (p.z < 80 || p.z > 4000) return false;

  int x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W);
  int y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H);

  x = constrain(x, 0, SCREEN_W - 1);
  y = constrain(y, 0, SCREEN_H - 1);

  if (TOUCH_SWAP_XY) { int tmp = x; x = y; y = tmp; }
  if (TOUCH_FLIP_X)  x = (SCREEN_W - 1) - x;
  if (TOUCH_FLIP_Y)  y = (SCREEN_H - 1) - y;

  sx = x;
  sy = y;
  return true;
}

bool touchNewPress(int& tx, int& ty) {
  static bool wasDown = false;
  static unsigned long lastPressMs = 0;

  bool down = false;
  int x = 0, y = 0;

  if (readTouchXY(x, y)) down = true;

  bool fire = false;
  unsigned long now = millis();

  if (down && !wasDown && (now - lastPressMs > 220)) {
    fire = true;
    lastPressMs = now;
    tx = x;
    ty = y;
  }

  wasDown = down;
  return fire;
}

// =========================================================
// API
// =========================================================
bool fetchSunriseSunset() {
  if (WiFi.status() != WL_CONNECTED) return false;

  static WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.sunrise-sunset.org/json?lat=") + String(LAT, 4) +
               "&lng=" + String(LNG, 4) + "&formatted=0";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, body)) return false;

  const char* sunriseStr = doc["results"]["sunrise"];
  const char* sunsetStr  = doc["results"]["sunset"];
  if (!sunriseStr || !sunsetStr) return false;

  auto parseIsoToEpochUTC = [](const char* iso) -> time_t {
    int Y, M, D, h, m, s;
    if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) return (time_t)-1;

    struct tm t{};
    t.tm_year = Y - 1900;
    t.tm_mon  = M - 1;
    t.tm_mday = D;
    t.tm_hour = h;
    t.tm_min  = m;
    t.tm_sec  = s;

    char* oldTz = getenv("TZ");
    String old = oldTz ? String(oldTz) : String("");

    setenv("TZ", "UTC0", 1);
    tzset();
    time_t epoch = mktime(&t);

    if (old.length()) setenv("TZ", old.c_str(), 1);
    else unsetenv("TZ");
    tzset();

    return epoch;
  };

  time_t srEpoch = parseIsoToEpochUTC(sunriseStr);
  time_t ssEpoch = parseIsoToEpochUTC(sunsetStr);
  if (srEpoch < 0 || ssEpoch < 0) return false;

  sunriseMin = minutesFromLocalEpoch(srEpoch);
  sunsetMin  = minutesFromLocalEpoch(ssEpoch);
  lastSunYmd = ymdFromLocal(time(nullptr));
  lastSyncTime = time(nullptr);
  return true;
}

void ensureSunTimesForToday() {
  time_t nowT = time(nullptr);
  int ymd = ymdFromLocal(nowT);

  if ((sunriseMin < 0 || sunsetMin < 0 || ymd != lastSunYmd) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchSunriseSunset()) dataDirty = true;
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  static WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + String(LAT, 4) +
               "&longitude=" + String(LNG, 4) +
               "&current=temperature_2m,wind_speed_10m,wind_direction_10m,uv_index,precipitation" +
               "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max,weather_code" +
               "&forecast_days=6&timezone=auto&wind_speed_unit=ms&temperature_unit=celsius";

  Serial.printf("[WX] Open-Meteo %s (%.4f, %.4f) units=%s\n",
                locationName.c_str(), LAT, LNG, unitKey.c_str());

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, body)) return false;

  tempC = doc["current"]["temperature_2m"] | NAN;
  windSpeedMs = doc["current"]["wind_speed_10m"] | NAN;
  windDirectionDeg = doc["current"]["wind_direction_10m"] | NAN;
  uvIndex = doc["current"]["uv_index"] | NAN;
  precipMm = doc["current"]["precipitation"] | NAN;
  tempMaxC = NAN;
  tempMinC = NAN;
  forecastCount = 0;

  JsonArray maxTemps = doc["daily"]["temperature_2m_max"];
  JsonArray minTemps = doc["daily"]["temperature_2m_min"];
  JsonArray precipSums = doc["daily"]["precipitation_sum"];
  JsonArray precipProbs = doc["daily"]["precipitation_probability_max"];
  JsonArray weatherCodes = doc["daily"]["weather_code"];
  JsonArray dayTimes = doc["daily"]["time"];

  int n = 0;
  if (maxTemps && !maxTemps.isNull()) n = (int)maxTemps.size();
  if (n > FORECAST_DAYS) n = FORECAST_DAYS;

  for (int i = 0; i < n; i++) {
    forecastDays[i].tmax = (maxTemps && i < (int)maxTemps.size()) ? (maxTemps[i] | NAN) : NAN;
    forecastDays[i].tmin = (minTemps && i < (int)minTemps.size()) ? (minTemps[i] | NAN) : NAN;
    forecastDays[i].precipMm = (precipSums && i < (int)precipSums.size()) ? (precipSums[i] | NAN) : NAN;
    forecastDays[i].precipProb = (precipProbs && i < (int)precipProbs.size()) ? (precipProbs[i] | NAN) : NAN;
    forecastDays[i].weatherCode = (weatherCodes && i < (int)weatherCodes.size()) ? (int)(weatherCodes[i] | -1) : -1;
    forecastDays[i].wday = -1;
    if (dayTimes && i < (int)dayTimes.size()) {
      const char* iso = dayTimes[i];
      if (iso) {
        int y = 0, m = 0, d = 0;
        if (sscanf(iso, "%d-%d-%d", &y, &m, &d) == 3) {
          struct tm tmDay = {};
          tmDay.tm_year = y - 1900;
          tmDay.tm_mon = m - 1;
          tmDay.tm_mday = d;
          tmDay.tm_hour = 12;
          mktime(&tmDay);
          forecastDays[i].wday = tmDay.tm_wday;
        }
      }
    }
    forecastCount++;
  }

  if (forecastCount > 0) {
    tempMaxC = forecastDays[0].tmax;
    tempMinC = forecastDays[0].tmin;
  }

  lastWeatherFetch = time(nullptr);
  lastSyncTime = lastWeatherFetch;
  lastWeatherPageCache = "";
  lastForecastText = "";
  return true;
}

void ensureWeather() {
  time_t nowT = time(nullptr);
  if ((isnan(tempC) || isnan(tempMinC) || isnan(tempMaxC) || forecastCount <= 0 || isnan(precipMm) || isnan(windSpeedMs) || isnan(windDirectionDeg) || isnan(uvIndex) ||
       (nowT - lastWeatherFetch) > WEATHER_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchWeather()) dataDirty = true;
  }
}

bool fetchAirQuality() {
  if (WiFi.status() != WL_CONNECTED) return false;

  static WiFiClientSecure client;
  client.setInsecure();

  String url = String("https://air-quality-api.open-meteo.com/v1/air-quality?latitude=") +
               String(LAT, 4) + "&longitude=" + String(LNG, 4) +
               "&current=european_aqi,us_aqi";

  HTTPClient http;
  if (!http.begin(client, url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, body)) return false;

  aqiEuropean = doc["current"]["european_aqi"] | NAN;
  aqiUs = doc["current"]["us_aqi"] | NAN;
  lastAqiFetch = time(nullptr);
  lastSyncTime = lastAqiFetch;
  lastWeatherPageCache = "";
  return true;
}

void ensureAirQuality() {
  time_t nowT = time(nullptr);
  if ((isnan(aqiEuropean) || (nowT - lastAqiFetch) > WEATHER_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchAirQuality()) dataDirty = true;
  }
}

String formatGeoName(const GeoHit& hit) {
  String out = hit.name.length() ? hit.name : geoLastQuery;
  if (hit.admin1.length() && hit.admin1 != hit.name) {
    out += ", ";
    out += hit.admin1;
  }
  if (hit.country.length()) {
    out += ", ";
    out += hit.country;
  }
  return out;
}

void persistTimezonePrefs() {
  prefs.putString("tz", timezoneKey);
  prefs.putString("tziana", timezoneIana);
  prefs.putString("tzposix", timezonePosixCustom);
}

void applyGeoHit(const GeoHit& hit) {
  locationName = formatGeoName(hit);
  LAT = hit.lat;
  LNG = hit.lng;
  if (hit.timezone.length()) applyTimezoneFromIana(hit.timezone);
}

static int geocodeCountryRank(const String& code) {
  if (code.equalsIgnoreCase("AR")) return 0;
  if (code.equalsIgnoreCase("UY") || code.equalsIgnoreCase("CL") ||
      code.equalsIgnoreCase("PY") || code.equalsIgnoreCase("BO")) return 1;
  return 2;
}

static void sortGeoHitsLatamFirst() {
  for (int i = 1; i < geoHitCount; i++) {
    GeoHit key = geoHits[i];
    int j = i - 1;
    int keyRank = geocodeCountryRank(key.countryCode);
    while (j >= 0 && geocodeCountryRank(geoHits[j].countryCode) > keyRank) {
      geoHits[j + 1] = geoHits[j];
      j--;
    }
    geoHits[j + 1] = key;
  }
}

static int geocodeFetchUrl(const String& url) {
  static WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(client, url)) {
    Serial.println("[GEO] No se pudo iniciar la busqueda.");
    return -1;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[GEO] HTTP %d al buscar ubicacion.\n", code);
    http.end();
    return -1;
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  if (deserializeJson(doc, body)) {
    Serial.println("[GEO] Respuesta JSON invalida.");
    return -1;
  }

  JsonArray results = doc["results"];
  if (results.isNull() || results.size() == 0) return 0;

  int n = min((int)results.size(), GEO_MAX);
  for (int i = 0; i < n; i++) {
    JsonObject item = results[i];
    float lat = item["latitude"] | NAN;
    float lng = item["longitude"] | NAN;
    if (isnan(lat) || isnan(lng)) continue;
    geoHits[geoHitCount].name = item["name"] | "";
    geoHits[geoHitCount].admin1 = item["admin1"] | "";
    geoHits[geoHitCount].country = item["country"] | "";
    geoHits[geoHitCount].countryCode = item["country_code"] | "";
    geoHits[geoHitCount].timezone = item["timezone"] | "";
    geoHits[geoHitCount].lat = lat;
    geoHits[geoHitCount].lng = lng;
    geoHitCount++;
  }
  return geoHitCount;
}

int geocodeSearch(const String& query) {
  geoHitCount = 0;
  geoSelected = -1;
  geoSearchFailed = false;
  geoLastQuery = query;
  geoLastQuery.trim();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[GEO] Sin WiFi; no se puede buscar la ubicacion.");
    geoSearchFailed = true;
    return 0;
  }
  if (!geoLastQuery.length()) {
    geoSearchFailed = true;
    return 0;
  }

  String lang = useEn() ? "en" : "es";
  String url = String("https://geocoding-api.open-meteo.com/v1/search?name=") +
               urlEncodeQuery(geoLastQuery) + "&count=10&language=" + lang + "&format=json";

  int fetched = geocodeFetchUrl(url);
  if (fetched < 0) {
    geoSearchFailed = true;
    return 0;
  }

  if (geoHitCount == 0 && !useEn()) {
    Serial.println("[GEO] Sin resultados; reintento con country=AR.");
    fetched = geocodeFetchUrl(url + "&country=AR");
    if (fetched < 0) {
      geoSearchFailed = true;
      return 0;
    }
  }

  if (geoHitCount == 0) {
    Serial.println("[GEO] Sin resultados.");
    geoSearchFailed = true;
    return 0;
  }

  if (!useEn()) sortGeoHitsLatamFirst();
  geoSelected = 0;
  Serial.printf("[GEO] %d resultado(s) para la busqueda.\n", geoHitCount);
  return geoHitCount;
}

bool fetchKpIndex() {
  if (WiFi.status() != WL_CONNECTED) return false;

  static WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json")) {
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  int lastRow = body.lastIndexOf('[');
  if (lastRow < 0) return false;

  int firstComma = body.indexOf(',', lastRow);
  if (firstComma < 0) return false;

  int q1 = body.indexOf('"', firstComma);
  if (q1 < 0) return false;
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return false;

  String kpStrLocal = body.substring(q1 + 1, q2);
  kpIndex = kpStrLocal.toFloat();

  lastKpFetch = time(nullptr);
  lastSyncTime = lastKpFetch;
  return true;
}

void ensureKpIndex() {
  time_t nowT = time(nullptr);
  if ((isnan(kpIndex) || (nowT - lastKpFetch) > KP_INTERVAL_SEC) &&
      WiFi.status() == WL_CONNECTED) {
    if (fetchKpIndex()) dataDirty = true;
  }
}

// =========================================================
// DRAW HELPERS
// =========================================================
void drawCard(int x, int y, int w, int h, bool accent = false) {
  tft.fillRoundRect(x, y, w, h, 10, COL_PANEL);
  tft.drawRoundRect(x, y, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void drawTopBar(const String& title) {
  tft.fillRect(0, 0, SCREEN_W, TOPBAR_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, TOPBAR_H - 1, SCREEN_W, COL_STROKE);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString(title, 10, 9, 2);

  const int bs = 25;
  const int bx = SCREEN_W - bs - 6;
  const int by = 4;

  uint16_t bg = (sleepDimmed || sleepOff) ? COL_ACCENT : COL_PANEL;
  uint16_t fg = (sleepDimmed || sleepOff) ? TFT_BLACK : COL_TEXT;

  tft.fillRoundRect(bx, by, bs, bs, 7, bg);
  tft.drawRoundRect(bx, by, bs, bs, 7, COL_ACCENT);

  const int cx = bx + 12;
  const int cy = by + 12;

  tft.drawCircle(cx, cy + 1, 6, fg);
  tft.drawFastHLine(cx - 4, cy - 5, 9, bg);
  tft.drawFastVLine(cx, cy - 7, 6, fg);
}

void drawNavBar() {
  const int y = SCREEN_H - NAV_H;
  tft.fillRect(0, y, SCREEN_W, NAV_H, COL_PANEL_ALT);
  tft.drawFastHLine(0, y, SCREEN_W, COL_STROKE);

  const int btnW = SCREEN_W / 4;
  const char* names[4] = {tr("Inicio", "Home"), tr("Clima", "Weather"), "Pomo", tr("Estado", "Status")};

  for (int i = 0; i < 4; i++) {
    int bx = i * btnW;
    bool active = ((int)currentPage == i);

    uint16_t bg = active ? COL_ACCENT : COL_PANEL;
    uint16_t fg = active ? TFT_BLACK : COL_TEXT;

    tft.fillRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, bg);
    tft.drawRoundRect(bx + 4, y + 6, btnW - 8, NAV_H - 12, 8, active ? COL_ACCENT : COL_STROKE);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(names[i], bx + btnW / 2, y + NAV_H / 2, 1);
  }

  tft.setTextDatum(TL_DATUM);
}

void makeSpriteCard(TFT_eSprite& spr, int w, int h, bool accent = false) {
  spr.setColorDepth(16);
  spr.createSprite(w, h);
  spr.fillSprite(COL_BG);
  spr.fillRoundRect(0, 0, w, h, 10, COL_PANEL);
  spr.drawRoundRect(0, 0, w, h, 10, accent ? COL_ACCENT : COL_STROKE);
}

void pushSpriteAndDelete(TFT_eSprite& spr, int x, int y) {
  spr.pushSprite(x, y, COL_BG);
  spr.deleteSprite();
}

void drawCleanSunIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 4, c);
  spr.drawLine(cx, cy - 9, cx, cy - 7, c);
  spr.drawLine(cx, cy + 7, cx, cy + 9, c);
  spr.drawLine(cx - 9, cy, cx - 7, cy, c);
  spr.drawLine(cx + 7, cy, cx + 9, cy, c);
  spr.drawLine(cx - 6, cy - 6, cx - 5, cy - 5, c);
  spr.drawLine(cx + 5, cy - 5, cx + 6, cy - 6, c);
  spr.drawLine(cx - 6, cy + 6, cx - 5, cy + 5, c);
  spr.drawLine(cx + 5, cy + 5, cx + 6, cy + 6, c);
}

void drawMoonIcon(TFT_eSprite& spr, int cx, int cy, uint16_t c) {
  spr.fillCircle(cx, cy, 6, c);
  spr.fillCircle(cx + 4, cy - 2, 6, COL_PANEL);
}

int drawWrappedTextLimited(int x, int y, int maxW, const String& text, int font, uint16_t fg, uint16_t bg, int maxLines) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(fg, bg);

  const int lineH = tft.fontHeight(font) + 2;
  String line = "";
  String word = "";
  int linesDrawn = 0;

  auto flushLine = [&]() {
    if (linesDrawn >= maxLines) return;
    if (line.length() > 0) tft.drawString(line, x, y, font);
    y += lineH;
    line = "";
    linesDrawn++;
  };

  auto placeWordOnEmptyLine = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    while (tft.textWidth(word, font) > maxW && word.length() > 1) {
      int cut = word.length();
      while (cut > 1 && tft.textWidth(word.substring(0, cut), font) > maxW) cut--;
      if (linesDrawn >= maxLines) return;
      tft.drawString(word.substring(0, cut), x, y, font);
      y += lineH;
      linesDrawn++;
      word = word.substring(cut);
    }

    if (linesDrawn < maxLines) {
      line = word;
      word = "";
    }
  };

  auto flushWord = [&]() {
    if (word.length() == 0 || linesDrawn >= maxLines) return;

    if (line.length() == 0) {
      placeWordOnEmptyLine();
      return;
    }

    String candidate = line + " " + word;
    if (tft.textWidth(candidate, font) <= maxW) {
      line = candidate;
      word = "";
      return;
    }

    flushLine();
    placeWordOnEmptyLine();
  };

  for (int i = 0; i < (int)text.length(); i++) {
    if (linesDrawn >= maxLines) break;
    char c = text[i];

    if (c == '\n') {
      flushWord();
      flushLine();
      continue;
    }

    if (c == ' ') {
      flushWord();
      continue;
    }

    word += c;
  }

  if (linesDrawn < maxLines) {
    flushWord();
    if (line.length() > 0) flushLine();
  }

  return y;
}

// =========================================================
// HOME SPRITES
// =========================================================
void drawClockCardSprite(bool force = false) {
  const int x = 8, y = PAGE_ROW1_Y, w = 224, h = HOME_WIDGET_H;

  time_t now = time(nullptr);
  struct tm tmNow;
  localtime_r(&now, &tmNow);

  String timeBuf = formatClockParts(tmNow, true);
  String dateBuf = formatDateParts(tmNow);

  String sr = formatMinuteOfDay(sunriseMin);
  String ss = formatMinuteOfDay(sunsetMin);

  String combined = timeBuf + "|" + dateBuf + "|" + sr + "|" + ss + "|" +
                    String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cacheClock) return;
  cacheClock = combined;

  makeSpriteCard(sprClock, w, h, true);

  sprClock.setTextDatum(TL_DATUM);

  sprClock.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = timeBuf.lastIndexOf(' ');
    String clockMain = splitAt > 0 ? timeBuf.substring(0, splitAt) : timeBuf;
    String clockSuffix = splitAt > 0 ? timeBuf.substring(splitAt + 1) : "";
    sprClock.drawString(clockMain, 10, 11, 4);
    if (clockSuffix.length() > 0) {
      int suffixX = 10 + sprClock.textWidth(clockMain, 4) + 4;
      sprClock.drawString(clockSuffix, suffixX, 18, 2);
    }
  } else {
    sprClock.drawString(timeBuf, 10, 11, 4);
  }

  sprClock.setTextColor(COL_DIM, COL_PANEL);
  sprClock.drawString(dateBuf, 10, 45, 2);

  drawCleanSunIcon(sprClock, 151, 22, COL_ACCENT);
  drawMoonIcon(sprClock, 151, 50, COL_ACCENT);

  sprClock.setTextColor(COL_ACCENT, COL_PANEL);
  sprClock.drawString(sr, 165, 15, 2);
  sprClock.drawString(ss, 165, 43, 2);

  pushSpriteAndDelete(sprClock, x, y);
}

void drawMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 31, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 55, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawWeatherStyleMetricSprite(int x, int y, int w, int h, const char* label, const String& value, String& cache, bool force = false, const String& detail = "") {
  String combined = String(label) + "|" + value + "|" + detail + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  sprSmall.drawString(value, 10, 28, 4);

  if (detail.length() > 0) {
    sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
    sprSmall.drawString(detail, 10, 52, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawSunEventWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String label = nextSunLabel();
  String value = nextSunTimeText();
  String combined = label + "|" + value + "|" + String(COL_PANEL) + "|" +
                    String(COL_STROKE) + "|" + String(COL_TEXT) + "|" + String(COL_ACCENT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_TEXT, COL_PANEL);
  if (useUsRegionFormat()) {
    int splitAt = value.lastIndexOf(' ');
    String mainValue = splitAt > 0 ? value.substring(0, splitAt) : value;
    String suffix = splitAt > 0 ? value.substring(splitAt + 1) : "";
    sprSmall.drawString(mainValue, 10, 30, 4);
    if (suffix.length() > 0) {
      int suffixX = 10 + sprSmall.textWidth(mainValue, 4) + 3;
      sprSmall.drawString(suffix, suffixX, 35, 2);
    }
  } else {
    sprSmall.drawString(value, 10, 30, 4);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawPlaceholderSprite(int x, int y, int w, int h, const char* label, String& cache, bool force = false) {
  String combined = String(label) + "|" + String(COL_PANEL) + "|" + String(COL_STROKE) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(label, 10, 8, 2);

  sprSmall.setTextColor(COL_STROKE, COL_PANEL);
  sprSmall.drawString(tr("Vacio", "Empty"), 10, 31, 2);

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawFocusTimerWidget(int x, int y, int w, int h, String& cache, bool force = false) {
  String value = formatTimerClock(focusRemainingSec);
  String hint = focusHintText();
  String combined = value + "|" + hint + "|" + String(focusMenuOpen ? 1 : 0) +
                    "|" + String(COL_PANEL) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);

  if (!force && combined == cache) return;
  cache = combined;

  makeSpriteCard(sprSmall, w, h, true);

  sprSmall.setTextDatum(TL_DATUM);
  sprSmall.setTextColor(COL_DIM, COL_PANEL);
  sprSmall.drawString(tr("Timer", "Timer"), 10, 8, 2);

  if (focusMenuOpen) {
    sprSmall.setTextColor(COL_TEXT, COL_PANEL);
    sprSmall.drawString(tr("Elegir", "Pick"), 10, 22, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString(tr("tiempo", "time"), 10, 44, 2);
  } else {
    sprSmall.setTextColor(focusTimerFinished ? COL_GREEN : COL_TEXT, COL_PANEL);
    sprSmall.drawString(value, 10, 24, 4);
    sprSmall.setTextColor(COL_DIM, COL_PANEL);
    sprSmall.drawString(hint, 10, 49, 1);
  }

  pushSpriteAndDelete(sprSmall, x, y);
}

void drawHomeSlotWidget(int slot, bool force = false) {
  int x, y, w, h;
  getHomeSlotRect(slot, x, y, w, h);

  switch (homeWidgetSlots[slot]) {
    case HOME_WIDGET_WEEK:
      drawMetricSprite(x, y, w, h, tr("Semana", "Week"), weekNumberText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_TIMER:
      drawFocusTimerWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_RAIN:
      drawWeatherStyleMetricSprite(x, y, w, h, tr("Lluvia", "Rain"), rainText(), cacheHomeSlots[slot], force);
      break;
    case HOME_WIDGET_OUTDOOR: {
      String city = weatherCityShort();
      String range = tempRangeText();
      String combined = city + "|" + tempText() + "|" + range + "|" + String(COL_PANEL);
      if (force || combined != cacheHomeSlots[slot]) {
        cacheHomeSlots[slot] = combined;
        makeSpriteCard(sprSmall, w, h, true);
        sprSmall.setTextDatum(TL_DATUM);
        sprSmall.setTextColor(COL_DIM, COL_PANEL);
        sprSmall.drawString(city, 10, 6, 2);
        sprSmall.setTextColor(COL_TEXT, COL_PANEL);
        sprSmall.drawString(tempText(), 10, 24, 4);
        sprSmall.setTextColor(COL_ACCENT, COL_PANEL);
        sprSmall.drawString(range, 10, 52, 2);
        pushSpriteAndDelete(sprSmall, x, y);
      }
      break;
    }
    case HOME_WIDGET_MINMAX:
      drawWeatherStyleMetricSprite(x, y, w, h, tr("Hoy max", "Today max"), minMaxValueText(), cacheHomeSlots[slot], force, minMaxDetailText());
      break;
    case HOME_WIDGET_AQI:
      drawWeatherStyleMetricSprite(x, y, w, h, tr("Aire", "Air"), aqiValueText(), cacheHomeSlots[slot], force, aqiLevelText());
      break;
    case HOME_WIDGET_FORECAST:
      drawWeatherStyleMetricSprite(x, y, w, h, tr("Manana", "Tomorrow"), forecastSnippetText(), cacheHomeSlots[slot], force, forecastSnippetDetail());
      break;
    case HOME_WIDGET_KP:
      drawWeatherStyleMetricSprite(x, y, w, h, tr("Indice KP", "KP index"), kpText(), cacheHomeSlots[slot], force, kpLevelText());
      break;
    case HOME_WIDGET_UV:
      drawWeatherStyleMetricSprite(x, y, w, h, tr("Indice UV", "UV index"), uvText(), cacheHomeSlots[slot], force, uvLevelText());
      break;
    case HOME_WIDGET_WIND:
      drawWeatherStyleMetricSprite(x, y, w, h, tr("Viento", "Wind"), windText(), cacheHomeSlots[slot], force, windDirectionText());
      break;
    case HOME_WIDGET_SUN:
      drawSunEventWidget(x, y, w, h, cacheHomeSlots[slot], force);
      break;
  }
}

void drawFocusMenuOverlay(bool force = false) {
  String combined = String(focusTimerRunning ? 1 : 0) + "|" + String(focusTimerFinished ? 1 : 0) +
                    "|" + String(focusTimerPaused ? 1 : 0) + "|" + String(COL_PANEL_ALT) + "|" +
                    String(COL_PANEL) + "|" + String(COL_ACCENT);
  if (!force && combined == cacheTimerMenu) return;
  cacheTimerMenu = combined;

  tft.fillRect(0, 0, SCREEN_W, SCREEN_H, COL_BG);
  tft.fillRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_PANEL_ALT);
  tft.drawRoundRect(TIMER_MENU_X, TIMER_MENU_Y, TIMER_MENU_W, TIMER_MENU_H, 12, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
  tft.drawString(tr("Iniciar timer", "Start timer"), TIMER_MENU_X + 14, TIMER_MENU_Y + 12, 2);
  tft.setTextColor(COL_DIM, COL_PANEL_ALT);
  tft.drawString(tr("Elegi la duracion", "Choose duration"), TIMER_MENU_X + 14, TIMER_MENU_Y + 34, 1);

  const int btnW = 74;
  const int btnH = 28;
  const int col1X = TIMER_MENU_X + 14;
  const int col2X = TIMER_MENU_X + 112;
  const int row1Y = TIMER_MENU_Y + 54;
  const int row2Y = TIMER_MENU_Y + 88;
  const int row3Y = TIMER_MENU_Y + 122;

  String labels[6];
  const int xs[] = {col1X, col2X, col1X, col2X, col1X, col2X};
  const int ys[] = {row1Y, row1Y, row2Y, row2Y, row3Y, row3Y};
  for (int i = 0; i < 6; i++) {
    labels[i] = String(timerPresetMin[i]) + " min";
  }

  for (int i = 0; i < 6; i++) {
    tft.fillRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_PANEL);
    tft.drawRoundRect(xs[i], ys[i], btnW, btnH, 8, COL_STROKE);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawCentreString(labels[i].c_str(), xs[i] + btnW / 2, ys[i] + 7, 2);
  }

  const int actionY = TIMER_MENU_Y + 160;
  const int actionW = 152;
  const int actionX = TIMER_MENU_X + 24;
  const char* actionLabel = (focusTimerRunning || focusTimerPaused)
    ? tr("Parar", "Stop")
    : (focusTimerFinished ? "Reset" : nullptr);
  uint16_t actionColor = (focusTimerRunning || focusTimerPaused) ? COL_RED : COL_ACCENT;

  if (actionLabel) {
    tft.fillRoundRect(actionX, actionY - 4, actionW, 26, 8, COL_PANEL);
    tft.drawRoundRect(actionX, actionY - 4, actionW, 26, 8, actionColor);
    tft.setTextColor(actionColor, COL_PANEL);
    tft.drawCentreString(actionLabel, actionX + actionW / 2, actionY + 4, 2);
  } else {
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(tr("Toca afuera para cerrar", "Tap outside to close"), TIMER_MENU_X + TIMER_MENU_W / 2, TIMER_MENU_Y + TIMER_MENU_H - 13, 1);
  }
}

void drawTimerDoneOverlay(bool force = false) {
  if (!timerDoneDialogOpen) return;

  String elapsed = formatElapsedText(focusDurationSec);
  String countdown = timerDoneCountdownText();
  bool flashOn = flashModeEnabled && ((millis() / 300UL) % 2UL == 0);
  setBacklight(flashModeEnabled ? (flashOn ? FLASH_BL_HIGH : FLASH_BL_LOW) : BL_FULL);
  String combined = elapsed + "|" + String(COL_PANEL_ALT) + "|" + String(COL_ACCENT) + "|" + String(COL_TEXT);
  String flashKey = String(flashOn ? 1 : 0);
  if (force || combined != cacheTimerDone || flashKey != cacheTimerDoneFlash) {
    cacheTimerDone = combined;
    cacheTimerDoneFlash = flashKey;
    cacheTimerDoneCountdown = "";

    uint16_t backdrop = flashOn ? COL_ACCENT : COL_BG;
    uint16_t panelBorder = flashOn ? TFT_WHITE : COL_ACCENT;
    tft.fillRect(0, 0, SCREEN_W, SCREEN_H, backdrop);
    tft.fillRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, COL_PANEL_ALT);
    tft.drawRoundRect(TIMER_DONE_X, TIMER_DONE_Y, TIMER_DONE_W, TIMER_DONE_H, 12, panelBorder);

    const char* doneTitle = tr("Timer listo", "Timer done");
    if (timerKind == TIMER_POMO) {
      doneTitle = (pomoEndedPhase == POMO_WORK)
        ? tr("Foco listo", "Focus done")
        : tr("Descanso listo", "Break done");
    }
    tft.setTextColor(COL_TEXT, COL_PANEL_ALT);
    tft.drawCentreString(doneTitle, TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 14, 2);
    tft.setTextColor(COL_ACCENT, COL_PANEL_ALT);
    tft.drawCentreString(elapsed, TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 42, 2);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(tr("Toca para cerrar", "Tap to close"), TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 68, 1);
  }

  if (force || countdown != cacheTimerDoneCountdown) {
    cacheTimerDoneCountdown = countdown;
    tft.fillRect(TIMER_DONE_X + 28, TIMER_DONE_Y + 82, TIMER_DONE_W - 56, 12, COL_PANEL_ALT);
    tft.setTextColor(COL_DIM, COL_PANEL_ALT);
    tft.drawCentreString(countdown.c_str(), TIMER_DONE_X + TIMER_DONE_W / 2, TIMER_DONE_Y + 82, 1);
  }
}

// =========================================================
// PAGES
// =========================================================
void drawHomePageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(homeTitleText());
  drawNavBar();

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  pageDirty = false;
  lastDrawnPage = PAGE_HOME;

  drawClockCardSprite(true);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, true);
  }
  if (focusMenuOpen) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateHomeDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen) {
    drawFocusMenuOverlay(false);
    return;
  }

  drawClockCardSprite(false);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    drawHomeSlotWidget(i, false);
  }
}

void drawWeatherPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(String(tr("Clima", "Weather")) + " · " + weatherCityShort());
  drawNavBar();

  drawCard(8, 38, 116, 52, true);
  drawCard(128, 38, 104, 52, true);
  drawCard(8, 94, 72, 32, true);
  drawCard(84, 94, 72, 32, true);
  drawCard(160, 94, 72, 32, true);
  drawCard(8, 130, 224, 142, true);

  pageDirty = false;
  lastDrawnPage = PAGE_WEATHER;
  lastWeatherPageCache = "";
}

void updateWeatherDynamic() {
  String city = weatherCityShort();
  String t = tempText();
  String tRange = tempRangeText();
  String aqi = aqiValueText();
  String aqiLv = aqiLevelText();
  String r = rainText();
  String w = windText();
  String u = uvText();
  String fc;
  fc.reserve(160);
  for (int i = 0; i < forecastCount; i++) {
    fc += String(forecastDays[i].wday);
    fc += formatTempNumber(forecastDays[i].tmax);
    fc += formatTempNumber(forecastDays[i].tmin);
    fc += String((int)roundf(isnan(forecastDays[i].precipProb) ? -1 : forecastDays[i].precipProb));
    fc += String(forecastDays[i].weatherCode);
  }
  String combined = city + "|" + t + "|" + tRange + "|" + aqi + "|" + aqiLv + "|" + r + "|" + w + "|" + u + "|" + fc;
  if (combined == lastWeatherPageCache) return;
  lastWeatherPageCache = combined;

  tft.setTextDatum(TL_DATUM);
  tft.fillRect(16, 42, 100, 44, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(city, 16, 42, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(t, 16, 54, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(tRange, 16, 76, 1);

  tft.fillRect(136, 42, 88, 44, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(tr("Aire", "Air"), 136, 42, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(aqi, 136, 54, 4);
  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(aqiLv, 136, 76, 1);

  tft.fillRect(12, 98, 64, 24, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(tr("Lluvia", "Rain"), 12, 98, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(r, 12, 110, 1);

  tft.fillRect(88, 98, 64, 24, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(tr("Viento", "Wind"), 88, 98, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(w, 88, 110, 1);

  tft.fillRect(164, 98, 64, 24, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("UV", 164, 98, 1);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(u, 164, 110, 1);

  tft.fillRect(16, 136, 208, 130, COL_PANEL);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(tr("Pronostico", "Forecast"), 16, 136, 1);
  int rowY = 150;
  int rows = forecastCount < FORECAST_DAYS ? forecastCount : FORECAST_DAYS;
  for (int i = 0; i < rows; i++) {
    String day = (i == 0) ? String(tr("Hoy", "Now")) : String(weekdayShort(forecastDays[i].wday));
    int prob = isnan(forecastDays[i].precipProb) ? 0 : (int)roundf(forecastDays[i].precipProb);
    String line = day + "  " + formatTempNumber(forecastDays[i].tmax) + "/" +
                  formatTempNumber(forecastDays[i].tmin) + "  " + String(prob) + "%  " +
                  weatherCodeShort(forecastDays[i].weatherCode);
    tft.setTextColor(i == 0 ? COL_TEXT : COL_DIM, COL_PANEL);
    tft.drawString(line, 16, rowY, 1);
    rowY += 20;
  }
}

void drawPomoPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar("Pomodoro");
  drawNavBar();
  drawCard(8, 42, 224, 100, true);
  pageDirty = false;
  lastDrawnPage = PAGE_POMO;
  lastPomoPageCache = "";
}

void updatePomoDynamic() {
  unsigned long shownSec = focusRemainingSec;
  if (!focusTimerRunning && !focusTimerPaused && !focusTimerFinished && shownSec == 0) {
    shownSec = (unsigned long)pomoMinutesFor(pomoPhase) * 60UL;
  }
  String clock = formatTimerClock(shownSec);
  String phase = (timerKind == TIMER_POMO || !focusTimerRunning) ? String(pomoPhaseLabel()) : tr("Timer", "Timer");
  String cycle = pomoCycleText();
  const char* action = focusTimerRunning ? tr("Pausa", "Pause") : tr("Inicio", "Start");
  bool showStop = focusTimerRunning || focusTimerPaused;
  String combined = clock + "|" + phase + "|" + cycle + "|" + action + "|" +
                    String(focusTimerRunning) + "|" + String(focusTimerPaused) + "|" +
                    String(showStop ? 1 : 0) + "|" + String(COL_ACCENT);
  if (combined == lastPomoPageCache) return;
  lastPomoPageCache = combined;

  tft.fillRect(16, 50, 208, 84, COL_PANEL);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString(phase + "  " + cycle, 120, 52, 2);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(clock, 120, 78, 6);
  tft.setTextDatum(TL_DATUM);

  tft.fillRect(8, 152, 224, 36, COL_BG);
  tft.setTextDatum(MC_DATUM);
  if (showStop) {
    tft.fillRoundRect(8, 152, 108, 36, 10, focusTimerRunning ? COL_PANEL : COL_ACCENT);
    tft.drawRoundRect(8, 152, 108, 36, 10, COL_ACCENT);
    tft.setTextColor(focusTimerRunning ? COL_TEXT : TFT_BLACK, focusTimerRunning ? COL_PANEL : COL_ACCENT);
    tft.drawString(action, 62, 170, 2);

    tft.fillRoundRect(124, 152, 108, 36, 10, COL_PANEL);
    tft.drawRoundRect(124, 152, 108, 36, 10, COL_RED);
    tft.setTextColor(COL_RED, COL_PANEL);
    tft.drawString(tr("Parar", "Stop"), 178, 170, 2);
  } else {
    tft.fillRoundRect(8, 152, 224, 36, 10, COL_ACCENT);
    tft.drawRoundRect(8, 152, 224, 36, 10, COL_ACCENT);
    tft.setTextColor(TFT_BLACK, COL_ACCENT);
    tft.drawString(action, 120, 170, 2);
  }

  tft.fillRoundRect(8, 196, 108, 36, 10, COL_PANEL);
  tft.drawRoundRect(8, 196, 108, 36, 10, COL_STROKE);
  tft.setTextColor(COL_TEXT, COL_PANEL);
  tft.drawString(tr("Saltar", "Skip"), 62, 214, 2);

  tft.fillRoundRect(124, 196, 108, 36, 10, COL_PANEL);
  tft.drawRoundRect(124, 196, 108, 36, 10, COL_STROKE);
  tft.drawString(tr("Reset", "Reset"), 178, 214, 2);
  tft.setTextDatum(TL_DATUM);

  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString(String(pomoWorkMin) + "/" + String(pomoShortMin) + "/" + String(pomoLongMin) + " min", 12, 244, 1);
}

void drawStatusPageFull() {
  tft.fillScreen(COL_BG);
  drawTopBar(tr("Estado", "Status"));
  drawNavBar();

  drawCard(8, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW1_Y, 108, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW2_Y, 224, PAGE_WIDGET_H, true);
  drawCard(8, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);
  drawCard(124, PAGE_ROW3_Y, 108, PAGE_WIDGET_H, true);

  pageDirty = false;
  lastDrawnPage = PAGE_STATUS;

  lastWifiText = "";
  lastSignalText = "";
  lastIpText = "";
  lastUptimeText = "";
  lastNetworkToggleText = "";
}

void updateStatusDynamic() {
  String w = wifiStatusText();
  if (w != lastWifiText) {
    tft.fillRect(18, PAGE_ROW1_Y + 24, 88, 30, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("WiFi", 18, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(statusColor(), COL_PANEL);
    tft.drawString(w, 18, PAGE_ROW1_Y + 32, 2);
    lastWifiText = w;
  }

  String s = signalText();
  if (s != lastSignalText) {
    tft.fillRect(134, PAGE_ROW1_Y + 30, 88, 24, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(tr("Senal", "Signal"), 134, PAGE_ROW1_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(s, 134, PAGE_ROW1_Y + 30, 4);
    lastSignalText = s;
  }

  String ip = ipText();
  String ipCombined = ip + "|" + String(wifiApMode ? AP_SSID : "");
  if (ipCombined != lastIpText) {
    tft.fillRect(18, PAGE_ROW2_Y + 28, 200, 32, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(wifiApMode ? tr("Red setup", "Setup net") : "IP", 18, PAGE_ROW2_Y + 8, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(ip, 18, PAGE_ROW2_Y + 28, 2);
    if (wifiApMode) {
      tft.setTextColor(COL_ACCENT, COL_PANEL);
      tft.drawString(AP_SSID, 18, PAGE_ROW2_Y + 46, 1);
    }
    lastIpText = ipCombined;
  }

  String up = uptimeText();
  String upCombined = up + "|" + lastSyncText();
  if (upCombined != lastUptimeText) {
    tft.fillRect(18, PAGE_ROW3_Y + 26, 88, 26, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(tr("Activo", "Uptime"), 18, PAGE_ROW3_Y + 10, 2);
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.drawString(up, 18, PAGE_ROW3_Y + 26, 2);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(lastSyncText(), 18, PAGE_ROW3_Y + 42, 1);
    lastUptimeText = upCombined;
  }

  String networkLabel = wifiEnabled ? "On" : "Off";
  if (networkLabel != lastNetworkToggleText) {
    uint16_t btnBg = wifiEnabled ? COL_ACCENT : COL_PANEL_ALT;
    uint16_t btnFg = wifiEnabled ? TFT_BLACK : COL_TEXT;
    uint16_t btnStroke = wifiEnabled ? COL_ACCENT : COL_STROKE;

    tft.fillRect(132, PAGE_ROW3_Y + 8, 92, 42, COL_PANEL);
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString(tr("Red", "Net"), 134, PAGE_ROW3_Y + 10, 2);
    tft.fillRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnBg);
    tft.drawRoundRect(134, PAGE_ROW3_Y + 30, 88, 22, 8, btnStroke);
    tft.setTextColor(btnFg, btnBg);
    tft.drawCentreString(networkLabel.c_str(), 178, PAGE_ROW3_Y + 32, 2);
    lastNetworkToggleText = networkLabel;
  }
}

void drawCurrentPageFull() {
  switch (currentPage) {
    case PAGE_HOME:    drawHomePageFull(); break;
    case PAGE_WEATHER: drawWeatherPageFull(); break;
    case PAGE_POMO:    drawPomoPageFull(); break;
    case PAGE_STATUS:  drawStatusPageFull(); break;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) drawFocusMenuOverlay(true);
  if (timerDoneDialogOpen) drawTimerDoneOverlay(true);
}

void updateCurrentPageDynamic() {
  if (timerDoneDialogOpen) {
    drawTimerDoneOverlay(false);
    return;
  }

  if (focusMenuOpen && currentPage == PAGE_HOME) {
    drawFocusMenuOverlay(false);
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:    updateHomeDynamic(); break;
    case PAGE_WEATHER: updateWeatherDynamic(); break;
    case PAGE_POMO:    updatePomoDynamic(); break;
    case PAGE_STATUS:  updateStatusDynamic(); break;
  }
}

bool handleFocusMenuTouch(int x, int y) {
  if (!focusMenuOpen) return false;

  const int btnW = 74;
  const int btnH = 28;
  const int col1X = TIMER_MENU_X + 14;
  const int col2X = TIMER_MENU_X + 112;
  const int row1Y = TIMER_MENU_Y + 54;
  const int row2Y = TIMER_MENU_Y + 88;
  const int row3Y = TIMER_MENU_Y + 122;
  const int actionX = TIMER_MENU_X + 24;
  const int actionY = TIMER_MENU_Y + 156;
  const int actionW = 152;
  const int actionH = 26;

  if ((focusTimerRunning || focusTimerPaused || focusTimerFinished) &&
      x >= actionX - 8 && x < actionX + actionW + 8 &&
      y >= actionY - 8 && y < actionY + actionH + 16) {
    if (focusTimerFinished) resetFocusTimer();
    else stopFocusTimer();
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  if (x < TIMER_MENU_X || x >= TIMER_MENU_X + TIMER_MENU_W || y < TIMER_MENU_Y || y >= TIMER_MENU_Y + TIMER_MENU_H) {
    focusMenuOpen = false;
    cacheTimerMenu = "";
    pageDirty = true;
    return true;
  }

  const int xs[] = {col1X, col2X, col1X, col2X, col1X, col2X};
  const int ys[] = {row1Y, row1Y, row2Y, row2Y, row3Y, row3Y};
  for (int i = 0; i < 6; i++) {
    if (x >= xs[i] && x < xs[i] + btnW && y >= ys[i] && y < ys[i] + btnH) {
      startFocusTimer(timerPresetMin[i]);
      pageDirty = true;
      return true;
    }
  }

  return true;
}

bool handleHomeTouch(int x, int y) {
  if (currentPage != PAGE_HOME) return false;

  if (focusMenuOpen) return handleFocusMenuTouch(x, y);

  for (int slot = 0; slot < HOME_SLOT_COUNT; slot++) {
    if (homeWidgetSlots[slot] != HOME_WIDGET_TIMER) continue;

    int slotX, slotY, slotW, slotH;
    getHomeSlotRect(slot, slotX, slotY, slotW, slotH);

    if (x >= slotX && x < slotX + slotW && y >= slotY && y < slotY + slotH) {
      if (focusTimerFinished) {
        resetFocusTimer();
      } else {
        focusMenuOpen = true;
        cacheFocusTimer = "";
        clearHomeSlotCaches();
        cacheTimerMenu = "";
      }
      pageDirty = true;
      return true;
    }
  }

  return false;
}

bool handleTimerDoneDialogTouch(int x, int y) {
  (void)x;
  (void)y;
  if (!timerDoneDialogOpen) return false;
  dismissTimerDoneDialog();
  return true;
}

bool handlePomoTouch(int x, int y) {
  if (currentPage != PAGE_POMO) return false;

  const bool showStop = focusTimerRunning || focusTimerPaused;
  if (showStop && x >= 124 && x < 232 && y >= 152 && y < 188) {
    stopFocusTimer();
    lastPomoPageCache = "";
    pageDirty = true;
    return true;
  }

  const bool hitStartPause = showStop
    ? (x >= 8 && x < 116 && y >= 152 && y < 188)
    : (x >= 8 && x < 232 && y >= 152 && y < 188);
  if (hitStartPause) {
    timerKind = TIMER_POMO;
    if (focusTimerRunning) {
      pauseFocusTimer();
    } else if (focusTimerPaused && focusRemainingSec > 0) {
      resumeFocusTimer();
    } else {
      startFocusTimer((unsigned long)pomoMinutesFor(pomoPhase), TIMER_POMO);
    }
    lastPomoPageCache = "";
    pageDirty = true;
    return true;
  }

  if (x >= 8 && x < 116 && y >= 196 && y < 232) {
    skipPomoPhase();
    lastPomoPageCache = "";
    pageDirty = true;
    return true;
  }

  if (x >= 124 && x < 232 && y >= 196 && y < 232) {
    resetPomoSession();
    lastPomoPageCache = "";
    pageDirty = true;
    return true;
  }

  return false;
}

bool handleStatusTouch(int x, int y) {
  if (currentPage != PAGE_STATUS) return false;

  if (x >= 124 && x < 232 && y >= 198 && y < 268) {
    setWifiEnabled(!wifiEnabled);
    return true;
  }

  return false;
}

// =========================================================
// NAVIGATION
// =========================================================
void handleNavTouch(int x, int y) {
  if (y < SCREEN_H - NAV_H) return;

  int btnW = SCREEN_W / 4;
  int idx = x / btnW;
  if (idx < 0 || idx > 3) return;

  Page newPage = (Page)idx;
  if (newPage != currentPage) {
    currentPage = newPage;
    pageDirty = true;
  }
}

// =========================================================
// WEB SERVER
// =========================================================
void handleRoot() {
  bool locErr = server.hasArg("locerr");
  String accent = prefs.getString("accent", "cyan");
  String bg     = prefs.getString("bg", "slate");
  String txt    = prefs.getString("text", "standard");
  String units  = prefs.getString("units", "metric");
  String region = prefs.getString("region", "europe");
  String tz     = sanitizeTimezoneKey(prefs.getString("tz", "europe_central"));
  String nickname = prefs.getString("nickname", "");
  bool flashMode = prefs.getBool("flashMode", false);
  String homeSlotKeys[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeSlotKeys[i] = prefs.getString((String("homeSlot") + String(i)).c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }

  String page;
  page.reserve(28000);

  page += "<!doctype html><html lang='";
  page += useEn() ? "en" : "es";
  page += "'><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>Deskbuddy</title>";
  page += "<style>";
  page += ":root{color-scheme:dark;}";
  page += "body{margin:0;background:linear-gradient(180deg,#0b1018 0%,#111827 100%);color:#edf2f7;font-family:system-ui,sans-serif;}";
  page += ".wrap{max-width:980px;margin:0 auto;padding:28px 16px 36px;}";
  page += ".hero{margin-bottom:18px;padding:18px 20px;border:1px solid #243244;border-radius:20px;background:linear-gradient(135deg,#111927 0%,#172235 100%);box-shadow:0 10px 30px rgba(0,0,0,.22);}";
  page += ".hero h1{font-size:30px;margin:0 0 8px 0;}";
  page += ".hero p{margin:0;color:#a9b7c9;font-size:14px;}";
  page += ".ip{display:inline-block;margin-top:14px;padding:8px 12px;border-radius:999px;background:#0b1220;border:1px solid #334155;color:#dbe7f5;font-size:13px;}";
  page += ".layout{display:grid;grid-template-columns:1.15fr .85fr;gap:16px;align-items:start;}";
  page += ".stack{display:grid;gap:16px;}";
  page += ".panel{background:#171b22;border:1px solid #2d3748;border-radius:18px;padding:18px;margin:0;}";
  page += ".panel-toggle{width:100%;display:flex;align-items:center;justify-content:space-between;gap:12px;background:none;border:none;color:#edf2f7;padding:0;margin:0;cursor:pointer;text-align:left;}";
  page += ".panel-toggle:hover{color:#ffffff;}";
  page += ".panel-toggle h2{flex:1;}";
  page += ".panel-chevron{font-size:18px;color:#8ea3ba;transition:transform .18s ease;}";
  page += ".panel.collapsed .panel-chevron{transform:rotate(-90deg);}";
  page += ".panel-body{margin-top:12px;}";
  page += ".panel.collapsed .panel-body{display:none;}";
  page += ".panel h2{margin:0 0 6px 0;font-size:18px;}";
  page += ".panel p{margin:0 0 14px 0;color:#94a3b8;font-size:13px;line-height:1.45;}";
  page += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;}";
  page += ".grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:14px;}";
  page += ".label{display:block;font-size:13px;margin:0 0 8px 0;color:#a0aec0;font-weight:600;}";
  page += "textarea,input,select{width:100%;border-radius:12px;border:1px solid #334155;background:#0b1220;color:#edf2f7;padding:12px;box-sizing:border-box;font:inherit;}";
  page += "textarea{min-height:170px;resize:vertical;}";
  page += "button{margin-top:18px;background:#38bdf8;border:none;color:#001018;padding:13px 18px;border-radius:12px;font-weight:800;cursor:pointer;font:inherit;}";
  page += ".muted{font-size:13px;color:#94a3b8;line-height:1.45;}";
  page += ".footer-note{margin-top:10px;font-size:12px;color:#7f92a8;}";
  page += ".loc-error{margin:0 0 12px 0;padding:10px 12px;border-radius:12px;background:#3b1515;border:1px solid #7f1d1d;color:#fecaca;font-size:13px;line-height:1.45;}";
  page += ".geo-list{display:grid;gap:8px;margin-top:12px;}";
  page += ".geo-item{display:flex;gap:10px;align-items:flex-start;padding:10px 12px;border:1px solid #334155;border-radius:12px;background:#0b1220;}";
  page += ".geo-item input{width:auto;margin-top:3px;}";
  page += ".btn-secondary{margin-top:12px;background:#1e293b;color:#edf2f7;border:1px solid #334155;}";
  page += ".setup-banner{margin-bottom:16px;padding:14px 16px;border-radius:16px;background:#14261c;border:1px solid #166534;color:#bbf7d0;}";
  page += ".settings-block{margin-top:18px;padding-top:16px;border-top:1px solid #2b3545;}";
  page += ".settings-block:first-of-type{margin-top:0;padding-top:0;border-top:none;}";
  page += ".settings-title{display:block;margin:0 0 6px 0;font-size:14px;font-weight:700;color:#edf2f7;letter-spacing:.02em;}";
  page += ".settings-desc{margin:0 0 12px 0;font-size:12px;color:#8ea3ba;line-height:1.45;}";
  page += ".color-stack{display:grid;gap:12px;}";
  page += ".color-row{display:grid;grid-template-columns:120px 1fr;gap:12px;align-items:center;}";
  page += ".color-meta{display:flex;align-items:center;justify-content:space-between;gap:10px;}";
  page += ".color-meta .label{margin:0;color:#dbe7f5;}";
  page += ".color-value{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += ".swatch-row{display:flex;flex-wrap:wrap;gap:8px;}";
  page += ".swatch{width:22px;height:22px;border-radius:999px;border:1px solid rgba(255,255,255,.18);cursor:pointer;position:relative;box-sizing:border-box;}";
  page += ".swatch input{display:none;}";
  page += ".swatch.active{box-shadow:0 0 0 2px #67e8f9, 0 0 0 5px rgba(103,232,249,.18);}";
  page += ".swatch.active::after{content:'';position:absolute;inset:5px;border-radius:999px;border:1px solid rgba(0,16,24,.45);}";
  page += ".timer-slot-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:14px;}";
  page += ".timer-slot{border:1px solid #334155;border-radius:12px;background:#0b1220;padding:10px 10px 12px 10px;}";
  page += ".timer-slot-head{font-size:12px;color:#8ea3ba;margin-bottom:8px;font-weight:600;}";
  page += ".timer-slot-input{display:flex;align-items:center;gap:8px;}";
  page += ".timer-slot input{padding:10px 12px;text-align:center;font-weight:700;}";
  page += ".timer-unit{font-size:12px;color:#8ea3ba;white-space:nowrap;}";
  page += "@media(max-width:820px){.layout{grid-template-columns:1fr;}.grid,.grid-3,.timer-slot-grid{grid-template-columns:1fr;}.color-row{grid-template-columns:1fr;}}";
  page += "</style></head><body><div class='wrap'>";
  page += "<div class='hero'>";
  page += "<h1>Deskbuddy</h1>";
  page += "<p>";
  page += tr("Personalizá Deskbuddy: widgets, pomodoro, colores y herramientas del día.", "Customize Deskbuddy: widgets, pomodoro, colors and daily tools.");
  page += "</p>";
  page += "<div class='ip'>IP: ";
  page += wifiApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  page += "</div></div>";
  if (wifiApMode) {
    page += "<div class='setup-banner'><strong>";
    page += tr("Modo setup WiFi", "WiFi setup mode");
    page += "</strong><div>";
    page += tr("Conectate a la red", "Join the network");
    page += " <b>Deskbuddy</b> &middot; 192.168.4.1</div></div>";
  }

  page += "<form method='POST' action='/save'>";
  page += "<div class='layout'><div class='stack'>";

  page += "<div class='panel' data-panel='pomo'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Pomodoro</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>";
  page += tr("Tiempos de foco y descansos. 4 focos y despues un descanso largo.", "Focus and break lengths. After 4 focus blocks comes a long break.");
  page += "</p>";
  page += "<div class='grid-3'>";
  page += "<div><label class='label'>";
  page += tr("Foco", "Focus");
  page += "</label><input type='number' min='5' max='90' name='pomoWork' value='";
  page += String(pomoWorkMin);
  page += "'></div>";
  page += "<div><label class='label'>";
  page += tr("Descanso", "Break");
  page += "</label><input type='number' min='1' max='30' name='pomoShort' value='";
  page += String(pomoShortMin);
  page += "'></div>";
  page += "<div><label class='label'>";
  page += tr("Largo", "Long");
  page += "</label><input type='number' min='5' max='45' name='pomoLong' value='";
  page += String(pomoLongMin);
  page += "'></div>";
  page += "</div>";
  page += "<div class='muted'>";
  page += tr("Por defecto 25 / 5 / 15 minutos. Se usa en la pestana Pomo.", "Defaults are 25 / 5 / 15 minutes. Used on the Pomo tab.");
  page += "</div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='theme'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>";
  page += tr("Tema y color", "Theme and color");
  page += "</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>";
  page += tr("Colores y estilo visual de la pantalla.", "Screen colors and visual style.");
  page += "</p>";
  page += "<div class='grid'>";

  page += "<div style='grid-column:1 / -1;' class='color-stack'>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>";
  page += tr("Acento", "Accent");
  page += "</label><span class='color-value' id='accent-value'>";
  page += accent;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(accent=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='accent' value='standard'" + String(accent=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='accent' value='ice'" + String(accent=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='accent' value='white'" + String(accent=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='accent' value='cyan'" + String(accent=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='accent' value='mint'" + String(accent=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='accent' value='green'" + String(accent=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='accent' value='blue'" + String(accent=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='accent' value='purple'" + String(accent=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='accent' value='pink'" + String(accent=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='accent' value='orange'" + String(accent=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='accent' value='amber'" + String(accent=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(accent=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='accent' value='red'" + String(accent=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>";
  page += tr("Texto", "Text");
  page += "</label><span class='color-value' id='text-value'>";
  page += txt;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(txt=="standard"?" active":"") + "' style='background:" + accentPreviewCss("standard") + ";'><input type='radio' name='text' value='standard'" + String(txt=="standard"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="ice"?" active":"") + "' style='background:" + accentPreviewCss("ice") + ";'><input type='radio' name='text' value='ice'" + String(txt=="ice"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="white"?" active":"") + "' style='background:" + accentPreviewCss("white") + ";'><input type='radio' name='text' value='white'" + String(txt=="white"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="cyan"?" active":"") + "' style='background:" + accentPreviewCss("cyan") + ";'><input type='radio' name='text' value='cyan'" + String(txt=="cyan"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="mint"?" active":"") + "' style='background:" + accentPreviewCss("mint") + ";'><input type='radio' name='text' value='mint'" + String(txt=="mint"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="green"?" active":"") + "' style='background:" + accentPreviewCss("green") + ";'><input type='radio' name='text' value='green'" + String(txt=="green"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="blue"?" active":"") + "' style='background:" + accentPreviewCss("blue") + ";'><input type='radio' name='text' value='blue'" + String(txt=="blue"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="purple"?" active":"") + "' style='background:" + accentPreviewCss("purple") + ";'><input type='radio' name='text' value='purple'" + String(txt=="purple"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="pink"?" active":"") + "' style='background:" + accentPreviewCss("pink") + ";'><input type='radio' name='text' value='pink'" + String(txt=="pink"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="orange"?" active":"") + "' style='background:" + accentPreviewCss("orange") + ";'><input type='radio' name='text' value='orange'" + String(txt=="orange"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="amber"?" active":"") + "' style='background:" + accentPreviewCss("amber") + ";'><input type='radio' name='text' value='amber'" + String(txt=="amber"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(txt=="red"?" active":"") + "' style='background:" + accentPreviewCss("red") + ";'><input type='radio' name='text' value='red'" + String(txt=="red"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "<div class='color-row'><div class='color-meta'><label class='label'>";
  page += tr("Tema", "Theme");
  page += "</label><span class='color-value' id='bg-value'>";
  page += bg;
  page += "</span></div><div class='swatch-row'>";
  page += "<label class='swatch" + String(bg=="slate"?" active":"") + "' style='background:" + themePreviewCss("slate") + ";'><input type='radio' name='bg' value='slate'" + String(bg=="slate"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="deep"?" active":"") + "' style='background:" + themePreviewCss("deep") + ";'><input type='radio' name='bg' value='deep'" + String(bg=="deep"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="nordic"?" active":"") + "' style='background:" + themePreviewCss("nordic") + ";'><input type='radio' name='bg' value='nordic'" + String(bg=="nordic"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="forest"?" active":"") + "' style='background:" + themePreviewCss("forest") + ";'><input type='radio' name='bg' value='forest'" + String(bg=="forest"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="coffee"?" active":"") + "' style='background:" + themePreviewCss("coffee") + ";'><input type='radio' name='bg' value='coffee'" + String(bg=="coffee"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="soft"?" active":"") + "' style='background:" + themePreviewCss("soft") + ";'><input type='radio' name='bg' value='soft'" + String(bg=="soft"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="midnight"?" active":"") + "' style='background:" + themePreviewCss("midnight") + ";'><input type='radio' name='bg' value='midnight'" + String(bg=="midnight"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="graphite"?" active":"") + "' style='background:" + themePreviewCss("graphite") + ";'><input type='radio' name='bg' value='graphite'" + String(bg=="graphite"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="garnet"?" active":"") + "' style='background:" + themePreviewCss("garnet") + ";'><input type='radio' name='bg' value='garnet'" + String(bg=="garnet"?" checked":"") + "></label>";
  page += "<label class='swatch" + String(bg=="ochre"?" active":"") + "' style='background:" + themePreviewCss("ochre") + ";'><input type='radio' name='bg' value='ochre'" + String(bg=="ochre"?" checked":"") + "></label>";
  page += "</div></div>";

  page += "</div>";

  page += "</div></div></div>";

  page += "<div class='panel' data-panel='settings'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>";
  page += tr("Ajustes", "Settings");
  page += "</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>";
  page += tr("Comportamiento general y timers.", "General behavior and timers.");
  page += "</p>";
  page += "<div class='settings-block'>";
  page += "<span class='settings-title'>";
  page += tr("General", "General");
  page += "</span>";
  page += "<div class='grid'>";
  page += "<div><label class='label'>";
  page += tr("Idioma", "Language");
  page += "</label><select name='lang'>";
  page += "<option value='es'";
  page += (langKey != "en" ? " selected" : "");
  page += ">Espanol</option>";
  page += "<option value='en'";
  page += (langKey == "en" ? " selected" : "");
  page += ">English</option>";
  page += "</select></div>";
  page += "<div><label class='label'>";
  page += tr("Apodo", "Nickname");
  page += "</label><input name='nickname' maxlength='24' value='" + htmlEscape(nickname) + "'></div>";
  page += "<div><label class='label'>";
  page += tr("Apagado automatico", "Auto sleep");
  page += "</label><select name='sleepMin'>";
  page += "<option value='0'"  + String(sleepIntervalMin==0?" selected":"")  + ">";
  page += tr("Nunca", "Never");
  page += "</option>";
  page += "<option value='1'"  + String(sleepIntervalMin==1?" selected":"")  + ">1 ";
  page += tr("minuto", "minute");
  page += "</option>";
  page += "<option value='5'"  + String(sleepIntervalMin==5?" selected":"")  + ">5 ";
  page += tr("minutos", "minutes");
  page += "</option>";
  page += "<option value='10'" + String(sleepIntervalMin==10?" selected":"") + ">10 ";
  page += tr("minutos", "minutes");
  page += "</option>";
  page += "<option value='30'" + String(sleepIntervalMin==30?" selected":"") + ">30 ";
  page += tr("minutos", "minutes");
  page += "</option>";
  page += "<option value='60'" + String(sleepIntervalMin==60?" selected":"") + ">1 ";
  page += tr("hora", "hour");
  page += "</option>";
  page += "</select><div class='muted' style='margin-top:8px;'>";
  page += tr("Primero baja el brillo y a los 60 segundos apaga la pantalla.", "Dims first, then turns the screen off after 60 seconds.");
  page += "</div></div>";
  page += "<div><label class='label'>";
  page += tr("Unidades", "Units");
  page += "</label><select name='units'>";
  page += "<option value='metric'"   + String(units=="metric"?" selected":"")   + ">Celsius / mm</option>";
  page += "<option value='imperial'" + String(units=="imperial"?" selected":"") + ">Fahrenheit / in</option>";
  page += "</select></div>";
  page += "<div><label class='label'>";
  page += tr("Formato de fecha", "Date format");
  page += "</label><select name='region'>";
  page += "<option value='europe'" + String(region=="europe"?" selected":"") + ">";
  page += tr("Europeo: dd.mm.aaaa", "European: dd.mm.yyyy");
  page += "</option>";
  page += "<option value='us'" + String(region=="us"?" selected":"") + ">";
  page += tr("EE.UU.: mm/dd/aaaa", "US: mm/dd/yyyy");
  page += "</option>";
  page += "</select></div>";
  page += "<div><label class='label'>";
  page += tr("Zona horaria", "Timezone");
  page += "</label><select name='tz'>";
  appendTimezoneOptions(page, tz);
  page += "</select></div>";
  page += "</div>";
  page += "</div>";
  page += "<div class='settings-block'><span class='settings-title'>Timer</span><div class='settings-desc'>";
  page += tr("Elegi los seis timers rapidos del menu.", "Choose the six quick timers.");
  page += "</div><div class='timer-slot-grid'>";
  for (int i = 0; i < 6; i++) {
    page += "<div class='timer-slot'><div class='timer-slot-head'>";
    page += tr("Hueco", "Slot");
    page += " " + String(i + 1) + "</div><div class='timer-slot-input'><input type='number' min='1' max='180' name='timer" + String(i) + "' value='" + String(timerPresetMin[i]) + "'><span class='timer-unit'>min</span></div></div>";
  }
  page += "</div>";
  page += "<div style='margin-top:14px;'><span class='settings-title'>";
  page += tr("Alerta", "Alert");
  page += "</span><label style='display:flex;align-items:center;gap:10px;color:#edf2f7;'><input type='checkbox' name='flashMode' value='1'" + String(flashMode ? " checked" : "") + " style='width:auto;'>";
  page += tr("Parpadear la pantalla al terminar el timer", "Flash the screen when the timer ends");
  page += "</label></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>";
  page += tr("Ubicacion", "Location");
  page += "</span>";
  page += "<div class='settings-desc'>";
  page += tr("Escribí una ciudad, busca coincidencias y elegí una. La zona horaria se ajusta sola. La temperatura es el clima de esa ciudad (Open-Meteo), no un sensor de la placa.", "Type a city, search matches and pick one. Timezone is set automatically. Temperature is that city's outdoor weather (Open-Meteo), not a board sensor.");
  page += "</div>";
  if (locErr || geoSearchFailed) {
    page += "<div class='loc-error'>";
    page += tr("No se encontró ese lugar. Se mantiene la ubicación anterior.", "That place was not found. The previous location is kept.");
    page += "</div>";
  }
  page += "<div><label class='label'>";
  page += tr("Ciudad o lugar", "City or place");
  page += "</label><input name='locquery' maxlength='80' placeholder='";
  page += tr("Ej: Buenos Aires", "e.g. Buenos Aires");
  page += "' value='" + htmlEscape(geoLastQuery.length() ? geoLastQuery : locationName) + "'></div>";
  page += "<button type='submit' class='btn-secondary' formaction='/geosearch'>";
  page += tr("Buscar ciudad", "Search city");
  page += "</button>";
  if (geoHitCount > 0) {
    page += "<div class='geo-list'>";
    for (int i = 0; i < geoHitCount; i++) {
      String label = formatGeoName(geoHits[i]);
      page += "<label class='geo-item'><input type='radio' name='geopick' value='";
      page += String(i);
      page += "'";
      if (i == geoSelected || (geoSelected < 0 && i == 0)) page += " checked";
      page += "><span><strong>";
      page += htmlEscape(label);
      page += "</strong><div class='muted'>";
      page += String(geoHits[i].lat, 4);
      page += ", ";
      page += String(geoHits[i].lng, 4);
      page += "</div></span></label>";
    }
    page += "</div>";
  }
  page += "<div class='footer-note'>";
  page += htmlEscape(locationName);
  page += " &middot; ";
  page += String(LAT, 4);
  page += ", ";
  page += String(LNG, 4);
  page += "</div></div>";
  page += "<div class='settings-block'><span class='settings-title'>WiFi</span>";
  page += "<div class='settings-desc'>";
  page += tr("Si no hay red, Deskbuddy abre el punto de acceso Deskbuddy en 192.168.4.1.", "If there is no network, Deskbuddy opens the Deskbuddy access point at 192.168.4.1.");
  page += "</div>";
  page += "<div class='grid'><div><label class='label'>SSID</label><input name='wifiSsid' maxlength='32' value='";
  page += htmlEscape(wifiSsid);
  page += "'></div><div><label class='label'>";
  page += tr("Clave WiFi", "WiFi password");
  page += "</label><input type='password' name='wifiPass' maxlength='64' placeholder='";
  page += tr("Dejar vacio para no cambiar", "Leave empty to keep current");
  page += "'></div></div></div>";
  page += "<div class='settings-block'><span class='settings-title'>OTA</span>";
  page += "<div class='settings-desc'>";
  page += tr("Actualización de firmware por la web local.", "Firmware update from the local web UI.");
  page += " <a href='/update' style='color:#67e8f9;'>";
  page += tr("Abrir actualizador", "Open updater");
  page += "</a></div></div>";
  page += "</div></div>";

  page += "<div class='panel' data-panel='widgets'>";
  page += "<button type='button' class='panel-toggle' aria-expanded='true'><h2>Widgets</h2><span class='panel-chevron'>&#9662;</span></button>";
  page += "<div class='panel-body'>";
  page += "<p>";
  page += tr("Elegi que widgets van en los cuatro huecos de Inicio, debajo del reloj.", "Choose the four Home widgets under the clock.");
  page += "</p>";
  page += "<div class='grid'>";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    page += "<div><label class='label'>";
    page += homeSlotLabel(i);
    page += "</label><select name='homeSlot";
    page += String(i);
    page += "'>";
    appendHomeWidgetOptions(page, homeSlotKeys[i]);
    page += "</select></div>";
  }
  page += "</div>";
  page += "</div></div>";

  page += "</div><div class='stack'>";

  page += "<button type='submit'>";
  page += tr("Guardar en Deskbuddy", "Save to Deskbuddy");
  page += "</button>";
  page += "</div></div></form>";
  page += "<script>";
  if (useEn()) {
    page += "var colorNames={accent:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},text:{standard:'Standard',ice:'Ice',white:'White',cyan:'Cyan',mint:'Mint',green:'Green',blue:'Blue',purple:'Purple',pink:'Pink',orange:'Orange',amber:'Amber',red:'Red'},bg:{slate:'Slate',deep:'Black',nordic:'Nordic',forest:'Forest',coffee:'Coffee',soft:'Soft dark',midnight:'Midnight',graphite:'Graphite',garnet:'Garnet',ochre:'Ochre'}};";
  } else {
    page += "var colorNames={accent:{standard:'Estandar',ice:'Hielo',white:'Blanco',cyan:'Cian',mint:'Menta',green:'Verde',blue:'Azul',purple:'Violeta',pink:'Rosa',orange:'Naranja',amber:'Ambar',red:'Rojo'},text:{standard:'Estandar',ice:'Hielo',white:'Blanco',cyan:'Cian',mint:'Menta',green:'Verde',blue:'Azul',purple:'Violeta',pink:'Rosa',orange:'Naranja',amber:'Ambar',red:'Rojo'},bg:{slate:'Pizarra',deep:'Negro',nordic:'Nordico',forest:'Bosque',coffee:'Cafe',soft:'Oscuro suave',midnight:'Medianoche',graphite:'Grafito',garnet:'Granate',ochre:'Ocre'}};";
  }
  page += "var panelStorageKey='deskbuddy-panel-state-v1';";
  page += "document.querySelectorAll('.swatch input').forEach(function(input){";
  page += "input.addEventListener('change',function(){";
  page += "document.querySelectorAll('.swatch input[name=\"'+input.name+'\"]').forEach(function(peer){";
  page += "peer.closest('.swatch').classList.toggle('active', peer.checked);";
  page += "});";
  page += "var valueEl=document.getElementById(input.name+'-value');";
  page += "if(valueEl&&colorNames[input.name]&&colorNames[input.name][input.value]){valueEl.textContent=colorNames[input.name][input.value];}";
  page += "});";
  page += "});";
  page += "function readPanelState(){try{return JSON.parse(localStorage.getItem(panelStorageKey)||'{}');}catch(e){return {};}}";
  page += "function writePanelState(state){localStorage.setItem(panelStorageKey,JSON.stringify(state));}";
  page += "function applyPanelState(panel,collapsed){panel.classList.toggle('collapsed',collapsed);var btn=panel.querySelector('.panel-toggle');if(btn){btn.setAttribute('aria-expanded',collapsed?'false':'true');}}";
  page += "var savedPanelState=readPanelState();";
  page += "document.querySelectorAll('.panel[data-panel]').forEach(function(panel){";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(Object.prototype.hasOwnProperty.call(savedPanelState,panelId)){applyPanelState(panel,!!savedPanelState[panelId]);}";
  page += "});";
  page += "document.querySelectorAll('.panel-toggle').forEach(function(btn){";
  page += "btn.addEventListener('click',function(){";
  page += "var panel=btn.closest('.panel');";
  page += "var collapsed=!panel.classList.contains('collapsed');";
  page += "applyPanelState(panel,collapsed);";
  page += "var state=readPanelState();";
  page += "var panelId=panel.getAttribute('data-panel');";
  page += "if(panelId){state[panelId]=collapsed;writePanelState(state);}";
  page += "});";
  page += "});";
  page += "</script>";
  page += "</div></body></html>";

  server.send(200, "text/html; charset=utf-8", page);
}

void handleSave() {
  String newAccent = server.hasArg("accent") ? server.arg("accent") : "cyan";
  String newBg     = server.hasArg("bg") ? server.arg("bg") : "slate";
  String newText   = server.hasArg("text") ? server.arg("text") : "standard";
  String newUnits  = server.hasArg("units") ? server.arg("units") : "metric";
  String newRegion = server.hasArg("region") ? server.arg("region") : "europe";
  String newTz     = server.hasArg("tz") ? server.arg("tz") : timezoneKey;
  String newLang   = server.hasArg("lang") ? server.arg("lang") : langKey;
  String locQuery  = server.hasArg("locquery") ? server.arg("locquery") : (server.hasArg("locname") ? server.arg("locname") : locationName);
  String newNickname = server.hasArg("nickname") ? server.arg("nickname") : buddyNickname;
  HomeWidgetType newHomeSlots[HOME_SLOT_COUNT];
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    String currentKey = homeWidgetKey(homeWidgetSlots[i]);
    newHomeSlots[i] = homeWidgetFromKey(server.hasArg(key) ? server.arg(key) : currentKey);
  }

  String newLoc = locationName;
  float newLat = LAT;
  float newLng = LNG;
  bool geocodeFailed = false;
  bool cityPicked = false;
  String cityTimezone = "";

  locQuery.trim();
  newNickname.trim();
  if (newLang != "en") newLang = "es";
  if (newNickname.length() > 24) newNickname = newNickname.substring(0, 24);

  int pick = server.hasArg("geopick") ? server.arg("geopick").toInt() : -1;
  if (pick >= 0 && pick < geoHitCount) {
    newLoc = formatGeoName(geoHits[pick]);
    newLat = geoHits[pick].lat;
    newLng = geoHits[pick].lng;
    cityTimezone = geoHits[pick].timezone;
    cityPicked = true;
    geoSelected = pick;
  } else if (locQuery.length() > 0 && locQuery != locationName && locQuery != geoLastQuery) {
    int found = geocodeSearch(locQuery);
    if (found == 1) {
      newLoc = formatGeoName(geoHits[0]);
      newLat = geoHits[0].lat;
      newLng = geoHits[0].lng;
      cityTimezone = geoHits[0].timezone;
      cityPicked = true;
    } else if (found > 1) {
      server.sendHeader("Location", "/?pick=1");
      server.send(303);
      return;
    } else {
      geocodeFailed = true;
      Serial.println("[GEO] Se mantiene la ubicacion anterior.");
    }
  }
  if (newUnits != "metric" && newUnits != "imperial") newUnits = "metric";
  if (newRegion != "europe" && newRegion != "us") newRegion = "europe";
  newTz = sanitizeTimezoneKey(newTz);

  int newSleepMin = server.hasArg("sleepMin") ? server.arg("sleepMin").toInt() : sleepIntervalMin;
  sleepIntervalMin = constrain(newSleepMin, 0, 120);
  bool newFlashMode = server.hasArg("flashMode");

  bool locationChanged =
    (fabsf(newLat - LAT) > 0.0001f) ||
    (fabsf(newLng - LNG) > 0.0001f) ||
    (newLoc != locationName);

  buddyNickname = newNickname;
  locationName = newLoc;
  LAT = newLat;
  LNG = newLng;
  unitKey = newUnits;
  regionFormatKey = newRegion;
  langKey = newLang;
  flashModeEnabled = newFlashMode;
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    homeWidgetSlots[i] = newHomeSlots[i];
  }

  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    int currentValue = timerPresetMin[i];
    int nextValue = server.hasArg(key) ? server.arg(key).toInt() : currentValue;
    timerPresetMin[i] = sanitizeTimerMinutes(nextValue);
  }

  pomoWorkMin  = sanitizePomoWorkMin(server.hasArg("pomoWork") ? server.arg("pomoWork").toInt() : pomoWorkMin);
  pomoShortMin = sanitizePomoShortMin(server.hasArg("pomoShort") ? server.arg("pomoShort").toInt() : pomoShortMin);
  pomoLongMin  = sanitizePomoLongMin(server.hasArg("pomoLong") ? server.arg("pomoLong").toInt() : pomoLongMin);

  prefs.putString("accent", newAccent);
  prefs.putString("bg", newBg);
  prefs.putString("text", newText);
  prefs.putString("units", unitKey);
  prefs.putString("region", regionFormatKey);
  prefs.putString("lang", langKey);
  prefs.putString("nickname", buddyNickname);
  prefs.putString("locname", locationName);
  prefs.putFloat("lat", LAT);
  prefs.putFloat("lng", LNG);
  prefs.putInt("sleepMin", sleepIntervalMin);
  prefs.putBool("flashMode", flashModeEnabled);
  prefs.putInt("pomoWork", pomoWorkMin);
  prefs.putInt("pomoShort", pomoShortMin);
  prefs.putInt("pomoLong", pomoLongMin);
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    String key = String("homeSlot") + String(i);
    prefs.putString(key.c_str(), homeWidgetKey(homeWidgetSlots[i]));
  }
  for (int i = 0; i < 6; i++) {
    String key = String("timer") + String(i);
    prefs.putInt(key.c_str(), timerPresetMin[i]);
  }

  applyThemeByKey(newAccent, newBg);
  applyTextColorByKey(newText);
  if (cityPicked && cityTimezone.length()) {
    applyTimezoneFromIana(cityTimezone);
  } else {
    applyDeviceTimezoneByKey(newTz);
  }
  persistTimezonePrefs();

  if (server.hasArg("wifiSsid")) {
    String nextSsid = server.arg("wifiSsid");
    String nextPass = server.hasArg("wifiPass") ? server.arg("wifiPass") : "";
    nextSsid.trim();
    if (nextSsid.length()) {
      wifiSsid = nextSsid;
      if (nextPass.length()) wifiPass = nextPass;
      prefs.putString("wifiSsid", wifiSsid);
      prefs.putString("wifiPass", wifiPass);
      beginWiFiConnect();
    }
  }

  if (!sleepDimmed && !sleepOff) setBacklight(BL_FULL);

  pageDirty = true;
  dataDirty = true;

  cacheClock = "";
  cacheHomeEmpty1 = "";
  cacheHomeEmpty2 = "";
  cacheFocusTimer = "";
  cacheTimerMenu = "";
  cacheTimerDone = "";
  for (int i = 0; i < HOME_SLOT_COUNT; i++) {
    cacheHomeSlots[i] = "";
  }

  lastTempText = "";
  lastRainText = "";
  lastKpText = "";
  lastKpLevelText = "";
  lastWindText = "";
  lastWindDirText = "";
  lastNextSunLabel = "";
  lastNextSunTime = "";
  lastUptimeText = "";

  if (locationChanged) resetDataCaches();

  server.sendHeader("Location", geocodeFailed ? "/?locerr=1" : "/");
  server.send(303);
}

void handleGeoSearch() {
  String q = server.hasArg("locquery") ? server.arg("locquery") : "";
  q.trim();
  int found = geocodeSearch(q);
  if (found <= 0) {
    server.sendHeader("Location", "/?locerr=1");
  } else {
    server.sendHeader("Location", "/?pick=1");
  }
  server.send(303);
}

void handleUpdateGet() {
  String page;
  page += "<!doctype html><html lang='";
  page += useEn() ? "en" : "es";
  page += "'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>OTA</title><style>body{font-family:system-ui,sans-serif;background:#111827;color:#edf2f7;padding:24px;}form{max-width:420px;display:grid;gap:12px;}input,button{padding:12px;border-radius:10px;border:1px solid #334155;background:#0b1220;color:#edf2f7;}button{background:#38bdf8;color:#001018;font-weight:800;border:none;}a{color:#67e8f9;}</style></head><body>";
  page += "<h1>OTA</h1><p>";
  page += tr("Subí un firmware .bin compilado para este ESP32. Confirmá antes de enviar.", "Upload a .bin firmware built for this ESP32. Confirm before sending.");
  page += "</p><form method='POST' action='/update' enctype='multipart/form-data'>";
  page += "<input type='file' name='firmware' accept='.bin' required>";
  page += "<label><input type='checkbox' name='confirm' value='1' required> ";
  page += tr("Confirmo actualizar Deskbuddy", "I confirm updating Deskbuddy");
  page += "</label><button type='submit'>";
  page += tr("Instalar firmware", "Install firmware");
  page += "</button></form><p><a href='/'>";
  page += tr("Volver", "Back");
  page += "</a></p></body></html>";
  server.send(200, "text/html; charset=utf-8", page);
}

void handleUpdateFinish() {
  server.sendHeader("Connection", "close");
  if (Update.hasError()) {
    server.send(500, "text/plain", tr("Fallo la actualizacion", "Update failed"));
  } else {
    server.send(200, "text/plain", "OK");
    delay(400);
    ESP.restart();
  }
}

void handleUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA start %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Serial.println("OTA begin failed");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Serial.println("OTA write failed");
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA end %u bytes\n", (unsigned)upload.totalSize);
    } else {
      Serial.println("OTA end failed");
    }
  }
}

void handleNotFound() {
  if (wifiApMode) {
    server.sendHeader("Location", "http://192.168.4.1/");
    server.send(302);
    return;
  }
  server.send(404, "text/plain", "Not found");
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/geosearch", HTTP_POST, handleGeoSearch);
  server.on("/update", HTTP_GET, handleUpdateGet);
  server.on("/update", HTTP_POST, handleUpdateFinish, handleUpdateUpload);
  server.on("/generate_204", HTTP_GET, handleRoot);
  server.on("/hotspot-detect.html", HTTP_GET, handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
}

// =========================================================
// SETUP / LOOP
// =========================================================
void waitForNtpTime() {
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < 1700000000 && millis() - t0 < 10000) {
    delay(200);
    now = time(nullptr);
  }
}

void logWiFiStatus(const char* prefix) {
  Serial.printf("%s ssid=%s status=%d rssi=%d ip=%s\n",
                prefix,
                wifiSsid.c_str(),
                (int)WiFi.status(),
                WiFi.RSSI(),
                wifiApMode ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str());
}

void stopSetupPortal() {
  if (!wifiApMode) return;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  wifiApMode = false;
  Serial.println("WiFi setup portal stopped");
  pageDirty = true;
}

void startSetupPortal() {
  if (wifiApMode) return;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  delay(120);
  IPAddress ip = WiFi.softAPIP();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", ip);
  wifiApMode = true;
  wifiConnectInProgress = false;
  Serial.printf("WiFi setup portal SSID=%s ip=%s\n", AP_SSID, ip.toString().c_str());
  pageDirty = true;
  dataDirty = true;
}

void beginWiFiConnect() {
  if (!wifiEnabled) {
    stopSetupPortal();
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    wifiConnectInProgress = false;
    Serial.println("WiFi disabled");
    return;
  }

  stopSetupPortal();
  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname("deskbuddy");
  Serial.printf("WiFi connecting to %s...\n", wifiSsid.c_str());
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  wifiConnectInProgress = true;
  wifiConnectStartedMs = millis();
}

void connectWiFi(bool waitForConnection = true) {
  beginWiFiConnect();
  if (!waitForConnection) return;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  wifiConnectInProgress = (WiFi.status() != WL_CONNECTED);
  logWiFiStatus("WiFi result");
}

void updateWiFiConnectionState() {
  if (!wifiEnabled) return;

  if (wifiConnectInProgress) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnectInProgress = false;
      stopSetupPortal();
      ensureSunTimesForToday();
      ensureWeather();
      ensureAirQuality();
      ensureKpIndex();
      dataDirty = true;
      pageDirty = true;
      return;
    }
    if (millis() - wifiConnectStartedMs >= WIFI_CONNECT_TIMEOUT_MS) {
      wifiConnectInProgress = false;
      startSetupPortal();
      pageDirty = true;
    }
  }
}

void setWifiEnabled(bool enabled) {
  wifiEnabled = enabled;
  prefs.putBool("wifiEnabled", wifiEnabled);

  if (!wifiEnabled) {
    wifiConnectInProgress = false;
    stopSetupPortal();
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
  } else {
    beginWiFiConnect();
  }

  dataDirty = true;
  pageDirty = true;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("Deskbuddy boot");

  pinMode(BACKLIGHT_PIN, OUTPUT);
  analogWrite(BACKLIGHT_PIN, BL_FULL);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  loadStoredSettings();
  wifiEnabled = true;
  Serial.printf("Settings loaded, wifiEnabled=%d\n", wifiEnabled);
  setBacklight(BL_FULL);
  lastInteractionMs = millis();

  beginWiFiConnect();

  Serial.println("TFT init");
  tft.init();
  tft.setRotation(ROT);
  tft.invertDisplay(INV);
  tft.setSwapBytes(true);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(tr("Iniciando Deskbuddy...", "Starting Deskbuddy..."), 10, 10, 2);

  touchSPI.begin(T_SCK, T_MISO, T_MOSI);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  ts.begin(touchSPI);
  ts.setRotation(ROT);

  setupWebServer();

  tft.drawString(tr("Conectando WiFi...", "Connecting WiFi..."), 10, 34, 2);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    server.handleClient();
    Serial.print('.');
  }
  Serial.println();
  wifiConnectInProgress = (WiFi.status() != WL_CONNECTED);
  logWiFiStatus("WiFi result");

  if (WiFi.status() == WL_CONNECTED) {
    tft.drawString(tr("Sincronizando hora...", "Syncing time..."), 10, 58, 2);
    const char* posix = (timezoneKey == "auto" && timezonePosixCustom.length())
      ? timezonePosixCustom.c_str()
      : timezonePosixByKey(timezoneKey == "auto" ? "europe_central" : timezoneKey);
    configTzTime(posix, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    waitForNtpTime();
    ensureSunTimesForToday();
    ensureWeather();
    ensureAirQuality();
    ensureKpIndex();
  } else {
    startSetupPortal();
    tft.drawString(tr("Modo setup WiFi", "WiFi setup mode"), 10, 58, 2);
    tft.drawString(AP_SSID, 10, 82, 2);
    tft.drawString("192.168.4.1", 10, 106, 2);
    delay(800);
  }

  pageDirty = true;
  dataDirty = true;

  drawCurrentPageFull();
  updateCurrentPageDynamic();

  lastClockTick = millis();
  lastDataTick = millis();

  Serial.print("Deskbuddy web: http://");
  Serial.println(wifiApMode ? WiFi.softAPIP() : WiFi.localIP());
}

void loop() {
  if (wifiApMode) dnsServer.processNextRequest();
  server.handleClient();
  updateWiFiConnectionState();
  updateFocusTimerState();
  updateTimerDoneDialogState();
  handleAutoSleep();

  int tx = 0, ty = 0;
  if (touchNewPress(tx, ty)) {
    lastInteractionMs = millis();

    if (sleepOff) {
      if (manualDimMode) {
        sleepOff = false;
        sleepDimmed = true;
        setBacklight(BL_DIM);
        pageDirty = true;
      } else {
        wakeDisplay();
      }
      return;
    }

    if (handleTimerDoneDialogTouch(tx, ty)) {
      return;
    }

    if (tx >= SCREEN_W - 36 && ty <= TOPBAR_H) {
      toggleSleepMode();
      pageDirty = true;
    } else {
      if (sleepDimmed) {
        if (!manualDimMode) {
          wakeDisplay();
        } else {
          if (!handleHomeTouch(tx, ty) && !handlePomoTouch(tx, ty) && !handleStatusTouch(tx, ty)) {
            handleNavTouch(tx, ty);
          }
        }
      } else {
        if (!handleHomeTouch(tx, ty) && !handlePomoTouch(tx, ty) && !handleStatusTouch(tx, ty)) {
          handleNavTouch(tx, ty);
        }
      }
    }
  }

  if (millis() - lastDataTick >= DATA_TICK_MS) {
    lastDataTick = millis();
    ensureSunTimesForToday();
    ensureWeather();
    ensureAirQuality();
    ensureKpIndex();
  }

  if (pageDirty || lastDrawnPage != currentPage) {
    drawCurrentPageFull();
    updateCurrentPageDynamic();
    pageDirty = false;
    dataDirty = false;
  }

  if (millis() - lastClockTick >= CLOCK_TICK_MS) {
    lastClockTick = millis();
    updateCurrentPageDynamic();
    dataDirty = false;
  }

  delay(10);
}
