/**
 * ESP32-CAM image capture and transmit base64 over serial
 * 
 * Author: Shawn Hymel
 * Date: June 15, 2022
 * 
 * Based on work by Rui Santos at
 * https://RandomNerdTutorials.com/esp32-cam-take-photo-save-microsd-card
 * 
 * Important:
 *  - Install TinyMLShield Arduino library
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

#include <TinyMLShield.h>
#include "base64.h"             // Used to convert data to Base64 encoding

// Preprocessor settings
#define BAUD_RATE         230400          // Must match receiver application
#define SEND_IMG          1               // Transmit raw RGB888 image over serial

// Camera settings: https://github.com/tinyMLx/arduino-library/blob/main/src/OV767X_TinyMLx.h
static const int cam_type = OV7675;       // OV7675
static const int cam_resolution = QQVGA;  // QQVGA, QCIF, QVGA, CIF, VGA
static const int cam_format = RGB565;     // YUV422, RGB444, RGB565, GRAYSCALE
static const int cam_fps = 1;             // 1, 5, 10, 15, 30
static const int cam_width = 160;         // Width of resolution
static const int cam_height = 120;        // Height of resolution
static const int cam_bytes_per_pixel = 2;

// Other image settings
static const int crop_width = 64;         // Image width after crop
static const int crop_height = 64;        // Image height after crop
static const int rgb888_bytes_per_pixel = 3;

// EIML constants
static const char EIML_HEADER_SIZE = 12;
static const char EIML_SOF_SIZE = 3;
static const char EIML_SOF[] = {0xFF, 0xA0, 0xFF};

// EIML formats
typedef enum
{
  EIML_RESERVED = 0,
  EIML_RGB888 = 1
} EimlFormat;

// Return codes for image manipulation
typedef enum
{
  EIML_OK = 0,
  EIML_ERROR = 1
} EimlRet;

// Transmission header for raw image (must convert nicely to base64)
// |     SOF     |  format  |   width   |   height  |
// | xFF xA0 XFF | [1 byte] | [4 bytes] | [4 bytes] |
typedef struct EimlHeader
{
  uint8_t format;
  uint32_t width;
  uint32_t height;
} EimlHeader;

// Image buffers
static uint8_t cam_img[cam_width * cam_height * cam_bytes_per_pixel];
static uint8_t crop_img[crop_width * crop_height * cam_bytes_per_pixel];
static uint8_t rgb888_img[crop_width * crop_height * rgb888_bytes_per_pixel];

// Global variables
static int cam_bytes_per_frame;

/*******************************************************************************
 * Functions
 */

// Function: crop an image, store in another buffer
EimlRet eiml_crop_center(const unsigned char *in_pixels, 
                          unsigned int in_width, 
                          unsigned int in_height,
                          unsigned char *out_pixels,
                          unsigned int out_width,
                          unsigned int out_height,
                          unsigned int bytes_per_pixel)
{
  unsigned int in_x_offset;
  unsigned int in_y_offset;
  unsigned int out_x_offset;
  unsigned int out_y_offset;

  // Verify crop is smaller
  if ((in_width < out_width) || (in_height < out_height))
  {
    return EIML_ERROR;
  }

  // Copy pixels (in center of input image) to new buffer
  unsigned int out_buf_len = out_width * out_height;

  // Go through each row
  for (unsigned int y = 0; y < out_height; y++)
  {
    in_y_offset = bytes_per_pixel * in_width * 
                  ((in_height - out_height) / 2 + y);
    out_y_offset = bytes_per_pixel * out_width * y;

    // Go through each pixel in each row
    for (unsigned int x = 0; x < out_width; x++) 
    {
      in_x_offset = bytes_per_pixel * ((in_width - out_width) / 2 + x);
      out_x_offset = bytes_per_pixel * x;

      // go through each byte in each pixel
      for (unsigned int b = 0; b < bytes_per_pixel; b++)
      {
        out_pixels[out_y_offset + out_x_offset + b] = 
                                  in_pixels[in_y_offset + in_x_offset + b];
      }
    }
  }

  return EIML_OK;
}

// Function: Convert RGB565 to RGB888
EimlRet eiml_rgb565_to_rgb888( const unsigned char *in_pixels,
                                unsigned char *out_pixels,
                                unsigned int num_pixels)
{
  unsigned char r;
  unsigned char g;
  unsigned char b;

  // Go through each pixel
  for (unsigned int i = 0; i < num_pixels; i++)
  {
    // Get RGB values
    r = in_pixels[2 * i] & 0xF8;
    g = (in_pixels[2 * i] << 5) | 
        ((in_pixels[(2 * i) + 1] & 0xE0) >> 3);
    b = in_pixels[(2 * i) + 1] << 3;

    // Copy RGB values to new buffer
    out_pixels[3 * i] = r;
    out_pixels[(3 * i) + 1] = g;
    out_pixels[(3 * i) + 2] = b;
  }

  return EIML_OK;
}

// Function: generate header
EimlRet eiml_generate_header(EimlHeader header, unsigned char *out_header)
{
  // Copy SOF
  for (int i = 0; i < EIML_SOF_SIZE; i++)
  {
    out_header[i] = EIML_SOF[i];
  }

 // Copy format
 out_header[EIML_SOF_SIZE] = header.format;

 // Copy width and height (keep little endianness)
 for (int i = 0; i < 4; i++)
 {
  out_header[EIML_SOF_SIZE + 1 + i] = (header.width >> (i * 8)) & 0xFF;
 }
 for (int i = 0; i < 4; i++)
 {
  out_header[EIML_SOF_SIZE + 5 + i] = (header.height >> (i * 8)) & 0xFF;
 }

 return EIML_OK;
}

/*******************************************************************************
 * Main
 */

void setup() {

  // Wait for serial to connect
  Serial.begin(BAUD_RATE);
  while (!Serial);

  // Initialize TinyML shield
  initializeShield();

  // Initialize the OV7675 camera
  if (!Camera.begin(cam_resolution, cam_format, cam_fps, cam_type)) {
    Serial.println("Failed to initialize camera");
    while (1);
  }
  cam_bytes_per_frame = Camera.width() * Camera.height() * Camera.bytesPerPixel();
}

void loop() {

  static EimlRet eiml_ret;
  static int c888_img_bytes = crop_width * crop_height * rgb888_bytes_per_pixel;

  // Capture frame
  Camera.readFrame(cam_img);

  // Crop image
  eiml_ret = eiml_crop_center(cam_img,
                              cam_width,
                              cam_height,
                              crop_img,
                              crop_width,
                              crop_height,
                              cam_bytes_per_pixel);
  if (eiml_ret != EIML_OK) {
    Serial.println("Image cropping error");
    return;
  }

  // Convert cropped image to RGB888
  eiml_ret = eiml_rgb565_to_rgb888(crop_img, rgb888_img, crop_width * crop_height);
  if (eiml_ret != EIML_OK) {
    Serial.println("Image conversion error");
    return;
  }

  // Convert cropped RGB888 image to base64
  uint32_t enc_len = (c888_img_bytes + 2) / 3 * 4;
  unsigned char *enc_buf;
  enc_buf = (unsigned char*)malloc((enc_len + 1) * sizeof(unsigned char));
  unsigned int num = encode_base64(rgb888_img, c888_img_bytes, enc_buf);
  
  // Send encoded image out over serial
#if SEND_IMG

  // Construct header
  EimlHeader header;
  header.format = EIML_RGB888;
  header.width = crop_width;
  header.height = crop_height;

  // Generate header
  unsigned char header_buf[EIML_HEADER_SIZE];
  eiml_ret = eiml_generate_header(header, header_buf);
  if (eiml_ret != EIML_OK) {
    Serial.println("Error generating header");
    return;
  }

  // Convert header to base64
  unsigned char enc_header[(EIML_HEADER_SIZE + 2) / 3 * 4];
  num = encode_base64(header_buf, EIML_HEADER_SIZE, enc_header);

  // Print header and image body
  Serial.print((char*)enc_header);
  Serial.println((char*)enc_buf);
#endif

  // Free buffers
  free(enc_buf);
}
