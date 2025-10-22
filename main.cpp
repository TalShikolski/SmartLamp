#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>
#include <LiquidCrystal_I2C.h>

const char* WIFI_SSID = "SHIKOLSKI";
const char* WIFI_PASSWORD = "a1a2a3a4";
const char* MDNS_NAME = "SmartLamp";
 
#define PIN_NEOPIX 5
#define LED_COUNT 16
#define PIN_LDR_ADC 32
#define LCD_ADDR 0x27


Adafruit_NeoPixel led(LED_COUNT, PIN_NEOPIX, NEO_GRB + NEO_KHZ800);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
WebServer server(80);


//HTML Page
String htmlPage() {
  String s =
"<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'/>"
"<style>"
"body{font-family:sans-serif;max-width:560px;margin:24px auto;padding:0 12px}"
".card{border:1px solid #ddd;border-radius:12px;padding:16px;box-shadow:0 1px 4px rgba(0,0,0,.06)}"
".row{display:flex;align-items:center;justify-content:space-between;margin:10px 0}"
"label{display:block;margin:8px 0 6px}"
"input[type=range]{width:100%}"
"button,select{padding:10px 14px;border:0;border-radius:8px;cursor:pointer}"
".on{background:#4caf50;color:#fff}.off{background:#f44336;color:#fff}"
".autoOn{background:#3f51b5;color:#fff}.autoOff{background:#9e9e9e;color:#fff}"
".stat{font-size:14px;color:#444;margin-top:10px}"
"</style></head><body>"
"<h2>Smart Lamp</h2>"
"<div class='card'>"

"<div class='row'>"
"  <div><b>Power</b></div>"
"  <button id='pwr' class='on' onclick='togglePower()'>ON</button>"
"</div>"

"<div class='row'>"
"  <div><b>Auto Mode</b></div>"
"  <button id='auto' class='autoOff' onclick='toggleAuto()'>AUTO: OFF</button>"
"</div>"

"<label for='br'>Brightness: <span id='bval'>--</span>%</label>"
"<input id='br' type='range' min='0' max='100' value='0' "
"       oninput='updateLabel(this.value)' "
"       onmouseup='sendBrightness(this.value)' "
"       ontouchend='sendBrightness(this.value)'>"

"<label for='color'>Color</label>"
"<select id='color' onchange='sendColor(this.value)'>"
"  <option value='warmwhite'>WarmWhite</option>"
"  <option value='white'>White</option>"
"  <option value='red'>Red</option>"
"  <option value='green'>Green</option>"
"  <option value='blue'>Blue</option>"
"  <option value='amber'>Amber</option>"
"  <option value='cyan'>Cyan</option>"
"  <option value='magenta'>Magenta</option>"
"  <option value='purple'>Purple</option>"
"  <option value='yellow'>Yellow</option>"
"</select>"

"<div class='stat' id='ldr'>LDR: --%</div>"
"</div>"

"<script>"
"function updateUI(s){"
"  document.getElementById('bval').textContent = s.brightness;"
"  document.getElementById('br').value = s.brightness;"
"  document.getElementById('ldr').textContent = 'LDR: ' + s.ldr + '%';"
"  var p = document.getElementById('pwr');"
"  if (s.on){ p.textContent='ON'; p.className='on'; } else { p.textContent='OFF'; p.className='off'; }"
"  var a = document.getElementById('auto');"
"  if (s.auto){ a.textContent='AUTO: ON'; a.className='autoOn'; } else { a.textContent='AUTO: OFF'; a.className='autoOff'; }"
"  var sel = document.getElementById('color');"
"  if (sel && s.color){ sel.value = s.color; }"
"}"
"function updateLabel(v){ document.getElementById('bval').textContent = v; }"
"function sendBrightness(v){ fetch('/set?b='+v); }"
"function togglePower(){"
"  var nowOn = document.getElementById('pwr').textContent === 'ON';"
"  fetch('/power?on=' + (nowOn ? '0' : '1'));"
"}"
"function toggleAuto(){"
"  var isOn = document.getElementById('auto').textContent.includes('ON');"
"  fetch('/auto?on=' + (isOn ? '0' : '1'));"
"}"
"function sendColor(name){ fetch('/color?name=' + encodeURIComponent(name)); }"
"async function poll(){"
"  try{ let r=await fetch('/state'); let s=await r.json(); updateUI(s); }catch(e){}"
"  setTimeout(poll,800);"
"}"
"poll();"
"</script>"

"</body></html>";
  return s;
}



bool lamp_on = true;
uint8_t brightness = 60;

static inline uint8_t clamp01(int v)
{
  if(v < 0) return 0;
  if(v > 100) return 100;
  return v;
}

uint8_t readLdrPct(){

  int raw = analogRead(PIN_LDR_ADC);
  int inv = 4095 - raw;
  int pct = map(inv , 0 , 4095 , 0 , 100);
  if(pct < 0) pct = 0;
  if(pct > 100) pct = 100;
  return (uint8_t)pct;
}



bool auto_mode = false;
String color_name = "warmwhite";

const uint8_t LDR_ON_THRESHOLD = 70;
const uint8_t LDR_OFF_THRESHOLD = 60;

uint32_t colorFromName(const String& name) {
  String n = name; n.toLowerCase();
  if (n == "warmwhite") return led.Color(255, 204, 153);
  if (n == "white")     return led.Color(255, 255, 255);
  if (n == "red")       return led.Color(255,   0,   0);
  if (n == "green")     return led.Color(  0, 255,   0);
  if (n == "blue")      return led.Color(  0,   0, 255);
  if (n == "amber")     return led.Color(255, 191,   0);
  if (n == "cyan")      return led.Color(  0, 255, 255);
  if (n == "magenta")   return led.Color(255,   0, 255);
  if (n == "purple")    return led.Color(180,   0, 255);
  if (n == "yellow")    return led.Color(255, 255,   0);
  return led.Color(255, 204, 153);
}



void applyLeds() {
  uint8_t level;
  if(lamp_on == true)
  {
    level = map(brightness , 0 , 100 , 0 , 255);
  }
  else{
    level = 0;
  }

  uint32_t base = colorFromName(color_name);
  uint8_t r = (uint8_t)((base >> 16) & 0xFF);
  uint8_t g = (uint8_t)((base >> 8) & 0xFF);
  uint8_t b = (uint8_t)((base) & 0xFF);
  r = (uint8_t)((uint16_t)r * level /255);
  g = (uint8_t)((uint16_t)g * level /255);
  b = (uint8_t)((uint16_t)b * level /255);
  for(uint16_t i = 0 ; i < LED_COUNT ; i++)
  {
    led.setPixelColor(i , led.Color(r , g, b));
  }
  led.show();
}


// ===== HTTP Handlers =====

// דף הבית – מחזיר את ה-HTML
void handleRoot() {
  server.send(200, "text/html", htmlPage());
}

// שינוי בהירות ידני: /set?b=NN  (0..100)
void handleSet() {
  if (server.hasArg("b")) {
    brightness = clamp01(server.arg("b").toInt());
    applyLeds();
  }
  server.send(204); // No Content
}

// הדלקה/כיבוי: /power?on=1 או /power?on=0  (אם אין פרמטר – יעשה toggle)
void handlePower() {
  if (server.hasArg("on")) {
    lamp_on = (server.arg("on") == "1");
  } else {
    lamp_on = !lamp_on;
  }
  applyLeds();
  server.send(204);
}

// מצב אוטומטי: /auto?on=1 או /auto?on=0  (אם אין פרמטר – toggle)
void handleAuto() {
  if (server.hasArg("on")) {
    auto_mode = (server.arg("on") == "1");
  } else {
    auto_mode = !auto_mode;
  }
  server.send(204);
}

// בחירת צבע: /color?name=red  (או כל שם שקיים ב-colorFromName)
void handleColor() {
  if (server.hasArg("name")) {
    color_name = server.arg("name");
    applyLeds();
  }
  server.send(204);
}

// מצב לממשק (JSON): /state  – עבור ה-polling של הדף
void handleState() {
  char buf[160];
  snprintf(buf, sizeof(buf),
           "{\"on\":%s,\"auto\":%s,\"brightness\":%u,\"ldr\":%u,\"color\":\"%s\"}",
           lamp_on == true ? "true" : "false",
           auto_mode == true ? "true" : "false",
           brightness, readLdrPct(), color_name.c_str());
  server.send(200, "application/json", buf);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());

  // mDNS – עדיף שם באותיות קטנות
  if (MDNS.begin("smartlamp")) {
    Serial.println("mDNS: http://smartlamp.local");
  } else {
    Serial.println("mDNS start failed");
  }
}

void setup() {
  Serial.begin(115200);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Smart Lamp");
  lcd.setCursor(0,1); lcd.print("Booting...");

  // NeoPixel
  led.begin();
  led.clear();
  led.show();

  // ADC (LDR)
  analogReadResolution(12);        // 0..4095
  analogSetAttenuation(ADC_11db);  // טווח קרוב ל-0..3.3V

  // Wi-Fi + mDNS
  connectWiFi();

  // רישום נתיבי ה-HTTP
  server.on("/",      handleRoot);
  server.on("/set",   handleSet);
  server.on("/power", handlePower);
  server.on("/auto",  handleAuto);
  server.on("/color", handleColor);
  server.on("/state", handleState);

  // הפעלת השרת
  server.begin();

  // הצגת ה-IP על ה-LCD
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("IP:");
  lcd.setCursor(3,0); lcd.print(WiFi.localIP().toString());

  // הדלקת לדים לפי מצב התחלתי
  applyLeds();
}


void loop()
{
  server.handleClient();

  static unsigned long tAuto = 0;
  if(auto_mode == true && (millis() - tAuto) > 500)
  {
    tAuto = millis();
    unsigned ldr = readLdrPct();
    if(lamp_on == false && ldr > LDR_ON_THRESHOLD)
    {
      lamp_on = true;
      applyLeds();
    }
    else if(lamp_on == true && ldr < LDR_OFF_THRESHOLD)
    {
      lamp_on = false;
      applyLeds();
    }
  }


  static unsigned long tLcd = 0;
if (millis() - tLcd > 1000) {
  tLcd = millis();

  
 
  lcd.setCursor(0,1);
  if (lamp_on) {
    lcd.print("Brightness:");
    lcd.setCursor(12,1);
    lcd.print(brightness);
    lcd.print("%   ");  
  } else {
    lcd.print("               "); 
  }
}
 

}
