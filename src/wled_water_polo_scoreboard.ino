/* =================================================================
 * MATRIX32: 400 LED Matrix with 100 LEDs/m for ESP32
 * February, 2025
 * Version 0.26
 * Copyright ResinChemTech - released under the Apache 2.0 license
 * ================================================================= */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "time.h"
#include <HTTPClient.h>         //For syncing weather from OpenWeatherMap (HTTP Get command)
#include "esp_sntp.h"
#include <ESP32Time.h>          //For setting and getting RTC time from ESP32: https://github.com/fbiego/ESP32Time (v2.0.6)
#include <ArduinoJson.h>        //Needed for saved config file: https://arduinojson.org/ (v7.2.0)
#include <Wire.h>
#include <WiFiUdp.h>
// #include <DFRobot_AHT20.h>      //AHT20 temp/humidity sensor: https://github.com/DFRobot/DFRobot_AHT20 (v1.0.0)
// #include <ESP32RotaryEncoder.h> //Rotary Encoder: https://github.com/MaffooClock/ESP32RotaryEncoder (v1.1.0)
#include <ArduinoOTA.h>         //OTA Updates via Arduino IDE
#include <Update.h>             //OTA Updates via web page
#include "html.h"               //html code for the firmware update page
#define FASTLED_INTERNAL        //Suppress FastLED SPI/bitbanged compiler warnings (only applies after first compile)
#include <FastLED.h>

#include "homepage_js.h" // Include the auto-generated header
#include "game_and_shot_clk_js.h" // Include the auto-generated header
#include "shot_clk_js.h" // Include the auto-generated header
#include "game_clk_js.h" // Include the auto-generated header
#include "utils_js.h" // Include the auto-generated header
#include "shot_clk_only_js.h" // Include the auto-generated header
#include "game_clk_only_js.h" // Include the auto-generated header

#define VERSION "v0.26 (ESP32)"
#define APPNAME "MATRIX CLOCK"
#define TIMEZONE "EST+5EDT,M3.2.0/2,M11.1.0/2"        // Set your custom time zone from this list: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define GMT_OFFSET -5                                 // Manually set time zone offset hours from GMT (e.g EST = -5) - only used if useCustomOffsets is true below
#define DST_OFFSET 1                                  // Manually set DST hour adjustment (add one hour) - only used if useCustomOffsets is true below
#define SYNC_INTERVAL 15                              // How often, in minutes, to sync to NTP server (15-60 minutes recommended) - can be changed via web app
#define WIFIMODE 2                                    // 0 = Only Soft Access Point, 1 = Only connect to local WiFi network with UN/PW, 2 = Both (both needed for onboarding)
#define SERIAL_DEBUG 1                                // 0 = Disable (must be disabled if using RX/TX pins), 1 = enable
#define FORMAT_LITTLEFS_IF_FAILED true                // DO NOT CHANGE
#define NUMELEMENTS(x) (sizeof(x) / sizeof(x[0]))     // DO NOT CHANGE - number of elements in array
// ==============================================================================================
//  *** REVIEW AND UPDATE THESE VARIABLES TO MATCH YOUR ENVIRONMENT/BUILD ***
// ==============================================================================================
//Pin Definitions - Update if your build is different
#define BUS1_SDA 21       //I2C Bus 1 Data (GY-302 Light Sensor)
#define BUS1_SCL 22       //I2C Bus 1 Clock (GY-302 Light Sensor)
#define BUS2_SDA 17       //I2C Bus 2 Data (AHT20 Temp/Humidity Sensor)
#define BUS2_SCL 19       //I2C Bus 2 Clock (AHT20 Temp/Humidity Sensor)
#define BUZZER_OUTPUT 13  //Output pin to drive buzzer or other device upon countdown expiration
#define LED_DATA_PIN 2   //LED Data Output
#define MODE_PIN 25       //Push button (white): Change Mode
#define GREEN_PIN 26      //Push button (green - old V0):
#define RED_PIN 27        //Push button (red - old V1):
#define ENCODER_A 32      //Rotary Encoder CLK
#define ENCODER_B 33      //Rotary Encoder DAT
#define ENCODER_SW 35     //Rotary Encoder Switch (push button)
#define NUM_LEDS 1024      //Total of 400 LED's if matrix built as shown
#define MILLI_AMPS 5000   //Update to match <80% max milliamp output of your power suppy (e.g. 20A supply = 16000 max milliamps). Do not set above 15000.
#define MAX_NUM_LEDS_PER_SEGMENT 5 //Max Number of LEDs in each 7 segment digit
#define NUM_SEPARATOR_SEGS 2 //Number of segments in the separator (colon) for game clock
#define PIXELS_PER_SEPARATOR_SEG 2

enum ClockMode {
    GameAndShotClkMode,    
    GameClkMode,  
    ShotClkMode,
    CntDwn60Sec    
};

/* ================ Default Starting Values ================
 * All of these following values can be modified and saved via
 * the web interface and the values below normally do not need to be modified.
 * Using the web settings is the preferred way to change these default values.  
 * Values listed here are just starting values before the config file exists or
 * if it can't be read.
 */

String ntpServer = "pool.ntp.org";      // Default server for syncing time
String owmKey = "NA";                   // OpenWeatherMap API key (can be entered after onboarding via web app)
String owmLat = "39.8083";              // Your Latitude for OpenWeatherMap (can be entered after onboarding
String owmLong = "-98.555";             // Your Longitude for OpenWeatherMap (can be entered after onboarding)
bool tempExtUseApi = false;             // Use local API for external temperature instead of OWM
bool tempIntUseApi = false;             // Use local API for internal temperature instead of onboard sensor

String timeZone = TIMEZONE;             // Loaded from #define above.  Change #define if you wish to change this value.
bool useCustomOffsets = false;          // Set to true to use custom offsets instead of timezone (need for some zones that use 1/2 hours - no auto-DST adjustments)
long gmtOffsetHours = -5;               // Example: EST is -5 hours from GMT for standard hours (only used if useCustomOffsets = true)
int dstOffsetHours = 1;                 // Example: 1 'springs forward' one hour.  Set to 0 if your zone does not observe DST (only used if useCustomOffsets = true)
ClockMode defaultClockMode = GameAndShotClkMode;              
bool binaryClock = false;               // Show clock in binary mode (no temperature, 24-hour mode only)
byte brightness = 64;                   // Default starting brightness at boot. 255=max brightness (based on milliamp rating of power supply)
byte numFont = 1;                       // Default Large number font: 0-Original 7-segment (21 pixels), 1-modern (31 pixels), 2-hybrid (28 pixels)
byte temperatureSymbol = 13;            // Default temp display: 12=Celcius, 13=Fahrenheit
byte temperatureSource = 0;             // 0 Dual inside/outside temp, 1 Exterior temp only (provided via OpenWeatherMap), 2 Internal only (ATH20 sensor)
float temperatureCorrection = 0;        // Temp correction for AHT20 module.  Generally runs slightly "hot" due to heat from chip.  Adjust as needed via settings.
byte hourFormat = 12;                   // Change this to 24 if you want default 24 hours format instead of 12
String scoreboardTeamLeft = "&V";       // Default visitor (left) team name on scoreboard
String scoreboardTeamRight = "&H";      // Default home (right) team name on scoreboard
bool useBuzzer = true;                  // Sound buzzer when countdown time expires
String textTop = "HELLO";               // Default top line for text display
String textBottom = "WORLD";            // Default bottom line for text display
byte textEffect = 0;                    // Text effect to supply (0 = none, see documentation for others)
byte textEffectSpeed = 1;               // 1 = slow (1 sec) to max of 10 (.1 second)

bool autoSync = true;                   //Auto-sync to NTP server
int autoSyncInterval = 60;              //How often, in minutes, to sync the time.  Ignored if autoSync=false.

String wledAddress = "0.0.0.0";         // IP Address of WLED Controller (set to 0.0.0.0 if not used)
byte wledMaxPreset = 0;                //Max preset number to use for button cycle
             

//Define Color Arrays
//If adding new colors, increase array sizes and add to void defineColors()
CRGB ColorCodes[25];
String WebColors[25];

//===========================================================================================
// Do not change any values below this point unless you are sure you know what you are doing!
//===========================================================================================
CRGB clockColor = CRGB::Blue;              // Main time display color
CRGB temperatureColorInt = CRGB::Red;      // External temperature color
CRGB temperatureColorExt = CRGB::Blue;     // Internal temperature color
CRGB countdownColor = CRGB::Green;         // Active countdown timer color
CRGB countdownColorPaused = CRGB::Orange;  // If different from countdownColor, countdown color will change to this when paused/stopped.
CRGB countdownColorFinalMin = CRGB::Red;   // If different from countdownColor, countdown color will change to this for final 60 seconds.
CRGB scoreboardColorLeft = CRGB::Green;    // Color for left (visitor) score
CRGB scoreboardColorRight = CRGB::Red  ;   // Color for right (home) score
CRGB textColorTop = CRGB::Red;             // Color for top text row
CRGB textColorBottom = CRGB::Green;        // Color for bottom text row
CRGB alternateColor = CRGB::Black;         // Recommend to leave as Black. Otherwise unused pixels will be lit in digits

//For web interface.  The value should match the webColor[] index for the matching color above.  These values can be change via web app.
byte webColorClock = 0;              //Blue
byte webColorTemperatureInt = 3;     //Red
byte webColorTemperatureExt = 0;     //Blue
byte webColorCountdown = 1;          //Green
byte webColorCountdownPaused = 2;    //Orange
byte webColorCountdownFinalMin = 3;  //Red
byte webColorScoreboardLeft = 1;     //Green
byte webColorScoreboardRight = 3;    //Red
byte webColorTextTop = 3;            //Red
byte webColorTextBottom = 1;         //Green

//Local Variables (wifi/onboarding)
String deviceName = "MatrixClock";  //Default Device Name - 16 chars max, no spaces.
String wifiHostName = deviceName;
String wifiSSID = "";
String wifiPW = "";
byte macAddr[6];                //Array of device mac address as hex bytes (reversed)
String strMacAddr;              //Formatted string of device mac address
String baseIPAddress;           //Device assigned IP Address
bool onboarding = false;        //Will be set to true if no config file or wifi cannot be joined
long daylightOffsetHours = 0;   //EDT: offset of 1 hour (GMT -4) during DST dates (set in code when time obtained)
int milliamps = MILLI_AMPS;     //Limited to 5,000 - 25,000 milliamps.

//OTA Variables
String otaHostName = deviceName + "_OTA";  // Will be updated by device name from onboarding + _OTA
bool ota_flag = true;                      // Must leave this as true for board to broadcast port to IDE upon boot
uint16_t ota_boot_time_window = 2500;      // minimum time on boot for IP address to show in IDE ports, in millisecs
uint16_t ota_time_window = 20000;          // time to start file upload when ota_flag set to true (after initial boot), in millsecs
uint16_t ota_time_elapsed = 0;             // Counter when OTA active
uint16_t ota_time = ota_boot_time_window;

uint8_t web_otaDone = 0;        //Web OTA Update

ClockMode clockMode = defaultClockMode;
byte oldMode = 0;
int oldTemp = 0;

//Misc. App Variables
byte holdBrightness = brightness;         //Hold variable for toggling LEDs off/on
bool ledsOn = true;                       //Set to false when LEDs turned off via web or rotary knob.
byte scoreboardLeft = 0;                  // Starting "Visitor" (left) score on scoreboard
byte scoreboardRight = 0;                 // Starting "Home" (right) score on scoreboard
unsigned long tempUpdatePeriod = 60;      //Seconds to elapse before updating temp display in clock mode. Default 1 minute if update period not specified in Settings
unsigned long tempUpdateCount = 0;
unsigned long tempUpdatePeriodExt = 600;  //Seconds to elapse before updating external temp from OWM.  Min. 600 recommended so as not to exceed daily API call allotment
unsigned long tempUpdateCountExt = 0;
int externalTemperature = 0;              // Will only be used if external temp selected in Settings
int internalTemperature = 0;
long lastReconnectAttempt = 0;
unsigned long prevTime = 0;
unsigned long prevAutoTime = 0;           // For auto on/off feature
bool useWLED = false;                     // Indicates whether secondary WLED controller is present (based on non-zero IP address)
bool wledStateOn = false;                 // Current state of WLED (can get out of sync if changed via WLED interface)
byte wledCurPreset = 0;                   // Current active preset (used and set by local pushbuttons)

//Flags for rotary encoder turns
volatile bool turnedRightFlag = false;
volatile bool turnedLeftFlag = false;

//Rotary encoder button debouncing
unsigned long lastDebounceTime = 0;         
unsigned long debounceDelay = 250;          //millisecond delay
int startButtonState;                       //For tracking state
int lastButtonState = HIGH;                 //Set initial state (HIGH = not pressed)

//auto On-Off times and settings (v0.26)
bool autoOnOff = false;
bool autoOnValid = false;
bool autoOffValid = false;
byte autoOffHr = 0;
byte autoOffMin = 0;
int autoOffBrightness = 0;
byte autoOnHr = 0;
byte autoOnMin = 0;
int autoOnBrightness = 0;

byte r_val = 255;  // RGB values for randomizing colors when using rainbow effect
byte g_val = 0;
byte b_val = 0;
bool dotsOn = true;
byte defaultCountdownMin = 0;
byte defaultCountdownSec = 0;
unsigned long countdownMilliSeconds;
unsigned long endCountDownMillis;
bool timerRunning = false;
unsigned long remCountdownMillis = 0;   // Stores value on pause/resume
unsigned long initCountdownMillis = 0;  // Stores initial last countdown value for reset
//Variables for processing text effects
unsigned int textEffectPeriod = 1000;  // Update period for text effects in millis (min 250 max 2500)
bool effectTextHide = false;           // For text Flash and FlashAlternate effects
int effectBrightness = 0;              // For text Fade In and Fade Out effects
byte oldTextEffect = 0;                // For determining effect switch and to setup new effect
int appearCount = 99;                  // Counter for Appear and Appear Flash effects

//Instaniate objects
time_t now;
tm timeinfo;
WiFiUDP ntpUDP;
HTTPClient http;
ESP32Time rtc(0);  // offset handled via NTP time server

TwoWire bus1 = TwoWire(0);     //I2C Bus 1
TwoWire bus2 = TwoWire(1);     //I2C Bus 2
// DFRobot_AHT20 aht(bus2);

//RotaryEncoder rotaryEncoder(ENCODER_A, ENCODER_B, ENCODER_SW);
// RotaryEncoder rotaryEncoder(ENCODER_A, ENCODER_B);

WebServer server(80);
CRGB LEDs[NUM_LEDS];

const unsigned long shotClkMilliSecRstCnt = 30000;  //30 seconds
long lastShotClkMilliSecCnt = shotClkMilliSecRstCnt;
long shotClkMilliSecCnt = shotClkMilliSecRstCnt;
bool runShotClk;
bool enShotClk;   //Display shot clock on matrix

const unsigned long gameClkMilliSecRstCnt = 420000;  //7 min = 420 seconds
long lastGameClkMilliSecCnt = gameClkMilliSecRstCnt;
long gameClkMilliSecCnt = gameClkMilliSecRstCnt;
bool runGameClk;
bool enGameClk;   //Display game clock on matrix
bool syncGameAndShotClks = true;  //Set to true to sync starting and stopping of the game and shot clocks 

int test = 0;

//---- Create arrays for characters: These turn on indiviual pixels to create letters/numbers
//7 segments Digits
//Segment Order
// 0 = Top, 1 = Top Right, 2 = Bottom Right, 3 = Bottom, 4 = Bottom Left, 5 = Top Left, 6 = Middle
unsigned long SevenSegDigits[10] = {
  // 7-Segment Font
    0b0111111,  // [0,0] 0
    0b0000110,  // [1] 1
    0b1011011,  // [2] 2
    0b1001111,  // [3] 3
    0b1100110,  // [4] 4
    0b1101101,  // [5] 5
    0b1111101,  // [6] 6
    0b0000111,  // [7] 7
    0b1111111,  // [8] 8
    0b1101111,  // [9] 9
};

//Pixel Locations for 7 segment digits.
// Negative one indicates and unsed pixel
int shotClkPixelPos[2][7][MAX_NUM_LEDS_PER_SEGMENT] = {
  {{974,961,958,945,-1},{945,944,79,78, 77},{77,76,75,74,73},{73,70,57,54,-1},{54,53,52,51,50},{50,49,48,975,974},{50,61,66,77,-1}},  //[0] Unit Seconds
  {{1009,1006,993,990,-1},{990,991,32,33,34},{34,35,36,37,38},{38,25,22,9,-1},{9,10,11,12,13},{13,14,15,1008,1009},{13,18,29,34,-1}},  //[1] Ten Seconds
  };

int gameClkPixelPos[3][7][MAX_NUM_LEDS_PER_SEGMENT] = {
  {{545,542,529,526,-1},{526,527,496,497,498},{497,499,500,501,502},{502,489,486,473,-1},{473,474,475,476,477},{477,478,479,544,545},{477,482,493,498,-1}},  //[0] Unit Seconds
  {{590,577,574,561,-1},{561,560,463,462,461},{461,460,459,458,457},{457,454,441,438,-1},{438,437,436,435,434},{434,433,432,591,590},{434,445,450,461,-1}},  //[1] Ten Second
  {{641,638,625,622,-1},{622,623,400,401,402},{402,403,404,405,406},{406,393,390,377,-1},{377,378,379,380,381},{381,382,383,640,641},{381,386,397,402,-1}},  //[2] Unit Minutes
  };

int gameClkSeparatorPixelPos[NUM_SEPARATOR_SEGS][PIXELS_PER_SEPARATOR_SEG] = {
  {417,416},  //[0] Top Separator
  {420,421},  //[1] Bottom Separator
  };

//===========================
// Populate Color Arrays
//===========================
void defineColors() {
  //  Increase array sizes (ColorCodes[] and WebColors[]) in declarations above if adding new
  //  Color must be defined as a CRGB::Named Color or as a CRGB RGB value: CRGB(r, g, b);
  //  Each ColorCode[] value must have a matching WebColor[] value with the color plain text description

  ColorCodes[0] = CRGB::Blue;
  ColorCodes[1] = CRGB::Green;
  ColorCodes[2] = CRGB::Orange;
  ColorCodes[3] = CRGB::Red;
  ColorCodes[4] = CRGB::Yellow;
  ColorCodes[5] = CRGB::White;
  ColorCodes[6] = CRGB::Black;
  ColorCodes[7] = CRGB::Aqua;
  ColorCodes[8] = CRGB::CadetBlue;
  ColorCodes[9] = CRGB::Coral;
  ColorCodes[10] = CRGB::Crimson;
  ColorCodes[11] = CRGB::Cyan;
  ColorCodes[12] = CRGB::Fuchsia;
  ColorCodes[13] = CRGB::Gold;
  ColorCodes[14] = CRGB::Lavender;
  ColorCodes[15] = CRGB::LightBlue;
  ColorCodes[16] = CRGB::Lime;
  ColorCodes[17] = CRGB::Magenta;
  ColorCodes[18] = CRGB::Maroon;
  ColorCodes[19] = CRGB::Navy;
  ColorCodes[20] = CRGB::Pink;
  ColorCodes[21] = CRGB::Purple;
  ColorCodes[22] = CRGB::Salmon;
  ColorCodes[23] = CRGB::Teal;
  ColorCodes[24] = CRGB::Turquoise;

  WebColors[0] = "Blue";
  WebColors[1] = "Green";
  WebColors[2] = "Orange";
  WebColors[3] = "Red";
  WebColors[4] = "Yellow";
  WebColors[5] = "White";
  WebColors[6] = "Black (off)";
  WebColors[7] = "Aqua";
  WebColors[8] = "Cadet Blue";
  WebColors[9] = "Coral";
  WebColors[10] = "Crimson";
  WebColors[11] = "Cyan";
  WebColors[12] = "Fuchsia";
  WebColors[13] = "Gold";
  WebColors[14] = "Lavender";
  WebColors[15] = "Light Blue";
  WebColors[16] = "Lime";
  WebColors[17] = "Magenta";
  WebColors[18] = "Maroon";
  WebColors[19] = "Navy";
  WebColors[20] = "Pink";
  WebColors[21] = "Purple";
  WebColors[22] = "Salmon";
  WebColors[23] = "Teal";
  WebColors[24] = "Turquoise";
}

//=======================================
// Read config file from flash (LittleFS)
//=======================================
void readConfigFile() {

  if (LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("mounted file system");
    #endif
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
            Serial.println("reading config file");
      #endif
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
                Serial.println("opened config file");
        #endif
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if (!deserializeError) {

          #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
                    Serial.println("\nparsed json");
          #endif
          // Read values here from LittleFS (use defaults for all values in case they don't exist to avoid potential boot loop)
          //DON'T NEED TO STORE OR RECALL WIFI INFO - Written to flash automatically by library when successful connection.
          deviceName = json["device_name"] | "MatrixClock";
          defaultClockMode = ClockMode( json["clock_mode"] | 0);
          int binary = json["binary_clock"] | 0;
          if (binary == 1) {
            binaryClock = true;
          } else {
            binaryClock = false;
          }
          milliamps = json["max_milliamps"] | 5000;
          brightness = json["led_brightness"] | 64;
          numFont = json["num_font"] | 1;
          int autoOpt = json["auto_on_off"] | 0;
          if (autoOpt == 1) {
            autoOnOff = true;
          } else {
            autoOnOff = false;
          }
          autoOffHr = json["auto_off_hr"] | 0;
          autoOffMin = json["auto_off"] | 0;
          autoOffBrightness = json["auto_off_brightness"] | 0;
          autoOnHr = json["auto_on_hr"] | 0;
          autoOnMin = json["auto_on_min"] | 0;
          autoOnBrightness = json["auto_on_brightness"] | 0;
          owmKey = json["owm_key"] | "NA";                        // OpenWeatherMap API Key
          owmLat = json["owm_lat"] | "39.8083";                   // Latitude for OWM (stored as string)
          owmLong = json["owm_long"] | "-98.555";                 // Longitude for OWM (stored as string)
          int useApiExt = json["temp_ext_api"] | 0;               // Use API for external temperature instead of OWM
          if (useApiExt == 1) {
            tempExtUseApi = true;
          } else {
            tempExtUseApi = false;
          }
          int useApiInt = json["temp_int_api"] | 0;               // Use API for internal temperature instead of onboard internal temp sensor
          if (useApiInt == 1) {
            tempIntUseApi = true;
          } else {
            tempIntUseApi = false;
          }
          temperatureSymbol = json["temp_symbol"] | 13;           // 12 celcius, 13 fahrenheit
          temperatureSource = json["temp_source"] | 0;            // 0 dual, 1 external only, 2 internal only
          temperatureCorrection = json["temp_correction"] | 0.00; // Correction (in degrees) value to add to internal temp sensor
          tempUpdatePeriod = json["temp_upd_int"] | 5;            // How often, in minutes, to get a new internal temp reading from sensor (minimum = 1)
          tempUpdatePeriodExt = json["temp_upd_ext"] | 15;        // How often, in minutes, to poll for new external temp value (minimum = 10... otherwise API may exceed daily limit)
          hourFormat = json["hour_format"] | 12;                  // 12 or 24
          timeZone = json["time_zone"] | TIMEZONE;                // Custom tz string to use instead of offsets
          int custom = json["use_custom_tz"] | 0;
          if (custom == 1) {                                      // 1 = use custom offsets instead of timezone string (no auto-DST adjustments)
            useCustomOffsets = true;
          } else {
            useCustomOffsets = false;
          }
          gmtOffsetHours = json["gmt_offset"] | -5;               // -5 = Eastern Standard Time (DST will be handled via NTP server)
          dstOffsetHours = json["dst_offset"] | 1;                // 1 = 'spring forward'. Set to 0 if not observing Daylight Savings time.
          int sync = json["auto_sync"] | 1;                       // Auto-sync to NTP server (1=true 0=false)
          if (sync == 1) {
            autoSync = true;
          } else {
            autoSync = false;
          }
          ntpServer = json["ntp_server"] | "us.pool.ntp.org";     //Default ntp server to use for time sync
          autoSyncInterval =  json["sync_interval"] | 60;         //How often, in minutes, to sync to server: min 15 - 1,440 max (24 hr)
          scoreboardTeamLeft = json["score_team_left"] | "&V";    
          scoreboardTeamRight = json["score_team_right"] | "&H";  
          textEffect = json["text_effect"] | 0;
          textEffectSpeed = json["text_speed"] | 5;
          //textFull = json["default_text"] | "";
          textTop = json["text_top"] | "HELLO";
          textBottom = json["text_bottom"] | "WORLD";
          defaultCountdownMin = json["countdown_min"] | 0;
          defaultCountdownSec = json["countdown_sec"] | 0;
          int buzzer = json["use_buzzer"] | 1;
          if (buzzer == 1) {
            useBuzzer = true;
          } else {
            useBuzzer = false;
          }
          wledAddress = json["wled_address"] | "0.0.0.0";
          wledMaxPreset = json["wled_max_preset"] | 0;
          webColorClock = json["clock_color"] | 0;
          webColorTemperatureInt = json["temp_color_int"] | 3;
          webColorTemperatureExt = json["temp_color_ext"] | 0;
          webColorCountdown = json["count_color_active"] | 1;
          webColorCountdownPaused = json["count_color_paused"] | 2;
          webColorCountdownFinalMin = json["count_color_final_min"] | 3;
          webColorScoreboardLeft = json["score_color_left"] | 3;
          webColorScoreboardRight = json["score_color_right"] | 1;
          webColorTextTop = json["text_color_top"] | 3;
          webColorTextBottom = json["text_color_bottom"] | 1;

          clockColor = ColorCodes[webColorClock];
          temperatureColorInt = ColorCodes[webColorTemperatureInt];
          temperatureColorExt = ColorCodes[webColorTemperatureExt];
          countdownColor = ColorCodes[webColorCountdown];
          countdownColorPaused = ColorCodes[webColorCountdownPaused];
          countdownColorFinalMin = ColorCodes[webColorCountdownFinalMin];
          scoreboardColorLeft = ColorCodes[webColorScoreboardLeft];
          scoreboardColorRight = ColorCodes[webColorScoreboardRight];
          textColorTop = ColorCodes[webColorTextTop];
          textColorBottom = ColorCodes[webColorTextBottom];

          //=== Set or calculate other globals =====
          wifiHostName = deviceName;
          otaHostName = deviceName + "_OTA";
          clockMode = defaultClockMode;
          //Assure milliamps between 5000 - 25000
          if (milliamps > 25000) {
            milliamps = 25000;
          } else if (milliamps < 5000) {
            milliamps = 5000;
          }

          //Default countdown minutes
          if (defaultCountdownMin > 59) defaultCountdownMin = 59;
          if (defaultCountdownSec > 59) defaultCountdownSec = 59;

          //Convert refresh periods to seconds
          tempUpdatePeriod = (tempUpdatePeriod * 60);
          tempUpdatePeriodExt = (tempUpdatePeriodExt * 60); 
          //Do not allow external refresh more than once per 10 minutes
          if (tempUpdatePeriodExt < 600) tempUpdatePeriodExt = 600;

          initCountdownMillis = ((defaultCountdownMin * 60) + defaultCountdownSec) * 1000;
          countdownMilliSeconds = initCountdownMillis;
          remCountdownMillis = initCountdownMillis;

          //Default Team Names
          if (scoreboardTeamLeft.length() > 3) {
            scoreboardTeamLeft = scoreboardTeamLeft.substring(0, 4);
          } else if (scoreboardTeamLeft.length() < 1) {
            scoreboardTeamLeft = "&V";
          }
          if (scoreboardTeamRight.length() > 3) {
            scoreboardTeamRight = scoreboardTeamRight.substring(0, 4);
          } else if (scoreboardTeamRight.length() < 1) {
            scoreboardTeamRight = "&H";
          }

          //WLED Interface
          if (wledAddress == "0.0.0.0") {
            useWLED = false;
           } else {
            useWLED = true;
          }

          defaultClockMode = clockMode;
           
        } else {
         #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
            Serial.println("failed to load json config");
         #endif
          onboarding = true;
        }
        configFile.close();
      } else {
        onboarding = true;
      }
    } else {
      // No config file found - set to onboarding
      onboarding = true;
    }

    LittleFS.end();  //End - need to prevent issue with OTA updates
  } else {
    //could not mount filesystem
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("failed to mount FS");
        Serial.println("LittleFS Formatted. Restarting ESP.");
    #endif
    onboarding = true;
  }
}

//======================================
// Write config file to flash (LittleFS)
//======================================
void writeConfigFile(bool restart_ESP) {
  //Write settings to LittleFS (reboot to save)

  if (LittleFS.begin()) {
    DynamicJsonDocument doc(1024);
    doc.clear();
    //Add any values to save to JSON document
    doc["device_name"] = deviceName;
    doc["clock_mode"] = defaultClockMode;
    if (binaryClock) {
      doc["binary_clock"] = 1;
    } else {
      doc["binary_clock"] = 0;
    }
    doc["max_milliamps"] = milliamps;
    doc["led_brightness"] = brightness;
    doc["num_font"] = numFont;
    if (autoOnOff) {
      doc["auto_on_off"] = 1;
    } else {
      doc["auto_on_off"] = 0;
    }
    doc["auto_off_hr"] = autoOffHr;
    doc["auto_off_min"] = autoOffMin;
    doc["auto_off_brightness"] = autoOffBrightness;
    doc["auto_on_hr"] = autoOnHr;
    doc["auto_on_min"] = autoOnMin;
    doc["auto_on_brightness"] = autoOnBrightness;
    doc["owm_key"] = owmKey;
    doc["owm_lat"] = owmLat;
    doc["owm_long"] = owmLong;
    if (tempExtUseApi) {
      doc["temp_ext_api"]  = 1;
    } else {
      doc["temp_ext_api"]  = 0;
    }
    if (tempIntUseApi) {
      doc["temp_int_api"] = 1;
    } else {
      doc["temp_int_api"] = 0;
    }
    doc["temp_symbol"] = temperatureSymbol;
    doc["temp_source"] = temperatureSource;
    doc["temp_correction"] = temperatureCorrection;
    doc["temp_upd_int"] = (tempUpdatePeriod / 60);    //convert back to minutes
    doc["temp_upd_ext"] = (tempUpdatePeriodExt / 60); //convert back to minutes
    doc["hour_format"] = hourFormat;
    doc["time_zone"] = timeZone;
    doc["gmt_offset"] = gmtOffsetHours;
    doc["dst_offset"] = dstOffsetHours;
    if (useCustomOffsets) {
      doc["use_custom_tz"] = 1;
    } else {
      doc["use_custom_tz"] = 0;
    }
    if (autoSync) {
      doc["auto_sync"] = 1;
    } else {
      doc["auto_sync"] = 0;
    }
    doc["ntp_server"] = ntpServer;
    doc["sync_interval"] = autoSyncInterval;
    doc["score_team_left"] = scoreboardTeamLeft;
    doc["score_team_right"] = scoreboardTeamRight;
    doc["text_effect"] = textEffect;
    doc["text_speed"] = textEffectSpeed;
    doc["text_top"] = textTop;
    doc["text_bottom"] = textBottom;
    doc["countdown_min"] = defaultCountdownMin;
    doc["countdown_sec"] = defaultCountdownSec;
    if (useBuzzer) {
      doc["use_buzzer"] = 1;
    } else {
      doc["use_buzzer"] = 0;
    }
    doc["wled_address"] = wledAddress;
    doc["wled_max_preset"] = wledMaxPreset;
    doc["clock_color"] = webColorClock;
    doc["temp_color_int"] = webColorTemperatureInt;
    doc["temp_color_ext"] = webColorTemperatureExt;
    doc["count_color_active"] = webColorCountdown;
    doc["count_color_paused"] = webColorCountdownPaused;
    doc["count_color_final_min"] = webColorCountdownFinalMin;
    doc["score_color_left"] = webColorScoreboardLeft;
    doc["score_color_right"] = webColorScoreboardRight;
    doc["text_color_top"] = webColorTextTop;
    doc["text_color_bottom"] = webColorTextBottom;

    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("failed to open config file for writing");
      #endif
      configFile.close();
      return;
    } else {
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        serializeJson(doc, Serial);
      #endif
      serializeJson(doc, configFile);
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("Settings saved.");
      #endif
      configFile.close();
      LittleFS.end();
      if (restart_ESP) {
        ESP.restart();
      }
    }
  } else {
//could not mount filesystem
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("failed to mount FS");
    #endif
  }
}

/* =========================
    PRIMARY WEB PAGES
   =========================*/
void webMainPage() {
  String mainPage = "<html><head>";
  mainPage += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";  //make page responsive
  if (onboarding) {
    //Show portal/onboarding page
    mainPage += "<title>VAR_APP_NAME Onboarding</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\
    </style>\
    </head>\
    <body>";
    mainPage += "<h1>VAR_APP_NAME Onboarding</h1>";
    mainPage += "Please enter your WiFi information below. These are CASE-SENSITIVE and limited to 64 characters each.<br><br>";
    mainPage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/onboard\">\
      <table>\
      <tr>\
      <td><label for=\"ssid\">SSID:</label></td>\
      <td><input type=\"text\" name=\"ssid\" maxlength=\"64\" value=\"";
    mainPage += wifiSSID;
    mainPage += "\"></td></tr>\
        <tr>\
        <td><label for=\"wifipw\">Password:</label></td>\
        <td><input type=\"password\" name=\"wifipw\" maxlength=\"64\" value=\"";
    mainPage += wifiPW;
    mainPage += "\"></td></tr></table><br>";
    mainPage += "<b>Device Name: </b>Please give this device a unique name from all other devices on your network, including other installs of VAR_APP_NAME. ";
    mainPage += "This will be used to set the WiFi and OTA hostnames.<br><br>";
    mainPage += "16 alphanumeric (a-z, A-Z, 0-9) characters max, no spaces:";
    mainPage += "<table>\
        <tr>\
        <td><label for=\"devicename\">Device Name:</label></td>\
        <td><input type=\"text\" name=\"devicename\" maxlength=\"16\" value=\"";
    mainPage += deviceName;
    mainPage += "\"></td></tr>";
    mainPage += "</table><br><br>";
    mainPage += "<b>Max Milliamps: </b>Enter the max current the LEDs are allowed to draw.  This should be about 80% of the rated peak max of the power supply. ";
    mainPage += "Valid values are 5000 to 25000.  See documentation for more info.<br><br>";
    mainPage += "<table>\
        <tr>\
        <td><labelfor=\"maxmilliamps\">Max Milliamps:</label></td>\
        <td><input type=\"number\" name=\"maxmilliamps\" min=\"5000\" max=\"25000\" step=\"1\" value=\"";
    mainPage += String(milliamps);
    mainPage += "\"></td></tr>";
    mainPage += "</table><br><br>";
    mainPage += "<input type=\"submit\" value=\"Submit\">";
    mainPage += "</form>";

  } else {
    //Normal Settings Page
    mainPage += "<title>VAR_APP_NAME Main Page</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #0000ff; }\
    </style>\
    </head>\
    <body>";
    mainPage += "<H1>VAR_APP_NAME Settings and Options</H1>";
    mainPage += "Firmware Version: VAR_CURRENT_VER<br><br>";
    mainPage += "<table border=\"1\" >";
    mainPage += "<tr><td>Device Name:</td><td>" + deviceName + "</td</tr>";
    mainPage += "<tr><td>WiFi Network:</td><td>" + WiFi.SSID() + "</td</tr>";
    mainPage += "<tr><td>MAC Address:</td><td>" + strMacAddr + "</td</tr>";
    mainPage += "<tr><td>IP Address:</td><td>" + baseIPAddress + "</td</tr>";
    mainPage += "<tr><td>Max Milliamps:</td><td>" + String(milliamps) + "</td></tr>";
    mainPage += "</table><br>";

    //Standard mode button header
    mainPage += "<H2>Mode Display & Control</H2>";
    mainPage += "<table><tr><td>";

    mainPage += "<button type=\"button\" id=\"btnLEDToggle\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
    if (ledsOn) {
      mainPage += "background-color: #63de3e;\" \">Display On";
    } else {
      mainPage += "background-color: #c9535a;\" \">Display Off";
    }
    
    mainPage += "</button></td>";

    mainPage += "<td>&nbsp;</td><td>&nbsp;</td>";
    mainPage += "</tr><tr><td>&nbsp;</td></tr><tr>";

    //Shot Clock Only Mode
    mainPage += "<td><button id=\"btnclock\" style=\"font-size: 20px; ";
    if (clockMode == ShotClkMode) {
      mainPage += "background-color: #95f595; font-weight: bold; ";
    } else {
      mainPage += "background-color: #d6c9d6; ";
    }
    mainPage += "text-align: center; border-radius: 8px; width: 140px; height: 60px;\" onclick=\"location.href = './shot_clock';\">Shot Clock</button></td>";
  
    //Game Clock Only Mode
    mainPage += "<td><button id=\"btnclock\" style=\"font-size: 20px; ";
    if (clockMode == GameClkMode) {
      mainPage += "background-color: #95f595; font-weight: bold; ";
    } else {
      mainPage += "background-color: #d6c9d6; ";
    }
    mainPage += "text-align: center; border-radius: 8px; width: 140px; height: 60px;\" onclick=\"location.href = './game_clock';\">Game Clock</button></td>";
  
    //Game and Shot Clock Mode
    mainPage += "<td><button id=\"btnclock\" style=\"font-size: 20px; ";
    if (clockMode == GameAndShotClkMode) {
      mainPage += "background-color: #95f595; font-weight: bold; ";
    } else {
      mainPage += "background-color: #d6c9d6; ";
    }
    mainPage += "text-align: center; border-radius: 8px; width: 140px; height: 60px;\" onclick=\"location.href = './game_and_shot_clk';\">Game & Shot Clock</button></td>";
 
  }

  //JavaScript Section
  mainPage += "<script>";
  mainPage += homepage_js;
  mainPage += "</script>";

  mainPage += "</body></html>";
  mainPage.replace("VAR_APP_NAME", APPNAME);
  mainPage.replace("VAR_CURRENT_VER", VERSION);
  mainPage.replace("INITIAL_DISPLAY_ON_STATE", String(ledsOn));
  server.send(200, "text/html", mainPage);
}

void webShotClockPage() {
  clockMode = ShotClkMode;
  String message = "<html><head>";

  //Shot Clock Controls
  message += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";  //make page responsive
  message += "<title>Shot Clock Controls</title>\
    <style>\
    body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #0000ff; }\
    </style>\
    </head>\
    <body>";

  //Shot Clock Controls
  message += "<H1>Shot Clock Controls</H1>";
  message += "</td></tr>";

  //Reset Shot Clock Button
  message += "<button type=\"button\" id=\"btnRstShotClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Reset</button>";

  message += "<br><br>";    //Line Space

  //Shot Clock Run/Stop Button
  message += "<button type=\"button\" id=\"btnRunShotClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  if (syncGameAndShotClks ? (runGameClk|runShotClk) : runShotClk) {
    message += "background-color: #c9535a;\"\">Stop";
  } else {
    message += "background-color: #63de3e;\"\">Run";
  }
  message += "</button></td>";

  message += "<br><br>";    //Line Space

  //Shot Clock Display
  message += "<div id=\"shotClkTimer\" style=\"font-size: 48px; color: black; \"></div>";

  //Edit Shot Clock Button
  message += "<button type=\"button\" id=\"btnEditShotClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Edit</button>";

  message += "<br><br>";    //Line Space
  message += "<br><br>";    //Line Space

  //Home Button
  message += "<button type=\"button\" id=\"btnHome\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "background-color: #63de3e;\">";
  message += "Home";
  message += "</button></td>";

  message += "<br><br>";    //Line Space

  //JavaScript Section
  message += "<script>";
  message += utils_js;
  message += shot_clk_only_js;
  message += shot_clk_js;
  message += "</script>";

  message += "</body></html>";

  message.replace("INITIAL_SHOT_CLK_VAL", String(shotClkMilliSecCnt));
  message.replace("RESET_SHOT_CLK_VAL", String(shotClkMilliSecRstCnt));
  message.replace("SHOT_CLK_ENABLE", String(runShotClk));

  server.send(200, "text/html", message);
}

void webGameClockPage() {
  clockMode = GameClkMode;
  String message = "<html><head>";

  //Game Clock Controls
  message += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";  //make page responsive
  message += "<title>Game Clock Controls</title>\
    <style>\
    body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #0000ff; }\
    </style>\
    </head>\
    <body>";

  message += "<H1>Game Clock Controls</H1>";
  message += "</td></tr>";

  // Reset Game Clock Button
  message += "<button type=\"button\" id=\"btnRstGameClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Reset</button>";
  message += "</button>";

  message += "<br><br>";    //Line Space

  // Game Clock Run/Stop
  message += "<button type=\"button\" id=\"btnRunGameClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  if (syncGameAndShotClks ? (runGameClk|runShotClk) : runGameClk) {
    message += "background-color: #c9535a;\"\">Stop";
  } else {
    message += "background-color: #63de3e;\"\">Run";
  }
  message += "</button></td>";

  message += "<br><br>";    //Line Space

  //Game Clock Display
  message += "<div id=\"gameClkTimer\" style=\"font-size: 48px; color: black; \"></div>";

  //Edit Game Clock Button
  message += "<button type=\"button\" id=\"btnEditGameClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Edit</button>";
  
  message += "<br><br>";    //Line Space

  //Home Button
  message += "<button type=\"button\" id=\"btnHome\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "background-color: #63de3e;\">";
  message += "Home";
  message += "</button></td>";

  message += "<br><br>";    //Line Space

  //JavaScript Section
  message += "<script>";
  message += utils_js;
  message += game_clk_only_js;
  message += game_clk_js;
  message += "</script>";

  message += "</body></html>";

  message.replace("INITIAL_GAME_CLK_VAL", String(gameClkMilliSecCnt));
  message.replace("RESET_GAME_CLK_VAL", String(gameClkMilliSecRstCnt));
  message.replace("GAME_CLK_ENABLE", String(runGameClk));

  server.send(200, "text/html", message);
}

void webGameAndShotClockPage() {
  clockMode = GameAndShotClkMode;
  String message = "<html><head>";

  //Game & Shot Clock Controls
  message += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";  //make page responsive
  message += "<title>Game & Shot Clock Controls</title>\
    <style>\
    body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #0000ff; }\
    </style>\
    </head>\
    <body>";

  message += "<H1>Game Clock Controls</H1>";
  message += "</td></tr>";

  //Reset Game Clock Button
  message += "<button type=\"button\" id=\"btnRstGameClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Reset</button>";
  message += "</button>";

  message += "<br><br>";    //Line Space

  //Game Clock Run/Stop
  message += "<button type=\"button\" id=\"btnRunGameClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  if (syncGameAndShotClks ? (runGameClk|runShotClk) : runGameClk) {
    message += "background-color: #c9535a;\"\">Stop";
  } else {
    message += "background-color: #63de3e;\"\">Run";
  }
  message += "</button></td>";

  message += "<br><br>";    //Line Space

  //Game Clock Display
  message += "<div id=\"gameClkTimer\" style=\"font-size: 48px; color: black; \"></div>";

  //Edit Game Clock Button
  message += "<button type=\"button\" id=\"btnEditGameClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Edit</button>";
  
  message += "<br><br>";    //Line Space

  //Shot Clock Controls
  message += "<H1>Shot Clock Controls</H1>";
  message += "</td></tr>";

  //Reset Shot Clock Button
  message += "<button type=\"button\" id=\"btnRstShotClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Reset</button>";

  message += "<br><br>";    //Line Space

  //Shot Clock Run/Stop Button
  message += "<button type=\"button\" id=\"btnRunShotClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  if (syncGameAndShotClks ? (runGameClk|runShotClk) : runShotClk) {
    message += "background-color: #c9535a;\"\">Stop";
  } else {
    message += "background-color: #63de3e;\"\">Run";
  }
  message += "</button></td>";

  message += "<br><br>";    //Line Space

  //Shot Clock Display
  message += "<div id=\"shotClkTimer\" style=\"font-size: 48px; color: black; \"></div>";

  //Edit Shot Clock Button
  message += "<button type=\"button\" id=\"btnEditShotClk\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "\">Edit</button>";

  message += "<br><br>";    //Line Space
  message += "<br><br>";    //Line Space

  //Home Button
  message += "<button type=\"button\" id=\"btnHome\" style=\"font-size: 16px; border-radius: 8px; width: 140px; height: 40px; "; 
  message += "background-color: #63de3e;\">";
  message += "Home";
  message += "</button></td>";

  message += "<br><br>";    //Line Space

  //Sync Clocks Checkbox
  message += R"rawliteral(
            <label>
              <input type="checkbox" id="syncClksCheckbox" checked>
              Sync Shot Clock to Game Clock
            </label>)rawliteral";

  //JavaScript Section
  message += "<script>";
  message += utils_js;
  message += game_and_shot_clk_js;
  message += game_clk_js;
  message += shot_clk_js;
  message += "</script>";

  message += "</body></html>";

  message.replace("INITIAL_GAME_CLK_VAL", String(gameClkMilliSecCnt));
  message.replace("RESET_GAME_CLK_VAL", String(gameClkMilliSecRstCnt));
  message.replace("GAME_CLK_ENABLE", String(runGameClk));
  message.replace("INITIAL_SHOT_CLK_VAL", String(shotClkMilliSecCnt));
  message.replace("RESET_SHOT_CLK_VAL", String(shotClkMilliSecRstCnt));
  message.replace("SHOT_CLK_ENABLE", String(runShotClk));
  message.replace("INITIAL_CHECKBOX_STATE", String(syncGameAndShotClks));
  message.replace("SYNC_CLKS", String(syncGameAndShotClks)); // Set the initial sync state

  server.send(200, "text/html", message);
}

// ============================
//  Web Page Handlers
// ============================
void handleOnboard() {
  byte count = 0;
  bool wifiConnected = true;
  uint32_t currentMillis = millis();
  uint32_t pageDelay = currentMillis + 5000;
  String webPage = "";
  //Output web page to show while trying wifi join
  webPage = "<html><head>\
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\ 
    <meta http-equiv=\"refresh\" content=\"1\">";  //make page responsive and refresh once per second
  webPage += "<title>VAR_APP_NAME Onboarding</title>\
      <style>\
        body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\
      </style>\
      </head>\
      <body>";
  webPage += "<h3>Attempting to connect to Wifi</h3><br>";
  webPage += "Please wait...  If WiFi connection is successful, device will reboot and you will be disconnected from the VAR_APP_NAME AP.<br><br>";
  webPage += "Reconnect to normal WiFi, obtain the device's new IP address and go to that site in your browser.<br><br>";
  webPage += "If this page does remains after one minute, reset the controller and attempt the onboarding again.<br>";
  webPage += "</body></html>";
  webPage.replace("VAR_APP_NAME", APPNAME);
  server.send(200, "text/html", webPage);
  while (pageDelay > millis()) {
    yield();
  }

  //Handle initial onboarding - called from main page
  //Get vars from web page
  wifiSSID = server.arg("ssid");
  wifiPW = server.arg("wifipw");
  deviceName = server.arg("devicename");
  milliamps = server.arg("maxmilliamps").toInt();
  wifiHostName = deviceName;

  //Attempt wifi connection
#if defined(ESP8266)
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  //Disable WiFi Sleep
#elif defined(ESP32)
  WiFi.setSleep(false);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.hostname(wifiHostName);
  WiFi.begin(wifiSSID, wifiPW);
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
  Serial.print("SSID:");
  Serial.println(wifiSSID);
  Serial.print("password: ");
  Serial.println(wifiPW);
  Serial.print("Connecting to WiFi (onboarding)");
#endif
  while (WiFi.status() != WL_CONNECTED) {
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.print(".");
#endif
    // Stop if cannot connect
    if (count >= 60) {
// Could not connect to local WiFi
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println();
      Serial.println("Could not connect to WiFi during onboarding.");
#endif
      wifiConnected = false;
      break;
    }
    delay(500);
    yield();
    count++;
  }

  if (wifiConnected) {
    //Save settings to LittleFS and reboot
    writeConfigFile(true);
  }
}

void handleRestart() {
  String restartMsg = "<HTML>\
      <head>\
        <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
        <title>Controller Restart</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
      </head>\
      <body>\
      <H1>Controller restarting...</H1><br>\
      <H3>Please wait</H3><br>\
      After the controller completes the boot process, you may click the following link to return to the main page:<br><br>\
      <a href=\"http://";
  restartMsg += baseIPAddress;
  restartMsg += "\">Return to settings</a><br>";
  restartMsg += "</body></html>";
  server.send(200, "text/html", restartMsg);
  delay(1000);
  ESP.restart();
}

void handleDisplayOnToggle() {
  //Function for turning display "off", while allowing controller to run
  //Equivalent to setting brightness to 0 or non-zero 
  ledsOn = !ledsOn; //Toggle the LED State
  if (ledsOn) {
    if ((holdBrightness > 250) || (holdBrightness < 5)) {
      holdBrightness = 25;   //provide default value
    } 
    brightness = holdBrightness;
  } else {
    holdBrightness = brightness;
    brightness = 0;
  }
  FastLED.setBrightness(brightness);
  FastLED.show();

  server.send(200, "text/plain", "OK");
}

void handleRstShotClk() {
  unsigned long deltaMilliSecCnt;
  //Reset the LED Counter
  shotClkMilliSecCnt = shotClkMilliSecRstCnt;
  lastShotClkMilliSecCnt = shotClkMilliSecRstCnt;

  //If the shot clock is greater than the game clock then make them equal
  if(shotClkMilliSecCnt > gameClkMilliSecCnt) {
    displayShotClkValue(gameClkMilliSecCnt/1000, CRGB::Green);
  } else {
    displayShotClkValue(shotClkMilliSecCnt/1000, CRGB::Green);
  }
  
  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("shotClkMilliSecCnt " + String(shotClkMilliSecCnt/1000));
  #endif
  server.send(200, "text/plain", "OK");
}

void handleEditShotClk() {

  if (server.hasArg("time")) {
    // Get the value of the 'time' parameter as a string
    String timeString = server.arg("time");
    unsigned long newTimeValue = timeString.toInt();
    //Edit the Game Clock
    shotClkMilliSecCnt = newTimeValue;
  }

  //If the shot clock is greater than the game clock then make them equal
  if(gameClkMilliSecCnt < shotClkMilliSecCnt) {
    shotClkMilliSecCnt = gameClkMilliSecCnt;
    displayShotClkValue(round((float(shotClkMilliSecCnt))/1000), CRGB::Green);
  }
  displayShotClkValue(round((float(shotClkMilliSecCnt))/1000), CRGB::Green);
  lastShotClkMilliSecCnt = shotClkMilliSecCnt;

  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("shotClkMilliSecCnt " + String(shotClkMilliSecCnt/1000));
  #endif

  server.send(200, "text/plain", "OK");
}

void handleSyncClks() {
  syncGameAndShotClks = true;
  //If either clock is running, make them both run
  runGameClk = runShotClk | runGameClk;
  runShotClk = runShotClk | runGameClk;
  server.send(200, "text/plain", "OK");
}

void handleUnSyncClks() {
  syncGameAndShotClks = false;
  server.send(200, "text/plain", "OK");
}

void handleToggleRunShotClk() {
  runShotClk = !runShotClk;
  if(syncGameAndShotClks) {
    runGameClk = runShotClk;
  }
  server.send(200, "text/plain", "OK");
}

void handleRstGameClk() {
  //Reset the LED Counter
  gameClkMilliSecCnt = gameClkMilliSecRstCnt;
  lastGameClkMilliSecCnt = gameClkMilliSecRstCnt;
  displayGameClkValue(gameClkMilliSecCnt/1000, CRGB::Green);
  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("gameClkMilliSecCnt " + String(gameClkMilliSecCnt/1000));
  #endif
  server.send(200, "text/plain", "OK");
}

void handleEditGameClk() {

  if (server.hasArg("time")) {
    // Get the value of the 'time' parameter as a string
    String timeString = server.arg("time");
    unsigned long newTimeValue = timeString.toInt();
    //Edit the Game Clock
    gameClkMilliSecCnt = newTimeValue;
  }

  displayGameClkValue(round((float(gameClkMilliSecCnt))/1000), CRGB::Green);
  lastGameClkMilliSecCnt = gameClkMilliSecCnt;

  //If the shot clock is greater than the game clock then make them equal
  if(gameClkMilliSecCnt < shotClkMilliSecCnt) {
    shotClkMilliSecCnt = gameClkMilliSecCnt;
    lastShotClkMilliSecCnt = gameClkMilliSecCnt;
    displayShotClkValue(round((float(shotClkMilliSecCnt))/1000), CRGB::Green);
  }

  FastLED.show();
  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.println("gameClkMilliSecCnt " + String(gameClkMilliSecCnt/1000));
  #endif

  server.send(200, "text/plain", "OK");
}

void handleToggleRunGameClk() {
  runGameClk = !runGameClk;
  if(syncGameAndShotClks) {
    runShotClk = runGameClk;
  }
  // #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
  //   Serial.println("test = " + String(test));
  // #endif
  // test++;

  server.send(200, "text/plain", "OK");
}

// void handleReset() {
//   String resetMsg = "<HTML>\
//       </head>\
//         <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\
//         <title>Controller Reset</title>\
//         <style>\
//           body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
//         </style>\
//       </head>\
//       <body>\
//       <H1>Controller Resetting...</H1><br>\
//       <H3>After this process is complete, you must setup your controller again:</H3>\
//       <ul>\
//       <li>Connect a device to the controller's local access point: VAR_APP_NAME_AP</li>\
//       <li>Open a browser and go to: 192.168.4.1</li>\
//       <li>Enter your WiFi information and set other default settings values</li>\
//       <li>Click Save. The controller will reboot and join your WiFi</li>\
//       </ul><br>\
//       Once the above process is complete, you can return to the main settings page by rejoining your WiFi and entering the IP address assigned by your router in a browser.<br>\
//       You will need to reenter all of your settings for the system as all values will be reset to original defaults<br><br>\
//       <b>This page will NOT automatically reload or refresh</b>\
//       </body></html>";
//   resetMsg.replace("VAR_APP_NAME", APPNAME);
//   server.send(200, "text/html", resetMsg);
//   delay(1000);
//   digitalWrite(2, LOW);
//   LittleFS.begin();
//   LittleFS.format();
//   LittleFS.end();
//   WiFi.disconnect(false, true);
//   delay(1000);
//   ESP.restart();
// }

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// ----------------------------
//  Setup Web Handlers
// -----------------------------
void setupWebHandlers() {
  //Main pages
  server.on("/", webMainPage);
  server.on("/shot_clock", webShotClockPage);
  server.on("/game_clock", webGameClockPage);
  server.on("/game_and_shot_clk", webGameAndShotClockPage);

  //Handlers/process actions
  server.on("/toggleDisplayOn", handleDisplayOnToggle);
  server.on("/rstShotClk", handleRstShotClk);
  server.on("/editShotClk", handleEditShotClk);
  server.on("/toggleRunShotClk", handleToggleRunShotClk);
  server.on("/rstGameClk", handleRstGameClk);
  server.on("/editGameClk", handleEditGameClk);
  server.on("/toggleRunGameClk", handleToggleRunGameClk);
  server.on("/onboard", handleOnboard);
  server.on("/restart", handleRestart);
  // server.on("/reset", handleReset);
  server.on("/syncClks", handleSyncClks);
  server.on("/unsyncClks", handleUnSyncClks);

  server.onNotFound(handleNotFound);
}

/* =====================================
    WIFI SETUP 
   =====================================
*/
void setupSoftAP() {
  //for onboarding
  WiFi.mode(WIFI_AP);
  WiFi.softAP(deviceName + "_AP");
  IPAddress Ip(192, 168, 4, 1);
  IPAddress NMask(255, 255, 255, 0);
  WiFi.softAPConfig(Ip, Ip, NMask);
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
  Serial.println("SoftAP Created");
  Serial.println("Web server starting...");
#endif
  server.begin();
}

bool setupWifi() {
  byte count = 0;
  //attempt connection
  //if successful, return true else false
  delay(200);
  WiFi.hostname(wifiHostName);
  WiFi.begin();
  while (WiFi.status() != WL_CONNECTED) {
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    Serial.print(".");
#endif
    // Stop if cannot connect
    if (count >= 60) {
// Could not connect to local WiFi
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println();
      Serial.println("Could not connect to WiFi.");
#endif
      return false;
    }
    delay(500);
    yield();
    count++;
  }
  //Successfully connected
  baseIPAddress = WiFi.localIP().toString();
  WiFi.macAddress(macAddr);
  strMacAddr = WiFi.macAddress();
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
  Serial.println("Connected to wifi... yay!");
  Serial.print("MAC Address: ");
  Serial.println(strMacAddr);
  Serial.print("IP Address: ");
  Serial.println(baseIPAddress);
  Serial.println("Starting web server...");
#endif
  server.begin();
  return true;
}

// ============================================
//   MAIN SETUP
// ============================================
void setup() {
#if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
  Serial.begin(115200);
  Serial.println("Starting setup...");
#endif
  esp_netif_init();
  setupWebHandlers();
  delay(500);

  defineColors();  //This must be done before reading config file
  readConfigFile();

  //Setup LEDs
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(LEDs, NUM_LEDS);
  FastLED.setDither(false);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, milliamps);
  FastLED.setBrightness(brightness);
  fill_solid(LEDs, NUM_LEDS, CRGB::Black);
  FastLED.show();

  if (onboarding) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("Entering Onboarding setup...");
    #endif
      LEDs[0] = CRGB::Blue; FastLED.show(); //TODO: In the future I should display an onboarding message on the LED Panel
      setupSoftAP();
  } else if (!setupWifi()) {
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        Serial.println("Wifi connect failed. Reentering onboarding...");
    #endif
    LEDs[0] = CRGB::Red; FastLED.show(); //TODO: In the future I should display an onboarding message on the LED Panel
    setupSoftAP();
    onboarding = true;
  }
      
  runShotClk = 0;
  enShotClk = 1;
  shotClkMilliSecCnt = shotClkMilliSecRstCnt;

  runGameClk = 0;
  enGameClk = 1;
  gameClkMilliSecCnt = gameClkMilliSecRstCnt;

  fill_solid(LEDs, NUM_LEDS, CRGB::Black);
  displayShotClkValue(round((float(shotClkMilliSecCnt))/1000), CRGB::Green);
  displayGameClkValue(round((float(gameClkMilliSecCnt))/1000), 0, CRGB::Green);
  FastLED.show();
};

// ===============================================================
//   MAIN LOOP
// ===============================================================
void loop() {
  int pixelPos;
  unsigned long milliSecCntr;
  unsigned long deltaMilliSecCnt;
  static unsigned long lastMilliSecCnt = 0;

  //Check if one sec has elasped
  milliSecCntr = millis();
  deltaMilliSecCnt = milliSecCntr - lastMilliSecCnt;
  if(runShotClk) shotClkMilliSecCnt = shotClkMilliSecCnt - deltaMilliSecCnt;
  if(runGameClk) gameClkMilliSecCnt = gameClkMilliSecCnt - deltaMilliSecCnt;
  lastMilliSecCnt = milliSecCntr;

  //Shot Clock Update
  if((lastShotClkMilliSecCnt - shotClkMilliSecCnt) > 1000) {
    if(shotClkMilliSecCnt < 0) shotClkMilliSecCnt = 0; //Prevent negative count)
    lastShotClkMilliSecCnt = shotClkMilliSecCnt;

    //If the shot clock is greater than the game clock the shot clk will get 
    // update with the game clk below to keep them in sync.
    if(shotClkMilliSecCnt < gameClkMilliSecCnt) {
      displayShotClkValue(round((float(shotClkMilliSecCnt))/1000), CRGB::Green);
    }
    
    FastLED.show();
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("shotClkMilliSecCnt " + String(shotClkMilliSecCnt));
    #endif
  }
  
  //Game Clock Update
  if((lastGameClkMilliSecCnt - gameClkMilliSecCnt) > 1000) {
    if(gameClkMilliSecCnt < 0) gameClkMilliSecCnt = 0; //Prevent negative count)
    lastGameClkMilliSecCnt = gameClkMilliSecCnt;
    displayGameClkValue(round((float(gameClkMilliSecCnt))/1000), CRGB::Green);

    //If the shot clock is greater than the game clock then make them equal
    if(shotClkMilliSecCnt > gameClkMilliSecCnt) {
      displayShotClkValue(round((float(gameClkMilliSecCnt))/1000), CRGB::Green);
    }

    FastLED.show();
    #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
      Serial.println("gameClkMilliSecCnt " + String(gameClkMilliSecCnt));
    #endif
  }

  server.handleClient();

}

void allBlank() {
  for (int i = 0; i < NUM_LEDS; i++) {
    yield();
    LEDs[i] = CRGB::Black;
  }
  FastLED.show();
}

void disableShotClkValue() {
  //Black out all of the digit segment pixels
  for (byte digitPlace = 0; digitPlace < 2; digitPlace++) {
    disableShotClkDigit(digitPlace);
  }
}

void displayShotClkValue(int value, CRGB color) {
    displayShotClkDigit(value/10,1,color); //Tens Place
    displayShotClkDigit(value%10,0,color); //Ones Place
}

void disableShotClkDigit(byte digitPlace){
  //digitPlace: 0 = Units, 1 = Tens
  int pixelPos;
  
  //Black out all of the digit segment pixels
  for (byte seg = 0; seg < 7; seg++) {
    for(byte j = 0; j < MAX_NUM_LEDS_PER_SEGMENT; j++) {
      pixelPos = (shotClkPixelPos[digitPlace][seg][j]);
      if(pixelPos >= 0) LEDs[pixelPos] = CRGB::Black;
    }
  }
}

void displayShotClkDigit(byte digit, byte digitPlace, CRGB color) {
  //digitPlace: 0 = Units, 1 = Tens
  int seg_active;
  int pixelPos;

  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    // Serial.println("Shot Clk digitPlace " + String(digitPlace));
    // Serial.println("Shot Clk digit " + String(digit));
  #endif

  //Black out all of the digit segment pixels
  disableShotClkDigit(digitPlace);

  //Turn on the segments for the digit
  for (byte seg = 0; seg < 7; seg++) {
    int seg_active = (SevenSegDigits[digit] >> seg) & 1;  //Determine if segment is active
    for(byte j = 0; j < MAX_NUM_LEDS_PER_SEGMENT; j++) {
      pixelPos = (shotClkPixelPos[digitPlace][seg][j]);
      if(pixelPos >= 0) if(seg_active) LEDs[pixelPos] = color;
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        // Serial.println("pixelPos " + String(pixelPos));
      #endif
    }
  }
}

void disableGameClkValue() {
  //Black out all of the digit segment pixels
  for (byte digitPlace = 0; digitPlace < 3; digitPlace++) {
    disableGameClkDigit(digitPlace);
  }
  disableGameClkSeparator();
}

void displayGameClkValue(int min, int sec, CRGB color) {
  displayGameClkDigit(min%10,2,color); //Ones Minutes
  displayGameClkSeparator(CRGB::Green); //":"  TODO this only need to be called when the game clk is first enabled
  displayGameClkDigit(sec/10,1,color); //Tens Seconds
  displayGameClkDigit(sec%10,0,color); //Ones Seconds
}

void displayGameClkValue(int sec, CRGB color) {
  int min = sec / 60; //Calculate minutes from seconds
  sec = sec - min * 60; //Calculate remaining seconds
  displayGameClkValue(min, sec, color);
}

void disableGameClkDigit(byte digitPlace){
  //digitPlace: 0 = Units, 1 = Tens, 2 = Minutes
  int pixelPos;
  
  //Black out all of the digit segment pixels
  for (byte seg = 0; seg < 7; seg++) {
    for(byte j = 0; j < MAX_NUM_LEDS_PER_SEGMENT; j++) {
      pixelPos = (gameClkPixelPos[digitPlace][seg][j]);
      if(pixelPos >= 0) LEDs[pixelPos] = CRGB::Black;
    }
  }
}

void displayGameClkDigit(byte digit, byte digitPlace, CRGB color) {
  //digitPlace: 0 = Units, 1 = Tens
  int seg_active;
  int pixelPos;

  #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
    // Serial.println("Game Clk digitPlace " + String(digitPlace));
    // Serial.println("Game Clk digit " + String(digit));
  #endif

  //Black out all of the digit segment pixels
  disableGameClkDigit(digitPlace);

  //Turn on the segments for the digit
  for (byte seg = 0; seg < 7; seg++) {
    int seg_active = (SevenSegDigits[digit] >> seg) & 1;  //Determine if segment is active
    for(byte j = 0; j < MAX_NUM_LEDS_PER_SEGMENT; j++) {
      pixelPos = (gameClkPixelPos[digitPlace][seg][j]);
      if(pixelPos >= 0) if(seg_active) LEDs[pixelPos] = color;
      #if defined(SERIAL_DEBUG) && (SERIAL_DEBUG == 1)
        // Serial.println("pixelPos " + String(pixelPos));
      #endif
    }
  }
}

void disableGameClkSeparator() {
  int pixelPos;
  for (byte seg = 0; seg < NUM_SEPARATOR_SEGS; seg++) {
    for(byte j = 0; j < PIXELS_PER_SEPARATOR_SEG; j++) {
      pixelPos = (gameClkSeparatorPixelPos[seg][j]);
      LEDs[pixelPos] = CRGB::Black;
    }
  }
}

void displayGameClkSeparator(CRGB color) {
  int pixelPos;
  for (byte seg = 0; seg < NUM_SEPARATOR_SEGS; seg++) {
    for(byte j = 0; j < PIXELS_PER_SEPARATOR_SEG; j++) {
      pixelPos = (gameClkSeparatorPixelPos[seg][j]);
      LEDs[pixelPos] = color;
    }
  }
}
