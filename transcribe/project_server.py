from flask import Flask, request
from vosk import Model, KaldiRecognizer
import json
import sounddevice as sd

import wave
import datetime
import json

app = Flask(__name__)
model = Model(lang="en-us")
device_info = sd.query_devices(None, "input")
sample_rate = int(device_info["default_samplerate"])
rec = KaldiRecognizer(model, sample_rate)

# Helper function to save audio data to a .wav file
def save_audio(data, sample_rate, bits, channels):
    timestamp = datetime.datetime.utcnow().strftime('%Y%m%dT%H%M%SZ')
    filename = f"{timestamp}_{sample_rate}_{bits}_{channels}.wav"
    with wave.open(filename, 'wb') as wavfile:
        wavfile.setparams((channels, bits // 8, sample_rate, 0, 'NONE', 'NONE'))
        wavfile.writeframes(data)
    return filename


@app.route('/stream', methods=['POST'])
def stream_audio():

    # Fetch audio metadata from headers
    requested_sample_rate = int(request.headers.get('x-audio-sample-rates', sample_rate))
    bits = int(request.headers.get('x-audio-bits', 16))
    channels = int(request.headers.get('x-audio-channel', 1))

    data = request.data  # This is the raw audio data
    # Process the data as needed

    # Save the audio data to a .wav file
    filename = save_audio(data, requested_sample_rate, bits, channels)
    print(f"Saved audio file: {filename}")
    wf = wave.open(filename, "rb")
    data = wf.readframes(4000) # may need to change 4000
    if rec.AcceptWaveform(data):
        print(rec.Result())
    else:
        print(rec.PartialResult())
    print("Received audio data")
    return "Audio received", 200

if __name__ == "__main__":
    app.run(host="192.168.0.199", port=8080)