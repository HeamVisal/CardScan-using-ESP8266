#include <ESP8266WiFi.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define MAX_DEVICES 4
#define CS_PIN D8
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

MD_MAX72XX mx(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
WiFiServer server(80);

const char* ssid = "HakSeng";
const char* pass = "0969856032";

constexpr uint16_t MESG_SIZE = 255;
constexpr uint8_t  CHAR_SPACING = 1;
constexpr uint16_t SCROLL_DELAY = 75;

char curMessage[MESG_SIZE] = "";
char newMessage[MESG_SIZE] = "";
volatile bool newMessageAvailable = false;

static const char WEB_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
<head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>LED Matrix</title>
<style>
body{font-family:Helvetica;text-align:center;background:#f5f5f5;margin:0;padding:20px}
#box{display:inline-block;border:2px solid #000;padding:12px;background:#00FFFF}
</style>
<script>
function SendText(){
  const v=document.getElementById("msg").value;
  fetch("/?MSG="+encodeURIComponent(v));
}
</script></head>
<body>
<h2><b>LED Matrix WiFi Control</b></h2>
<div id="box">
  <b>MESSAGE:</b> <input id="msg" maxlength="255">
  <button onclick="SendText()">SEND</button>
</div>
</body></html>
)rawliteral";

// --- URL decode (minimal) ---
static inline uint8_t hexVal(char c){
  if (c>='0' && c<='9') return c-'0';
  c = toupper(c);
  if (c>='A' && c<='F') return c-'A'+10;
  return 0;
}
static void urlDecode(char* s){
  char* o=s;
  while(*s){
    if(*s=='%'){
      s++;
      char c = (hexVal(*s++)<<4);
      c |= hexVal(*s++);
      *o++ = c;
    } else if(*s=='+'){
      *o++=' '; s++;
    } else {
      *o++=*s++;
    }
  }
  *o=0;
}

// Extract MSG from "GET /?MSG=.... HTTP/1.1"
static bool extractMSG(const char* line, char* out, size_t outLen){
  const char* p = strstr(line, "MSG=");
  if(!p) return false;
  p += 4;
  size_t i=0;
  while(*p && *p!=' ' && *p!='&' && i<outLen-1) out[i++] = *p++;
  out[i]=0;
  urlDecode(out);
  return true;
}

// --- Scrolling callbacks ---
void scrollDataSink(uint8_t, MD_MAX72XX::transformType_t, uint8_t) {}

uint8_t scrollDataSource(uint8_t, MD_MAX72XX::transformType_t){
  static enum { IDLE, NEXT_CHAR, SHOW_CHAR, SHOW_SPACE } st = IDLE;
  static char* p;
  static uint16_t curLen, showLen;
  static uint8_t cBuf[8];
  uint8_t colData = 0;

  switch(st){
    case IDLE:
      p = curMessage;
      if(newMessageAvailable){
        strncpy(curMessage, newMessage, MESG_SIZE);
        curMessage[MESG_SIZE-1] = 0;
        newMessageAvailable = false;
      }
      st = NEXT_CHAR;
      break;

    case NEXT_CHAR:
      if(*p == 0) st = IDLE;
      else{
        showLen = mx.getChar(*p++, sizeof(cBuf), cBuf);
        curLen = 0;
        st = SHOW_CHAR;
      }
      break;

    case SHOW_CHAR:
      colData = cBuf[curLen++];
      if(curLen >= showLen){
        showLen = (*p ? CHAR_SPACING : (MAX_DEVICES * COL_SIZE)/2);
        curLen = 0;
        st = SHOW_SPACE;
      }
      break;

    case SHOW_SPACE:
      if(++curLen >= showLen) st = NEXT_CHAR;
      break;
  }
  return colData;
}

void handleWiFi(){
  WiFiClient client = server.available();
  if(!client) return;

  client.setTimeout(200);
  String line = client.readStringUntil('\n');   // first request line only
  line.trim();

  if(line.startsWith("GET ")){
    if(extractMSG(line.c_str(), newMessage, MESG_SIZE)){
      newMessageAvailable = true;
      client.print(F("HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"));
    } else {
      client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"));
      client.print(reinterpret_cast<const __FlashStringHelper*>(WEB_PAGE));
    }
  }
  client.stop();
}

void scrollText(){
  static uint32_t t=0;
  if(millis()-t >= SCROLL_DELAY){
    mx.transform(MD_MAX72XX::TSL);
    t = millis();
  }
}

void setup(){
  mx.begin();
  mx.setShiftDataInCallback(scrollDataSource);
  mx.setShiftDataOutCallback(scrollDataSink);

  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED) delay(300);

  server.begin();

  IPAddress ip = WiFi.localIP();
  snprintf(curMessage, MESG_SIZE, "%u:%u:%u:%u", ip[0], ip[1], ip[2], ip[3]);
}

void loop(){
  handleWiFi();
  scrollText();
}
