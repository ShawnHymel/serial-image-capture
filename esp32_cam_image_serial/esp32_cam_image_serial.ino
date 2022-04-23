/**
 * ESP32-CAM image capture and transmit base64 over serial
 * 
 * Author: Shawn Hymel
 * Date: April 22, 2022
 * 
 * Based on work by Rui Santos at
 * https://RandomNerdTutorials.com/esp32-cam-take-photo-save-microsd-card
 * 
 * Important:
 *  - Select board "AI Thinker ESP32-CAM"
 *  - ESP32-CAM-MB adapter does not work! Use a separate USB-Serial adapter
 *  
 *  To upload a sketch:
 *  - Click "upload button"
 *  - Wait until you see "Connecting........____"
 *  - Connect GPIO 0 (IO0) to GND and cycle power
 *  - After uploading is complete, disconnect GPIO 0 and cycle power again
 *  
 * License: MIT
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to deal 
 * in the Software without restriction, including without limitation the rights 
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
 * copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 */

#include "esp_camera.h"
#include "Arduino.h"
#include "soc/soc.h"            // Disable brownout problems
#include "soc/rtc_cntl_reg.h"   // Disable brownout problems
#include "driver/rtc_io.h"
#include <EEPROM.h>             // Read and write from flash memory

#include "base64.h"             // Used to convert data to Base64 encoding

// Settings
#define BAUD_RATE       230400  // Must match receiver application
#define SEND_IMG        1       // Transmit jpeg image over serial connection
#define JPG_QAULITY     20      // 1 is lowest, 100 is highest

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

  // Pour some Serial
  Serial.begin(BAUD_RATE);

  // Print out any issues with the WiFi library
  Serial.setDebugOutput(true);
  Serial.println();

  // Configure the camera
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
  config.pixel_format = PIXFORMAT_RGB888; // PIXFORMAT_ + GRAYSCALE|JPEG|RGB565|RGB888
  config.jpeg_quality = 15;               // 10..63 (lower number means higher quality)
  config.fb_count = 2;
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Adjust rotation, color, and flip if necessary
  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, 0); // lower the saturation
  }
#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
}

void loop() { 
  
  // Take Picture with Camera
  camera_fb_t *fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Convert raw RGB888 to JPEG
  uint8_t *jpg_buf = NULL;
  size_t jpg_len = 0;
  bool res = fmt2jpg( fb->buf, 
                      fb->len, 
                      fb->width, 
                      fb->height, 
                      fb->format, 
                      JPG_QAULITY,
                      &jpg_buf,
                      &jpg_len);
  if (!res) {
    Serial.println("ERROR: Could not convert image to JPEG format");
    return;
  }

  // Convert JPEG image to base64
  uint32_t enc_len = (jpg_len + 2) / 3 * 4;
  unsigned char *enc_buf;
  enc_buf = (unsigned char*)malloc((enc_len + 1) * sizeof(unsigned char));
  unsigned int num = encode_base64((unsigned char*)jpg_buf, jpg_len, enc_buf);

  // Send encoded image out over serial
#if SEND_IMG
  Serial.println((char*)enc_buf);
#endif

  // Free buffers
  esp_camera_fb_return(fb);
  free(jpg_buf);
  free(enc_buf);
}
