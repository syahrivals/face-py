#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "board_config.h"

// ===========================
// Ganti SSID dan Password
// ===========================
const char* ssid = "4737";
const char* password = "1sampai6";

// ===========================
// Ganti IP sesuai komputer yang menjalankan face_server.py
// Pastikan IP ini SAMA dengan IP komputer (cek pakai `ipconfig`)
// ===========================
String serverUrl = "http://10.110.171.150:5000/recognize";
  // contoh

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  // ===========================
  // Konfigurasi kamera
  // ===========================
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;  
  config.frame_size = FRAMESIZE_VGA;      
  config.jpeg_quality = 12;               
  config.fb_count = 1;

  // Inisialisasi kamera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed!");
    while (true);
  }
  Serial.println("Camera ready!");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    delay(2000);
    return;
  }

  // Ambil foto dari kamera
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed!");
    delay(2000);
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "image/jpeg");

  // Kirim gambar ke server Flask
  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Response: ");
    Serial.println(response);
  } else {
    Serial.printf("HTTP POST failed, error: %d\n", httpResponseCode);
  }

  http.end();
  esp_camera_fb_return(fb);

  delay(3000);  // kirim setiap 3 detik
}
