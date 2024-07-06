// Time-lapse avec ESP32 Sense
// https://tutoduino.fr/
// Copyleft 2024

#include "esp_camera.h"
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define MICROSECONDS_PER_SECOND 1000000 /* Conversion factor for micro seconds to seconds */
#define MILLISECONDS_PER_SECOND 1000    /* Conversion factor for milli seconds to seconds */

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 10
#define SIOD_GPIO_NUM 40
#define SIOC_GPIO_NUM 39

#define Y9_GPIO_NUM 48
#define Y8_GPIO_NUM 11
#define Y7_GPIO_NUM 12
#define Y6_GPIO_NUM 14
#define Y5_GPIO_NUM 16
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 17
#define Y2_GPIO_NUM 15
#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM 47
#define PCLK_GPIO_NUM 13

#define LED_GPIO_NUM 21

// Comment this line if you do not want to have debug traces
#define DEBUG

unsigned long imageCount = 1;  // File Counter
bool camera_init = false;      // camera init status
bool sd_init = false;          // sd card status
unsigned int capture_period;   // capture period in seconds

// SD card write file
bool writeFile(fs::FS &fs, const char *path, uint8_t *data, size_t len) {
  size_t rc;

#ifdef DEBUG
  Serial.printf("Writing file: %s (length=%d)\n", path, len);
#endif
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
#ifdef DEBUG
    Serial.println("Failed to open file for writing");
#endif
    return false;
  }

  rc = file.write(data, len);
  if (rc == len) {
#ifdef DEBUG
    Serial.println("File written");
#endif
  } else {
#ifdef DEBUG
    Serial.printf("Write failed (rc=%d)\n", rc);
#endif
    return false;
  }
  file.close();
  return true;
}

// Capture picture and save it to SD card
bool captureAndSavePicture(const char *fileName) {

  // Take a picture
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
#ifdef DEBUG
    Serial.println("Failed to get camera frame buffer");
#endif
    return false;
  }

  // Write picture to SD
  if (!writeFile(SD, fileName, fb->buf, fb->len)) {
#ifdef DEBUG
    Serial.println("Failed to write picture to SD");
#endif
    return false;
  }

  // Release image buffer
  esp_camera_fb_return(fb);
  return true;
}

// Read picture capture period in file "period.txt" from SD card
unsigned int readCapturePeriod(fs::FS &fs, const char *path) {
  unsigned int period;
  File file = fs.open(path, FILE_READ);
  if (!file) {
#ifdef DEBUG
    Serial.println("Failed to open file containing period");
#endif
    // return 10 seconds period by default
    return 10;
  }
  period = file.parseInt();
  file.close();
  return period;
}

void setup() {
  uint8_t cardType;
#ifdef DEBUG
  Serial.begin(115200);
#endif

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_count = 1;

  // if PSRAM is enabled, init with UXGA resolution and higher JPEG quality
  // for larger pre-allocated frame buffer.
  if (psramFound()) {
    config.frame_size = FRAMESIZE_HD;
    config.jpeg_quality = 3;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;

  } else {
// no PSRAM
#ifdef DEBUG
    Serial.println("PSRAM not found, please enable it");
#endif
    return;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
#ifdef DEBUG
    Serial.printf("Camera init failed with error 0x%x", err);
#endif
    return;
  }

  camera_init = true;  // Camera initialization check passes

  // Initialize SD card
  if (!SD.begin(21)) {
#ifdef DEBUG
    Serial.println("Card Mount Failed");
#endif
    return;
  }
  cardType = SD.cardType();

  // Determine if the type of SD card is available
  if (cardType == CARD_NONE) {
#ifdef DEBUG
    Serial.println("No SD card attached");
#endif
    return;
  }

#ifdef DEBUG
  if (cardType == CARD_MMC) {
    Serial.println("SD Card Type: MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SD Card Type: SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SD Card Type: SDHC");
  } else {
    Serial.println("SD Card Type: UNKNOWN ");
  }
#endif

  sd_init = true;  // sd initialization check passes

  // wait 10 seconds to start taking pictures
  delay(10000);

  capture_period = readCapturePeriod(SD, "/period.txt");
#ifdef DEBUG
  Serial.printf("XIAO ESP32S3 Sense is starting to capture images every %d seconds \n", capture_period);
#endif
}

void loop() {

  // Camera & SD available, start taking pictures
  if (camera_init && sd_init) {
    char filename[32];
    // the file name ends with the image counter on 5 digits
    sprintf(filename, "/image%05d.jpg", imageCount);
    // take a picture and store it in file filename
    if (captureAndSavePicture(filename)) {
#ifdef DEBUG
      Serial.printf("Saved picture: %s\r\n", filename);
#endif
      // image was correctly taken and saved, increase the image counter
      imageCount++;
    };

    // Wait capture_period seconds
    delay(capture_period * MILLISECONDS_PER_SECOND);
  }
}
