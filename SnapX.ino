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

#define SCREEN_W 128
#define SCREEN_H 160
#define FRAME_W 320
#define FRAME_H 240

void drawFrameToScreen(camera_fb_t *fb) {
  if (fb->width != FRAME_W || fb->height != FRAME_H) return;
  uint16_t *pixels = (uint16_t *)fb->buf;
  static uint16_t rowBuf[SCREEN_W];

  static int xMap[SCREEN_W];
  static bool xMapReady = false;
  if (!xMapReady) {
    for (int x = 0; x < SCREEN_W; x++) {
      xMap[x] = (x * FRAME_W) / SCREEN_W;
    }
    xMapReady = true;
  }

  tft.startWrite();
  tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);
  for (int y = 0; y < SCREEN_H; y++) {
    int srcY = (y * FRAME_H) / SCREEN_H;
    uint16_t *srcRow = pixels + srcY * FRAME_W;
    for (int x = 0; x < SCREEN_W; x++) {
      rowBuf[x] = srcRow[xMap[x]];
    }
    tft.writePixels(rowBuf, SCREEN_W, true, true);
  }
  tft.endWrite();
}

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
String photoPath(int index);
void showStatus(const char *text);
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
  tft.setRotation(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.setSPISpeed(27000000);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    showStatus("FS FAIL");
  } else {
    fsReady = true;
    if (!LittleFS.exists(PHOTO_DIR)) {
      LittleFS.mkdir(PHOTO_DIR);
    }
    findNextPhotoIndex();
    Serial.printf("LittleFS ready, next photo index %d\n", nextPhotoIndex);
    showStatus("FS OK");
  }
  delay(1000);
  tft.fillScreen(ST77XX_BLACK);

  startWebServer();
  startCamera();

  Serial.println("CAMX ready");
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

  String html =
    "<!DOCTYPE html><html><head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>CAMX</title><style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{background:#0c0c0e;color:#f2f2f4;"
    "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Inter,sans-serif;"
    "-webkit-font-smoothing:antialiased;padding-bottom:48px}"
    "header{position:sticky;top:0;z-index:10;background:rgba(12,12,14,.85);"
    "backdrop-filter:blur(12px);border-bottom:1px solid #1e1e22;"
    "padding:18px 20px;display:flex;align-items:baseline;gap:10px}"
    ".logo{font-size:17px;font-weight:650;letter-spacing:.14em}"
    ".dot{width:7px;height:7px;border-radius:50%;background:#4ade80;"
    "align-self:center;box-shadow:0 0 8px #4ade80}"
    ".count{margin-left:auto;font-size:13px;color:#6f6f7a;"
    "font-variant-numeric:tabular-nums}"
    "main{padding:20px;max-width:1100px;margin:0 auto}"
    ".grid{display:grid;gap:14px;"
    "grid-template-columns:repeat(auto-fill,minmax(150px,1fr))}"
    ".card{position:relative;border-radius:14px;overflow:hidden;"
    "background:#151518;border:1px solid #232329;"
    "transition:transform .18s ease,border-color .18s ease}"
    ".card:hover{transform:translateY(-2px);border-color:#34343d}"
    ".card img{width:100%;aspect-ratio:4/3;object-fit:cover;display:block;"
    "background:#1c1c21}"
    ".meta{padding:9px 11px;font-size:12px;color:#8a8a95;"
    "display:flex;justify-content:space-between;align-items:center;"
    "font-variant-numeric:tabular-nums}"
    ".del{position:absolute;top:8px;right:8px;width:26px;height:26px;"
    "border-radius:8px;background:rgba(10,10,12,.72);color:#fb7185;"
    "display:flex;align-items:center;justify-content:center;"
    "font-size:15px;line-height:1;text-decoration:none;font-weight:600;"
    "opacity:0;transition:opacity .18s ease,background .18s ease}"
    ".card:hover .del{opacity:1}"
    ".del:hover{background:#fb7185;color:#0c0c0e}"
    "@media(hover:none){.del{opacity:1}}"
    ".empty{text-align:center;padding:80px 20px;color:#5c5c66}"
    ".empty .big{font-size:15px;color:#8a8a95;margin-bottom:6px}"
    ".empty .sub{font-size:13px}"
    "</style></head><body>"
    "<header><span class=\"dot\"></span>"
    "<span class=\"logo\">CAMX</span>"
    "<span class=\"count\">" + String(count) + " photo" +
    String(count == 1 ? "" : "s") + "</span></header><main>";

  if (count == 0) {
    html += "<div class=\"empty\"><div class=\"big\">Nothing here yet</div>"
            "<div class=\"sub\">Press the shutter button to take your first shot.</div></div>";
  } else {
    html += "<div class=\"grid\">";
    for (int i = count - 1; i >= 0; i--) {
      String url = "/photos/" + names[i];
      String label = names[i];
      label.replace("img_", "");
      label.replace(".jpg", "");
      html += "<div class=\"card\">"
              "<a class=\"del\" href=\"/delete?f=" + names[i] + "\" "
              "onclick=\"return confirm('Delete this photo?')\">&times;</a>"
              "<a href=\"" + url + "\" target=\"_blank\">"
              "<img src=\"" + url + "\" loading=\"lazy\"></a>"
              "<div class=\"meta\"><span>#" + label + "</span></div>"
              "</div>";
    }
    html += "</div>";
  }

  html += "</main></body></html>";
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
    while (true) delay(1000);
  }
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

  if (!fb) {
    return;
  }

  drawFrameToScreen(fb);
  delay(1000);

  if (!fsReady) {
    showStatus("NO FS");
    delay(1000);
    esp_camera_fb_return(fb);
    return;
  }

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
  showStatus(ok ? "SAVED" : "SAVE FAIL");
  delay(1000);
}

void showStatus(const char *text) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(3);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, (tft.height() - h) / 2);
  tft.print(text);
}

String photoPath(int index) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s/img_%05d.jpg", PHOTO_DIR, index);
  return String(buf);
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
