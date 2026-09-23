#bu kod tamam. elleme. 16 eylül 01:00
import asyncio
import json
import os
import wave
import audioop
import tempfile
import re
import websockets
from faster_whisper import WhisperModel
import requests
import subprocess


# ============================================================
# AYARLAR
# ============================================================

HOST = "0.0.0.0"
PORT = 8765

LM_STUDIO_URL = "http://localhost:1234/v1/chat/completions"
LM_MODEL = "qwen/qwen3.5-9b"

# Piper modelleri
PIPER_TR = os.path.expanduser("~/piper_voices/tr_TR-dfki-medium")
PIPER_EN = os.path.expanduser("~/piper_voices/en_US-amy-medium")

# ============================================================
# WHISPER
# ============================================================

print("Whisper yükleniyor...")

whisper_model = WhisperModel(
    "medium",
    device="cpu",
    compute_type="int8"
)

print("Whisper is ready.")


# ============================================================
# PIPER
# ============================================================

def piper_tts(text, model_path):
    """
    Piper ile WAV oluşturur.
    Sonra WAV içindeki PCM verisini alır.
    22050 Hz -> 44100 Hz dönüştürür.
    """

    temp_wav = tempfile.NamedTemporaryFile(
        suffix=".wav",
        delete=False
    )

    wav_path = temp_wav.name
    temp_wav.close()

    try:
        command = [
            "/opt/homebrew/bin/python3.10",
            "-m",
            "piper",
            "-m",
            model_path,
            "-f",
            wav_path
        ]


        result = subprocess.run(
            command,
            input=text.encode("utf-8"),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        if result.returncode != 0:
            print("PIPER HATASI:")
            print(result.stderr.decode("utf-8", errors="replace"))
            raise RuntimeError("Piper çalışmadı.")


        # WAV dosyasını aç
        with wave.open(wav_path, "rb") as wf:

            channels = wf.getnchannels()
            sample_width = wf.getsampwidth()
            sample_rate = wf.getframerate()
            pcm = wf.readframes(wf.getnframes())

        print(
            f"Piper: {sample_rate} Hz, "
            f"{channels} kanal, "
            f"{sample_width * 8} bit"
        )

        # Mono olduğundan emin ol
        if channels == 2:
            pcm = audioop.tomono(pcm,sample_width,0.5,0.5)

        # 16 bit olduğundan emin ol
        if sample_width != 2:
            raise RuntimeError(
                f"Beklenmeyen sample width: {sample_width}"
            )

        # 22050 -> 44100
        if sample_rate != 44100: pcm, _ = audioop.ratecv(pcm,2,1,sample_rate,44100,None)

        print(f"Send sound to ESP32:"f"{len(pcm)} byte")
        return pcm

    finally:

        if os.path.exists(wav_path):
            os.remove(wav_path)


# ============================================================
# LM STUDIO
# ============================================================

def ask_llm(text, language):
    if language == "en":
        system_prompt = """
    You are Niko, Michael's English-speaking friend.

    The user's name is Michael.

    Speak ONLY English.

    Your main purpose is to have natural, relaxed conversations with Michael in English.

    PERSONALITY:
    - Act like a friendly English-speaking friend, not a teacher.
    - Be natural, relaxed, funny and slightly playful.
    - Talk like a real person having a casual conversation with a friend.
    - Never sound like an English lesson, textbook or language course.
    - Do not constantly teach or explain English.
    - Do not praise Michael for speaking English unless it naturally fits the conversation.
    - Be curious about what Michael says and respond naturally.
    - You can joke and tease Michael occasionally.

    ENGLISH:
    - Help Michael improve his English naturally through conversation.
    - Do not correct every mistake.
    - If Michael makes an obvious mistake that affects the meaning, correct it naturally and briefly.
    - If a sentence sounds unnatural, you can casually show a more natural way to say it.
    - Never stop the conversation just to give an English lesson.
    - Do not explain grammar unless Michael asks.
    - If Michael does not understand a word or expression, explain it briefly.
    - Use normal everyday English.
    - Gradually use slightly more advanced words and expressions when appropriate.
    - Make the conversation feel natural, not educational.

    CONVERSATION:
    - Talk about anything Michael wants: daily life, technology, games, movies,
      music, science, hobbies, funny things, random ideas or interesting facts.
    - React naturally to what Michael says.
    - Ask a question when it naturally keeps the conversation going.
    - Do not ask a question after every answer.
    - Do not turn every conversation into English practice.
    - If Michael wants to simply chat, just chat with him.

    RESPONSE STYLE:
    - Keep answers very short and natural for spoken conversation.
    - Usually answer in one short sentence.
    - Sometimes use two short sentences if necessary.
    - Keep normal answers under 20 words.
    - Answer the exact question and then stop.
    - Do not repeat information.
    - Do not add unnecessary explanations or examples.
    - Never give a long answer unless Michael explicitly asks for a detailed answer.
    - Never use emojis.
    - Never use emoticons.
    - Never use Markdown.
    - Never use bullet points unless absolutely necessary.
    - Never use hashtags.

    CORRECTIONS:
    - Correct Michael naturally, like a friend would.
    - Keep corrections very short.
    - Do not say things like "Here is the correct grammar" or "Let's learn this."
    - If Michael says something slightly wrong, you can simply use the correct form naturally in your reply.
    - Only explicitly point out a mistake when it is useful.

    MATH:
    - Always calculate simple math correctly.
    - If Michael asks an easy math question, you may make one short playful comment.
    - Then clearly state the correct answer.
    - Never give an incorrect mathematical result.
    """

    else:

        system_prompt = """
        Sen Niko'sun.

        Kullanıcının adı Michael.

        SADECE Türkçe konuş.

        Cevaplarını kısa, doğal ve konuşma dilinde ver.
        Çünkü cevabın sesli okunacak.

        KESİN KURALLAR:
        - Asla emoji kullanma.
        - Asla gülen yüz veya benzeri semboller kullanma.
        - Asla Markdown kullanma.
        - Asla madde işareti kullanma.
        - Hashtag kullanma.
        - Gereksiz tırnak işareti kullanma.
        - Uzun açıklamalar yapma.
        - Genellikle bir veya iki kısa cümleyle cevap ver.
        - Cevabını mutlaka tamamla, yarım bırakma.
        - Kullanıcının sorduğu soruya doğrudan cevap ver.
        - Soruyu başka bir soruya dönüştürme.
        - Sayıları ve gerçekleri değiştirme.
        - Komik, zeki ve hafif ukala olabilirsin ama abartma.

        MATEMATİK KURALLARI:
        - Basit matematik işlemlerini her zaman doğru hesapla.
        - Michael çok basit bir matematik sorusu sorarsa bir kısa şakayla takılabilirsin.
        - Ardından doğru sonucu açıkça söyle.
        - Asla yanlış matematik sonucu verme.
        - Örneğin 5 kere 5 kesinlikle 25 eder.
        """

    payload = {
        "model": LM_MODEL,
        "messages": [
            {
                "role": "system",
                "content": system_prompt
            },
            {
                "role": "user",
                "content": text
            }
        ],
        "max_tokens": 50,
        "temperature": 0.45,
        "stream": False
    }

    response = requests.post(LM_STUDIO_URL,json=payload,timeout=120)

    response.raise_for_status()

    data = response.json()

    answer = data["choices"][0]["message"]["content"]

    # Emoji ve özel sembolleri temizle
    answer = re.sub(r"[\U00010000-\U0010ffff]","",answer)

    # Niko'nun cevabını kısa tut
    words = answer.split()

    if len(words) > 25:

        short_answer = ""

        for word in words[:25]:
            short_answer += word + " "

            # Cümle sonu geldiyse burada dur
            if word.endswith((".", "!", "?")):
                break

        answer = short_answer.strip()


    # Markdown işaretlerini temizle
    answer = answer.replace("**", "")
    answer = answer.replace("__", "")
    answer = answer.replace("```", "")
    answer = answer.replace("#", "")

    # Gereksiz boşlukları düzelt
    answer = " ".join(answer.split())

    return answer.strip()


# ============================================================
# ESP32 CLIENT
# ============================================================

async def handle_client(websocket):

    print("\nESP32 connected.")

    language = "tr"

    try:

        async for message in websocket:

            # ------------------------------------------------
            # TEXT MESAJI
            # ------------------------------------------------

            if isinstance(message, str):

                print("ESP32 message:", message)

                # Dil bilgisi
                if message == "LANG:TR":

                    language = "tr"
                    print("Dil: TÜRKÇE")
                    continue

                if message == "LANG:EN":

                    language = "en"
                    print("Language: English")
                    continue

                # ------------------------------------------------
                # NORMAL METİN GELİRSE
                # ------------------------------------------------

                print("Text:", message)

            # ------------------------------------------------
            # BINARY SES
            # ------------------------------------------------

            else:

                audio_data = message

                print(f"\nAudio captured: "f"{len(audio_data)} byte")

                # ------------------------------------------------
                # WHISPER
                # ------------------------------------------------

                # ESP32'den gelen ses:
                # 16 kHz
                # 16 bit
                # mono

                temp_audio = tempfile.NamedTemporaryFile(suffix=".raw",delete=False)

                raw_path = temp_audio.name

                temp_audio.write(audio_data)
                temp_audio.close()

                try:

                    # Whisper raw PCM için WAV oluştur
                    wav_path = raw_path + ".wav"

                    with wave.open(wav_path, "wb") as wf:

                        wf.setnchannels(1)
                        wf.setsampwidth(2)
                        wf.setframerate(16000)
                        wf.writeframes(audio_data)

                    # Whisper
                    segments, info = whisper_model.transcribe(
                        wav_path,
                        language=language,
                        vad_filter=True,
                        beam_size=5
                    )

                    text = ""

                    for segment in segments:
                        text += segment.text

                    text = text.strip()

                    print("Whisper:", text)

                    if not text:

                        print("The speech was not understood..")
                        continue

                    # ------------------------------------------------
                    # LLM
                    # ------------------------------------------------

                    print("Niko thinking...")

                    answer = ask_llm(
                        text,
                        language
                    )

                    print("Niko:", answer)

                    # ------------------------------------------------
                    # OLED'E CEVAP GÖNDER
                    # ------------------------------------------------

                    await websocket.send(answer)

                    # ------------------------------------------------
                    # PIPER
                    # ------------------------------------------------

                    if language == "en":

                        voice_model = PIPER_EN

                    else:

                        voice_model = PIPER_TR

                    print("Audio is being generated...")

                    pcm = piper_tts(
                        answer,
                        voice_model
                    )

                    # ------------------------------------------------
                    # ESP32'YE SES GÖNDER
                    # ------------------------------------------------

                    print("Audio is being sent to the ESP32...")

                    await websocket.send(pcm)

                    print("Ses gönderildi.")

                finally:

                    if os.path.exists(raw_path):
                        os.remove(raw_path)

                    if os.path.exists(raw_path + ".wav"):
                        os.remove(raw_path + ".wav")

    except websockets.exceptions.ConnectionClosed:

        print("The ESP32 connection has closed.")

    except Exception as e:

        print("HATA:", repr(e))


# ============================================================
# SERVER
# ============================================================

async def main():

    print()
    print("====================================")
    print(" Niko AI SERVER")
    print("====================================")
    print(f"WebSocket: ws://0.0.0.0:{PORT}")
    print("Waiting...")
    print()

    async with websockets.serve(
        handle_client,
        HOST,
        PORT,
        max_size=None
    ):

        await asyncio.Future()


if __name__ == "__main__":

    asyncio.run(main())