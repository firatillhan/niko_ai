# Robo - ESP32-S3 Sesli Yapay Zekâ Asistanı

Robo, ESP32-S3 üzerinde çalışan, sesli komutları algılayan ve yapay zekâ ile cevap veren kişisel bir sesli asistandır.

Proje, ESP32-S3 ile Mac üzerinde çalışan bir Python sunucusu arasında WebSocket bağlantısı kullanır.

Robo'nun amacı, kullanıcıyla doğal ve eğlenceli bir şekilde konuşmaktır.

## Özellikler

- ESP32-S3 üzerinden ses kaydı
- I2S mikrofon desteği
- Faster-Whisper ile sesin yazıya çevrilmesi
- LM Studio üzerinden yapay zekâ cevapları
- Piper TTS ile yazının sese dönüştürülmesi
- PCM5102A DAC üzerinden ses çıkışı
- OLED ekranda Robo yüz animasyonu
- Türkçe ve İngilizce dil seçimi
- WebSocket üzerinden ESP32 ve Python haberleşmesi
- İngilizce konuşma pratiğine uygun doğal sohbet
- Robo'nun kısa ve eğlenceli cevaplar vermesi

## Kullanılan Donanımlar

- ESP32-S3
- 1.3 inch SH1106 128x64 I2C OLED ekran
- I2S mikrofon
- PCM5102A I2S DAC
- 3.5 mm ses çıkışlı hoparlör veya ses sistemi
- Buton

## ESP32 Pin Bağlantıları

### OLED - SH1106

| OLED pini | ESP32-S3 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

### Buton

| Buton pini | ESP32-S3 |
|---|---|
| Bir uç | GPIO 4 |
| Diğer uç | GND |

Buton kod içerisinde `INPUT_PULLUP` olarak kullanılmaktadır.

### I2S Mikrofon

| Mikrofon pini | ESP32-S3 |
|---|---|
| WS | GPIO 16 |
| SD | GPIO 15 |
| SCK | GPIO 17 |
| VCC | 3.3V |
| GND | GND |

### PCM5102A DAC

| PCM5102A pini | ESP32-S3 |
|---|---|
| BCK | GPIO 11 |
| LCK / WS | GPIO 10 |
| DIN | GPIO 12 |
| VIN | 5V |
| GND | GND |

PCM5102A ses çıkışı, 3.5 mm girişli aktif hoparlöre veya harici ses sistemine bağlanabilir.

## Kullanılan Yazılımlar

### ESP32

- Arduino IDE
- ESP32 Arduino Core
- U8g2
- ArduinoWebsockets
- ESP32 I2S kütüphanesi

### Python

- Python 3
- faster-whisper
- Piper TTS
- websockets
- requests
- wave
- numpy

### Yapay Zekâ

- LM Studio
- Qwen modeli
- Faster-Whisper
- Piper TTS

## Çalışma Mantığı

Projenin çalışma sırası şu şekildedir:

1. ESP32-S3 Wi-Fi ağına bağlanır.
2. Kullanıcı Türkçe veya İngilizce dilini seçer.
3. ESP32, Python WebSocket sunucusuna bağlanır.
4. Kullanıcı butona basılı tuttuğu sürece ses kaydedilir.
5. Ses Python sunucusuna gönderilir.
6. Faster-Whisper sesi yazıya çevirir.
7. Yazı LM Studio üzerindeki yapay zekâ modeline gönderilir.
8. Yapay zekâ cevap üretir.
9. Piper TTS cevabı sese dönüştürür.
10. Ses ESP32-S3'e gönderilir.
11. PCM5102A üzerinden hoparlörden ses duyulur.
12. OLED ekranda Robo'nun yüz animasyonu oynatılır.

## Python Sunucusunu Çalıştırma

Öncelikle gerekli Python paketlerini yükleyin:

```bash
pip install faster-whisper piper-tts websockets requests numpy
```

LM Studio'yu açın ve yerel API sunucusunu başlatın.

Varsayılan bağlantı:

```text
http://localhost:1234/v1/chat/completions
```

Python dosyasında aşağıdaki ayarları kendi bilgisayarınıza göre düzenleyin:

```python
LM_STUDIO_URL = "http://localhost:1234/v1/chat/completions"
LM_MODEL = "qwen/qwen3.5-9b"
```

WebSocket sunucusu varsayılan olarak `8765` portunu kullanır.

Python sunucusunu çalıştırmak için:

```bash
python3 app.py
```

> Python dosyanızın adı farklıysa `server.py` yerine kendi dosya adınızı yazın.
