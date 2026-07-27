#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "img_converters.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "LittleFS.h"
#include <WiFi.h>
#include <WebServer.h>

#define TFT_CS   13
#define TFT_RST  -1
#define TFT_DC   12
#define TFT_SCLK 14
#define TFT_MOSI 15

SPIClass tftSPI(HSPI);
Adafruit_ST7735 tft = Adafruit_ST7735(&tftSPI, TFT_CS, TFT_DC, TFT_RST);

#define SCREEN_ROTATION 3
#define SCREEN_W 160
#define SCREEN_H 128
#define FRAME_W  320
#define FRAME_H  240

#define C_BG     0x0000
#define C_WHITE  0xFFFF
#define C_ACCENT 0x4EF0
#define C_DIM    0x6B6F
#define C_RED    0xFB90
#define C_PANEL  0x18E3

#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

#define BUTTON_PIN     3
#define FLASH_LED_PIN  4
#define FLASH_DUTY     15
#define DEBOUNCE_MS    50

#define PHOTO_DIR    "/photos"
#define JPEG_QUALITY 85

#define AP_SSID     "CAMX-Photos"
#define AP_PASSWORD "camera123"

WebServer server(80);

bool debouncedState = HIGH;
bool lastRawState = HIGH;
unsigned long lastDebounceTime = 0;

int nextPhotoIndex = 1;
bool fsReady = false;

void startCamera();
void handleButton();
void capturePhoto();
void findNextPhotoIndex();
int  countPhotos();
String photoPath(int index);
void drawFrameToScreen(camera_fb_t *fb);
void splashScreen();
void bootLine(const char *label, const char *value, uint16_t color, int y);
void toast(const char *text, uint16_t accent);
void fullStatus(const char *title, const char *sub, uint16_t accent);
void startWebServer();
void handleRoot();
void handlePhotoFile();
void handleDeletePhoto();

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(300);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  ledcAttach(FLASH_LED_PIN, 5000, 8);
  ledcWrite(FLASH_LED_PIN, 0);

  tftSPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(SCREEN_ROTATION);
  tft.setSPISpeed(27000000);
  tft.fillScreen(C_BG);

  splashScreen();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    bootLine("STORAGE", "FAIL", C_RED, 78);
  } else {
    fsReady = true;
    if (!LittleFS.exists(PHOTO_DIR)) {
      LittleFS.mkdir(PHOTO_DIR);
    }
    findNextPhotoIndex();
    char buf[16];
    snprintf(buf, sizeof(buf), "%d SHOTS", nextPhotoIndex - 1);
    bootLine("STORAGE", buf, C_ACCENT, 78);
  }
  delay(350);

  startWebServer();
  bootLine("NETWORK", AP_SSID, C_ACCENT, 92);
  delay(350);

  startCamera();
  bootLine("SENSOR", "OV2640", C_ACCENT, 106);
  delay(600);

  tft.fillScreen(C_BG);
  Serial.println("CAMX ready");
}

void splashScreen() {
  tft.fillScreen(C_BG);

  tft.drawRoundRect(6, 6, SCREEN_W - 12, SCREEN_H - 12, 8, C_PANEL);

  tft.setTextSize(3);
  tft.setTextColor(C_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds("CAMX", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_W - w) / 2, 26);
  tft.print("CAMX");

  tft.setTextSize(1);
  tft.setTextColor(C_DIM);
  tft.getTextBounds("HANDHELD CAMERA", 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_W - w) / 2, 54);
  tft.print("HANDHELD CAMERA");

  int barX = 30, barY = 66, barW = SCREEN_W - 60;
  tft.drawFastHLine(barX, barY, barW, C_PANEL);
  for (int i = 0; i <= barW; i += 4) {
    tft.drawFastHLine(barX, barY, i, C_ACCENT);
    delay(6);
  }
}

void bootLine(const char *label, const char *value, uint16_t color, int y) {
  tft.fillRect(12, y, SCREEN_W - 24, 10, C_BG);
  tft.setTextSize(1);
  tft.setTextColor(C_DIM);
  tft.setCursor(14, y + 1);
  tft.print(label);

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
  tft.setTextColor(color);
  tft.setCursor(SCREEN_W - 16 - w, y + 1);
  tft.print(value);
}

void toast(const char *text, uint16_t accent) {
  tft.setRotation((SCREEN_ROTATION + 2) % 4);

  int boxH = 22;
  int boxY = SCREEN_H - boxH - 8;
  int boxX = 10;
  int boxW = SCREEN_W - 20;

  tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, C_BG);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 6, accent);
  tft.fillRect(boxX + 4, boxY + 6, 3, boxH - 12, accent);

  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(boxX + (boxW - w) / 2, boxY + (boxH - h) / 2);
  tft.print(text);

  tft.setRotation(SCREEN_ROTATION);
}

void fullStatus(const char *title, const char *sub, uint16_t accent) {
  tft.setRotation((SCREEN_ROTATION + 2) % 4);

  tft.fillScreen(C_BG);
  tft.drawRoundRect(6, 6, SCREEN_W - 12, SCREEN_H - 12, 8, C_PANEL);

  int16_t x1, y1;
  uint16_t w, h;

  tft.setTextSize(2);
  tft.setTextColor(accent);
  tft.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_W - w) / 2, SCREEN_H / 2 - 16);
  tft.print(title);

  tft.setTextSize(1);
  tft.setTextColor(C_DIM);
  tft.getTextBounds(sub, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((SCREEN_W - w) / 2, SCREEN_H / 2 + 8);
  tft.print(sub);

  tft.setRotation(SCREEN_ROTATION);
}

void startWebServer() {
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP started, connect to WiFi \"");
  Serial.print(AP_SSID);
  Serial.println("\" then visit http://192.168.4.1/");

  server.on("/", handleRoot);
  server.on("/delete", handleDeletePhoto);
  server.onNotFound(handlePhotoFile);
  server.begin();
}

void handleRoot() {
  static String names[200];
  int count = 0;
  File dir = LittleFS.open(PHOTO_DIR);
  File entry = dir.openNextFile();
  while (entry && count < 200) {
    String name = String(entry.name());
    names[count++] = name.substring(name.lastIndexOf('/') + 1);
    entry = dir.openNextFile();
  }

  String js = "[";
  for (int i = count - 1; i >= 0; i--) {
    js += "\"" + names[i] + "\"";
    if (i > 0) js += ",";
  }
  js += "]";

  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>CAMX</title><style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{background:#08080a;color:#f4f4f6;min-height:100vh;"
    "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Inter,sans-serif;"
    "-webkit-font-smoothing:antialiased;padding-bottom:60px}"
    "body:before{content:'';position:fixed;inset:0;pointer-events:none;"
    "background:radial-gradient(900px 500px at 50% -10%,#1b2a24 0%,transparent 70%)}"
    "header{position:sticky;top:0;z-index:20;"
    "background:rgba(8,8,10,.82);backdrop-filter:blur(14px);"
    "border-bottom:1px solid #1a1a1f;padding:16px 20px;"
    "display:flex;align-items:center;gap:11px}"
    ".dot{width:7px;height:7px;border-radius:50%;background:#4ade80;"
    "box-shadow:0 0 10px #4ade80;animation:pulse 2.4s infinite}"
    "@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}"
    ".logo{font-size:16px;font-weight:700;letter-spacing:.18em}"
    ".sub{font-size:11px;color:#5f5f6b;letter-spacing:.1em;"
    "border-left:1px solid #26262d;padding-left:11px}"
    ".count{margin-left:auto;font-size:12px;color:#7a7a86;"
    "background:#141418;border:1px solid #232329;border-radius:999px;"
    "padding:5px 11px;font-variant-numeric:tabular-nums}"
    "main{position:relative;padding:22px 20px;max-width:1200px;margin:0 auto}"
    ".grid{display:grid;gap:14px;"
    "grid-template-columns:repeat(auto-fill,minmax(158px,1fr))}"
    ".card{position:relative;border-radius:16px;overflow:hidden;cursor:pointer;"
    "background:#121216;border:1px solid #212128;"
    "transition:transform .2s cubic-bezier(.2,.8,.2,1),border-color .2s,box-shadow .2s}"
    ".card:hover{transform:translateY(-3px);border-color:#3a3a45;"
    "box-shadow:0 10px 30px rgba(0,0,0,.55)}"
    ".card img{width:100%;aspect-ratio:4/3;object-fit:cover;display:block;"
    "background:#1a1a20;transition:transform .35s ease}"
    ".card:hover img{transform:scale(1.05)}"
    ".meta{display:flex;justify-content:space-between;align-items:center;"
    "padding:9px 12px;font-size:11px;color:#84848f;letter-spacing:.06em;"
    "font-variant-numeric:tabular-nums;border-top:1px solid #1c1c22}"
    ".del{position:absolute;top:8px;right:8px;width:27px;height:27px;"
    "border-radius:9px;background:rgba(8,8,10,.7);backdrop-filter:blur(6px);"
    "color:#fb7185;display:flex;align-items:center;justify-content:center;"
    "font-size:16px;line-height:1;text-decoration:none;font-weight:600;"
    "opacity:0;transition:opacity .2s,background .2s,color .2s;z-index:2}"
    ".card:hover .del{opacity:1}"
    ".del:hover{background:#fb7185;color:#08080a}"
    "@media(hover:none){.del{opacity:1}}"
    ".empty{text-align:center;padding:90px 20px}"
    ".empty .ico{font-size:34px;opacity:.25;margin-bottom:14px}"
    ".empty .big{font-size:15px;color:#9a9aa4;margin-bottom:6px}"
    ".empty .txt{font-size:13px;color:#55555f}"
    "#lb{position:fixed;inset:0;z-index:50;display:none;"
    "background:rgba(4,4,6,.94);backdrop-filter:blur(8px);"
    "align-items:center;justify-content:center;flex-direction:column;gap:16px}"
    "#lb.on{display:flex}"
    "#lb img{max-width:92vw;max-height:74vh;border-radius:12px;"
    "box-shadow:0 24px 70px rgba(0,0,0,.7)}"
    ".lbbar{display:flex;align-items:center;gap:14px;font-size:12px;color:#8a8a95;"
    "font-variant-numeric:tabular-nums}"
    ".lbbtn{background:#16161b;border:1px solid #26262d;color:#e6e6ea;"
    "border-radius:9px;padding:8px 14px;font-size:13px;cursor:pointer;"
    "text-decoration:none;transition:background .18s,border-color .18s}"
    ".lbbtn:hover{background:#20202a;border-color:#3a3a45}"
    ".lbclose{position:absolute;top:18px;right:20px;font-size:26px;color:#6f6f7a;"
    "cursor:pointer;line-height:1}"
    ".lbclose:hover{color:#f4f4f6}"
    "</style></head><body>"
    "<header><span class=\"dot\"></span>"
    "<span class=\"logo\">CAMX</span>"
    "<span class=\"sub\">GALLERY</span>"
    "<span class=\"count\">" + String(count) + " photo" +
    String(count == 1 ? "" : "s") + "</span></header><main>";

  if (count == 0) {
    html += "<div class=\"empty\"><div class=\"ico\">&#9673;</div>"
            "<div class=\"big\">No photos yet</div>"
            "<div class=\"txt\">Press the shutter button to take your first shot.</div></div>";
  } else {
    html += "<div class=\"grid\">";
    for (int i = count - 1; i >= 0; i--) {
      String url = "/photos/" + names[i];
      String label = names[i];
      label.replace("img_", "");
      label.replace(".jpg", "");
      int idx = count - 1 - i;
      html += "<div class=\"card\" onclick=\"openLb(" + String(idx) + ")\">"
              "<a class=\"del\" href=\"/delete?f=" + names[i] + "\" "
              "onclick=\"event.stopPropagation();return confirm('Delete this photo?')\">&times;</a>"
              "<img src=\"" + url + "\" loading=\"lazy\">"
              "<div class=\"meta\"><span>#" + label + "</span><span>JPG</span></div>"
              "</div>";
    }
    html += "</div>";
  }

  html +=
    "</main>"
    "<div id=\"lb\" onclick=\"if(event.target.id=='lb')closeLb()\">"
    "<span class=\"lbclose\" onclick=\"closeLb()\">&times;</span>"
    "<img id=\"lbimg\">"
    "<div class=\"lbbar\">"
    "<button class=\"lbbtn\" onclick=\"step(-1)\">&larr;</button>"
    "<span id=\"lbnum\"></span>"
    "<button class=\"lbbtn\" onclick=\"step(1)\">&rarr;</button>"
    "<a class=\"lbbtn\" id=\"lbdl\" download>Download</a>"
    "</div></div>"
    "<script>"
    "var P=" + js + ";var cur=0;"
    "function openLb(i){cur=i;render();document.getElementById('lb').classList.add('on');}"
    "function closeLb(){document.getElementById('lb').classList.remove('on');}"
    "function step(d){if(!P.length)return;cur=(cur+d+P.length)%P.length;render();}"
    "function render(){var u='/photos/'+P[cur];"
    "document.getElementById('lbimg').src=u;"
    "document.getElementById('lbdl').href=u;"
    "document.getElementById('lbnum').textContent=(cur+1)+' / '+P.length;}"
    "document.addEventListener('keydown',function(e){"
    "if(!document.getElementById('lb').classList.contains('on'))return;"
    "if(e.key=='Escape')closeLb();"
    "if(e.key=='ArrowLeft')step(-1);"
    "if(e.key=='ArrowRight')step(1);});"
    "</script></body></html>";

  server.send(200, "text/html", html);
}

void handleDeletePhoto() {
  if (server.hasArg("f")) {
    String name = server.arg("f");
    if (name.indexOf('/') == -1 && name.indexOf("..") == -1) {
      LittleFS.remove(String(PHOTO_DIR) + "/" + name);
    }
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handlePhotoFile() {
  String path = server.uri();
  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  File f = LittleFS.open(path, FILE_READ);
  server.streamFile(f, "image/jpeg");
  f.close();
}

void startCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init FAILED: 0x%x\n", err);
    fullStatus("SENSOR", "INIT FAILED", C_RED);
    while (true) delay(1000);
  }
}

#define FLIP_H false
#define FLIP_V false

void drawFrameToScreen(camera_fb_t *fb) {
  if (fb->width != FRAME_W || fb->height != FRAME_H) return;
  uint16_t *pixels = (uint16_t *)fb->buf;
  static uint16_t rowBuf[SCREEN_W];

  static int srcOffset[SCREEN_W];
  static int srcXMap[SCREEN_H];
  static bool mapsReady = false;
  if (!mapsReady) {
    for (int x = 0; x < SCREEN_W; x++) {
      int sy = (x * FRAME_H) / SCREEN_W;
      if (FLIP_H) sy = FRAME_H - 1 - sy;
      srcOffset[x] = sy * FRAME_W;
    }
    for (int y = 0; y < SCREEN_H; y++) {
      int sx = (y * FRAME_W) / SCREEN_H;
      if (!FLIP_V) sx = FRAME_W - 1 - sx;
      srcXMap[y] = sx;
    }
    mapsReady = true;
  }

  tft.startWrite();
  tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);
  for (int y = 0; y < SCREEN_H; y++) {
    int srcX = srcXMap[y];
    for (int x = 0; x < SCREEN_W; x++) {
      rowBuf[x] = pixels[srcOffset[x] + srcX];
    }
    tft.writePixels(rowBuf, SCREEN_W, true, true);
  }
  tft.endWrite();
}

void loop() {
  handleButton();
  server.handleClient();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  drawFrameToScreen(fb);

  esp_camera_fb_return(fb);
}

void handleButton() {
  bool raw = digitalRead(BUTTON_PIN);

  if (raw != lastRawState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    if (raw != debouncedState) {
      debouncedState = raw;
      if (debouncedState == LOW) {
        capturePhoto();
      }
    }
  }
  lastRawState = raw;
}

void capturePhoto() {
  ledcWrite(FLASH_LED_PIN, FLASH_DUTY);
  delay(120);

  camera_fb_t *stale = esp_camera_fb_get();
  if (stale) {
    esp_camera_fb_return(stale);
  }
  camera_fb_t *fb = esp_camera_fb_get();
  ledcWrite(FLASH_LED_PIN, 0);

  if (!fb) return;

  tft.fillScreen(C_WHITE);
  delay(45);
  drawFrameToScreen(fb);

  if (!fsReady) {
    toast("NO STORAGE", C_RED);
    delay(1200);
    esp_camera_fb_return(fb);
    return;
  }

  toast("SAVING", C_DIM);

  uint8_t *jpgBuf = NULL;
  size_t jpgLen = 0;
  bool ok = false;
  if (frame2jpg(fb, JPEG_QUALITY, &jpgBuf, &jpgLen)) {
    String path = photoPath(nextPhotoIndex);
    File f = LittleFS.open(path, FILE_WRITE);
    if (f) {
      f.write(jpgBuf, jpgLen);
      f.close();
      Serial.printf("Saved %s (%u bytes)\n", path.c_str(), (unsigned)jpgLen);
      nextPhotoIndex++;
      ok = true;
    } else {
      Serial.println("Failed to open file for writing (flash full?)");
    }
    free(jpgBuf);
  } else {
    Serial.println("JPEG encode failed");
  }

  esp_camera_fb_return(fb);

  if (ok) {
    char msg[24];
    snprintf(msg, sizeof(msg), "SAVED  #%05d", nextPhotoIndex - 1);
    toast(msg, C_ACCENT);
  } else {
    toast("SAVE FAILED", C_RED);
  }
  delay(1100);
}

String photoPath(int index) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s/img_%05d.jpg", PHOTO_DIR, index);
  return String(buf);
}

int countPhotos() {
  int n = 0;
  File dir = LittleFS.open(PHOTO_DIR);
  if (!dir) return 0;
  File entry = dir.openNextFile();
  while (entry) {
    n++;
    entry = dir.openNextFile();
  }
  return n;
}

void findNextPhotoIndex() {
  int maxIndex = 0;
  File dir = LittleFS.open(PHOTO_DIR);
  if (!dir) {
    nextPhotoIndex = 1;
    return;
  }

  File entry = dir.openNextFile();
  while (entry) {
    int idx = 0;
    const char *name = entry.name();
    const char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    if (sscanf(base, "img_%d.jpg", &idx) == 1 && idx > maxIndex) {
      maxIndex = idx;
    }
    entry = dir.openNextFile();
  }
  nextPhotoIndex = maxIndex + 1;
}
