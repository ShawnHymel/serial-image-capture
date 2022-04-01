/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-take-photo-save-microsd-card
  
  IMPORTANT!!! 
   - Select Board "AI Thinker ESP32-CAM"
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory

#include "base64.h"

// Baud rate (must match baud rate in receiver application)
#define BAUD_RATE         230400

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void setup() {

  // Disable brownout detection
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
 
  Serial.begin(BAUD_RATE);
  //Serial.setDebugOutput(true);
  //Serial.println();
  
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

  // Image settings
  // https://github.com/espressif/esp32-camera/blob/master/driver/include/sensor.h
  config.frame_size = FRAMESIZE_240X240;  // FRAMESIZE_ + 96X97|QQVGA|QCIF|HQVGA|240X240|QVGA
  config.pixel_format = PIXFORMAT_JPEG;   // PIXFORMAT_ + GRAYSCALE|JPEG|RGB565|RGB888
  config.jpeg_quality = 15;               // 10..63 (lower number means higher quality)
  config.fb_count = 2;
  
//  if(psramFound()){
//    config.frame_size = FRAMESIZE_VGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
//    config.jpeg_quality = 10;
//    config.fb_count = 2;
//  } else {
//    config.frame_size = FRAMESIZE_SVGA;
//    config.jpeg_quality = 12;
//    config.fb_count = 1;
//  }
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void loop() { 
  
  // Take Picture with Camera
  camera_fb_t *fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Convert JPEG image to base64
  uint32_t enc_len = (fb->len + 2) / 3 * 4;
  unsigned char *enc_buf;
  enc_buf = (unsigned char*)malloc((enc_len + 1) * sizeof(unsigned char));
  unsigned int num = encode_base64((unsigned char*)fb->buf, fb->len, enc_buf);

  // Send encoded image out over serial
  Serial.println((char*)enc_buf);

  // Free buffers
  free(enc_buf);
  esp_camera_fb_return(fb);
}
