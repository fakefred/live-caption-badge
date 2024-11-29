from flask import Flask, request
from vosk import Model, KaldiRecognizer
import json
import sounddevice as sd

import wave
import datetime
import json

app = Flask(__name__)
model = Model(lang="en-us")
#device_info = sd.query_devices(None, "input")
#sample_rate = int(device_info["default_samplerate"])
#rec = KaldiRecognizer(model, sample_rate)

# Helper function to save audio data to a .wav file
def save_audio(data, sample_rate, bits, channels):
    timestamp = datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%SZ')
    filename = f"{timestamp}_{sample_rate}_{bits}_{channels}.wav"
    with wave.open(filename, 'wb') as wavfile:
        wavfile.setparams((channels, bits // 8, sample_rate, 0, 'NONE', 'NONE'))
        wavfile.writeframes(data)
    return filename


@app.route('/upload', methods=['POST'])
def stream_audio():

    # Fetch audio metadata from headers
    requested_sample_rate = int(request.headers.get('x-audio-sample-rates',16000))
    bits = int(request.headers.get('x-audio-bits', 16))
    channels = int(request.headers.get('x-audio-channel', 1))

    data = request.data  # This is the raw audio data
    # Process the data as needed

    # Save the audio data to a .wav file
    filename = save_audio(data, requested_sample_rate, bits, channels)
    print(f"Saved audio file: {filename}")
    wf = wave.open(filename, "rb")

    rec = KaldiRecognizer(model, wf.getframerate())
    rec.SetWords(True)
    rec.SetPartialWords(True)

    while True:
        data = wf.readframes(4000)
        print(len(data))
        if len(data) == 0:
            break
        if rec.AcceptWaveform(data):
            print(rec.Result())
        else:
            print(rec.PartialResult())
    finalResult = json.loads(rec.FinalResult())
    print("result: ", finalResult['text'])
    print("Received audio data")
    return finalResult['text'], 200 

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)
