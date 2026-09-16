//Bu kod tamam elleme.

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>

using namespace websockets;

// =====================================================
// WIFI
// =====================================================

const char* ssid     = "wifi";
const char* password = "2020";

// =====================================================
// OLED
// =====================================================

#define OLED_SDA 8
#define OLED_SCL 9

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0,U8X8_PIN_NONE,OLED_SCL,OLED_SDA);

// =====================================================
// MEKANIK BUTON
// GPIO 4 -> BUTON -> GND
// =====================================================

#define BUTTON_PIN 4

// =====================================================
// I2S MICROPHONE
// =====================================================

#define MIC_I2S_PORT I2S_NUM_0

#define I2S_WS   16
#define I2S_SD   15
#define I2S_SCK  17

#define MIC_SAMPLE_RATE 16000
#define bufferLen 128

int32_t sBuffer[bufferLen];

// =====================================================
// PCM5102A
// =====================================================

#define DAC_I2S_PORT I2S_NUM_1

#define DAC_BCK 11
#define DAC_LCK 10
#define DAC_DIN 12

#define DAC_SAMPLE_RATE 44100

// =====================================================
// Robo AI YÜZÜ
// =====================================================

void drawRoboFace(int eyeDirection, bool mouthOpen) {

  u8g2.clearBuffer();

  // =================================================
  // GÖZLER
  // =================================================

  // Sol göz
  u8g2.drawRBox(25,18,22,18,5);

  // Sağ göz
  u8g2.drawRBox(81,18,22,18,5);

  // =================================================
  // GÖZ BEBEKLERİ
  // =================================================

  int pupilOffset = eyeDirection * 5;

  // Sol göz bebeği
  u8g2.setDrawColor(0);

  u8g2.drawDisc(36 + pupilOffset,27,4);

  // Sağ göz bebeği
  u8g2.drawDisc(92 + pupilOffset,27,4);

  // Tekrar çizim rengi
  u8g2.setDrawColor(1);

  // =================================================
  // AĞIZ
  // =================================================

  if (mouthOpen) {
    u8g2.drawRBox(48,45,32,10,4);
  } else {
    u8g2.drawRBox(52,48,24,4,2);
  }
  u8g2.sendBuffer();
}

// =====================================================
// WEBSOCKET
// =====================================================

WebsocketsClient client;

const char* wsHost =
  "ws://192.168.1.35:8765";

// =====================================================
// KAYIT
// =====================================================

#define MAX_RECORD_SECONDS 20

#define MAX_SAMPLES \
  (MIC_SAMPLE_RATE * MAX_RECORD_SECONDS)

int16_t* recordBuffer = nullptr;

int recordIndex = 0;

// =====================================================
// BUTON
// =====================================================

unsigned long lastButtonTime = 0;

const unsigned long buttonDelay = 500;

// =====================================================
// DİL SEÇİMİ
// =====================================================

enum Language {
  LANGUAGE_NONE,
  LANGUAGE_TR,
  LANGUAGE_EN
};

Language selectedLanguage = LANGUAGE_NONE;

// =====================================================
// MICROPHONE I2S
// =====================================================

void setupMic() {

  i2s_config_t config = {};

  config.mode =(i2s_mode_t)( I2S_MODE_MASTER | I2S_MODE_RX );
  config.sample_rate = MIC_SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = bufferLen;
  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;
  esp_err_t result = i2s_driver_install(MIC_I2S_PORT,&config,0,NULL);
  Serial.print("MIC driver: ");
  Serial.println(result);

  // -------------------------

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = I2S_SCK;
  pins.ws_io_num = I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = I2S_SD;
  result =i2s_set_pin(MIC_I2S_PORT,&pins);
  Serial.print("MIC pin: ");
  Serial.println(result);
  i2s_start(MIC_I2S_PORT);
  
  Serial.println("MIC HAZIR");
}

// =====================================================
// PCM5102A I2S
// =====================================================

void setupDAC() {

  i2s_config_t config = {};

  config.mode =
    (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);

  config.sample_rate = DAC_SAMPLE_RATE;

  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;

  // PCM5102A stereo
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;

  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;

  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;

  config.dma_buf_count = 8;
  config.dma_buf_len = 512;
  config.use_apll = true;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;
  esp_err_t result = i2s_driver_install(DAC_I2S_PORT,&config,0,NULL);

  Serial.print("DAC driver: ");
  Serial.println(result);

  if (result != ESP_OK) {
    Serial.println("DAC DRIVER HATASI!");
    return;
  }

  // -------------------------

  i2s_pin_config_t pins = {};

  // PCM5102A MCLK kullanmıyoruz
  pins.mck_io_num =I2S_PIN_NO_CHANGE;

  pins.bck_io_num =DAC_BCK;
  pins.ws_io_num =DAC_LCK;
  pins.data_out_num =DAC_DIN;
  pins.data_in_num =I2S_PIN_NO_CHANGE;
  result =i2s_set_pin(DAC_I2S_PORT,&pins);

  Serial.print("DAC pin: ");
  Serial.println(result);

  if (result != ESP_OK) {
    Serial.println("DAC PIN HATASI!");
  }

  i2s_zero_dma_buffer(DAC_I2S_PORT);
  Serial.println("PCM5102A HAZIR");
}

// =====================================================
// SES OYNAT
//
// Python'dan:
// 44100 Hz
// 16 bit
// MONO PCM
//
// PCM5102A:
// 44100 Hz
// 16 bit
// STEREO
// =====================================================

void playAudio(const uint8_t* data,size_t length) {

  if (length < 2) { return; }

  Serial.print("Ses oynatiliyor: ");

  Serial.print(length);

  Serial.println(" byte");

  // 256 MONO sample
  // 512 STEREO sample

  int16_t stereoBuffer[512];
  size_t totalSamples =length / 2;
  size_t position = 0;

  while (position < totalSamples) {

    static unsigned long lastFaceUpdate = 0;
    static bool mouthState = false;
    static int eyeDirection = 0;

    if (millis() - lastFaceUpdate >= 100) {

      lastFaceUpdate = millis();
      mouthState = !mouthState;

      if (eyeDirection == 0) {
        eyeDirection = -1;
      }
      else if (eyeDirection == -1) {
        eyeDirection = 1;
      }
      else {
        eyeDirection = 0;
      }

      drawRoboFace(eyeDirection,mouthState);
    }

    size_t chunk = totalSamples - position;

    if (chunk > 256) {
      chunk = 256;
    }

    // MONO -> STEREO

    for (size_t i = 0;i < chunk;i++) {

      int16_t sample;
      memcpy(&sample,data + ((position + i) * 2),2);
      stereoBuffer[i * 2] = sample;
      stereoBuffer[i * 2 + 1] = sample;

    }

    size_t bytesWritten = 0;

    esp_err_t result = i2s_write(DAC_I2S_PORT,stereoBuffer,chunk * 4,&bytesWritten,portMAX_DELAY);

    if (result != ESP_OK) {

      Serial.print("I2S WRITE HATA: ");
      Serial.println(result);
      break;
    }
    position += chunk;
  }
  Serial.println("Ses tamamlandi");
}

// =====================================================
// WEBSOCKET MESAJ
// =====================================================

void onMessageCallback(
  WebsocketsMessage message) {

  // ===================================================
  // BINARY = TTS SESİ
  // ===================================================

  if (message.isBinary()) {

    const char* audioData =message.c_str();

    size_t audioLength =message.length();
    Serial.print("TTS geldi: ");
    Serial.print(audioLength);
    Serial.println(" byte");

    playAudio((const uint8_t*) audioData,audioLength);
    drawRoboFace(0, false);
    return;
  }

  // ===================================================
  // TEXT = Robo CEVABI
  // ===================================================

  String msg =message.data();

  Serial.print("Robo: ");
  Serial.println(msg);
}

// =====================================================
// AÇILIŞ DİL SEÇİMİ
// =====================================================

void selectLanguage() {

  selectedLanguage = LANGUAGE_NONE;
  int pressCount = 0;
  unsigned long lastPressTime = 0;
  // Dil seçim ekranı
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(35,20,"DIL SEC");
  u8g2.drawStr(25,45,"TR      EN");
  u8g2.sendBuffer();

  while (true) {

    // -----------------------------------------------
    // BUTONA BASILDI
    // -----------------------------------------------

    if (digitalRead(BUTTON_PIN) == LOW) {

      // Butonun bırakılmasını bekle
      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }

      pressCount++;
      lastPressTime = millis();

      // ---------------------------------------------
      // 1 BASIŞ = TÜRKÇE
      // 2 BASIŞ = İNGİLİZCE
      // ---------------------------------------------

      if (pressCount == 1) {

        selectedLanguage = LANGUAGE_TR;
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(35,20,"DIL SEC");
        u8g2.drawStr(20,45,"[TR]    EN");
        u8g2.sendBuffer();
      }
      else if (pressCount == 2) {

        selectedLanguage = LANGUAGE_EN;
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(35,20,"DIL SEC");
        u8g2.drawStr(20,45," TR    [EN]");
        u8g2.sendBuffer();
        // 2. basıştan sonra seçim tamam
        delay(2000);

        break;
      }
    }

    // -----------------------------------------------
    // 1 BASIŞTAN SONRA 2 SANİYE BEKLE
    // -----------------------------------------------

    if (pressCount == 1 && millis() - lastPressTime >= 2000) {

      selectedLanguage = LANGUAGE_TR;
      delay(100);
      break;

    }

    delay(10);
  }

  // -----------------------------------------------
  // SEÇİM BİTTİ
  // -----------------------------------------------

  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("======================");
  Serial.println("       Robo AI");
  Serial.println("======================");

  // ===================================================
  // BUTON
  // ===================================================

  pinMode(BUTTON_PIN,INPUT_PULLUP);

  // ===================================================
  // OLED
  // ===================================================

  Wire.begin(OLED_SDA,OLED_SCL);
  u8g2.setI2CAddress(0x3C * 2);
  u8g2.begin();

  // ===================================================
  // MICROPHONE
  // ===================================================

  setupMic();

  // ===================================================
  // PCM5102A
  // ===================================================

  setupDAC();

  // ===================================================
  // PSRAM
  // ===================================================

  recordBuffer =(int16_t*)ps_malloc(MAX_SAMPLES *sizeof(int16_t));

  if (recordBuffer == nullptr) {

    Serial.println("PSRAM HATA!");
    u8g2.clearBuffer();
    u8g2.drawStr(5,30,"PSRAM HATA!");
    u8g2.sendBuffer();
    while (true) {
      delay(1000);
    }
  }

  Serial.println("PSRAM BUFFER OK");

  // ===================================================
  // WIFI
  // ===================================================

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  u8g2.clearBuffer();
  u8g2.setFont(
  u8g2_font_6x10_tr);  
  u8g2.drawStr(5,30,"WiFi baglaniyor...");
  u8g2.sendBuffer();
  Serial.println("WiFi baglaniyor...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
  }

  Serial.println("WiFi BAGLANDI");
  Serial.print("IP: ");
  Serial.println( WiFi.localIP());
  u8g2.clearBuffer();
  u8g2.drawStr(5,30,"WiFi baglandi");
  u8g2.sendBuffer();
  delay(2000);

// ===================================================
// DİL SEÇİMİ
// ===================================================

  selectLanguage();

  // ===================================================
  // WEBSOCKET
  // ===================================================

  client.onMessage(onMessageCallback);

  if (client.connect(wsHost)) {

    Serial.println("WebSocket BAGLANDI");

    // ===================================================
    // SEÇİLEN DİLİ PYTHON'A GÖNDER
    // ===================================================

    if (selectedLanguage == LANGUAGE_TR) {
      client.send("LANG:TR");
      Serial.println("Dil: TURKCE");
    }
    else if (selectedLanguage == LANGUAGE_EN) {
      client.send("LANG:EN");
      Serial.println("Dil: INGILIZCE");
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(5,30,"ESP32 baglandi");
    u8g2.sendBuffer();
    delay(2000);
    drawRoboFace(0, false);
  } else {
    Serial.println("WebSocket BAGLANAMADI");
  }

  delay(500);
}
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// =====================================================
// LOOP
// =====================================================

void loop() {

  client.poll();

  // ===================================================
  // BUTON
  // ===================================================

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastButtonTime > buttonDelay) {

      lastButtonTime = millis();
      recordIndex = 0;

      Serial.println();
      Serial.println("======================");
      Serial.println("KAYIT BASLADI" );
      Serial.println("======================");

      drawRoboFace(0, true);

      // -----------------------------------------------
      // BUTONA BASILI TUTULDUĞU SÜRECE KAYDET
      // -----------------------------------------------

        while (digitalRead(BUTTON_PIN) == LOW && recordIndex < MAX_SAMPLES ) {

        static unsigned long lastFaceMove = 0;
        static int faceDirection = 0;

        // Robo'nun gözlerini hareket ettir
        if (millis() - lastFaceMove > 500) {

          // lastFaceMove = millis();

          if (faceDirection == 0) {
            faceDirection = -1;
          }
          else if (faceDirection == -1) {
            faceDirection = 1;
          }
          else {
            faceDirection = 0;
          }

          drawRoboFace(faceDirection, true);
        }

        size_t bytesIn = 0;
        i2s_read(MIC_I2S_PORT,&sBuffer,sizeof(sBuffer),&bytesIn,portMAX_DELAY);
        int samplesRead = bytesIn / sizeof(int32_t);

        for (int i = 0; i < samplesRead && recordIndex < MAX_SAMPLES; i++) {
          int16_t sample = (int16_t)(sBuffer[i] >> 14);
          recordBuffer[recordIndex] = sample;
          recordIndex++;
        }
      }

      Serial.print("Kayit sample: ");
      Serial.println(recordIndex);

      // -----------------------------------------------
      // GÖNDER
      // -----------------------------------------------

      if (recordIndex > 0 && client.available()) {
        size_t bytesToSend =recordIndex *sizeof(int16_t);
        Serial.print("-----Gonderilen byte: ");
        Serial.println(bytesToSend);
        client.sendBinary((const char*)recordBuffer,bytesToSend);
      } else {
        Serial.println("WebSocket bagli degil!");
      }

      // -----------------------------------------------
      // BEKLE
      // -----------------------------------------------

    }
  }

  delay(1);
} //son satır 1114
