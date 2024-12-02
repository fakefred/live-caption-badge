import os, datetime, sys
import wave
import argparse
import socket
import json
import requests
import queue

import wave

from vosk import Model, KaldiRecognizer

print("###########Start loading Vosk model###########")
model = Model(lang="en-us")
print("###########Finish loading model###########")


from urllib import parse
from http.server import HTTPServer
from http.server import BaseHTTPRequestHandler
from http.server import ThreadingHTTPServer

PORT = 8000

global audio_queue, speaking, listening
audio_queue = queue.Queue()
speaking = False
listening = False


class Handler(BaseHTTPRequestHandler):
    def _set_headers(self, length):
        self.send_response(200)
        if length > 0:
            self.send_header("Content-length", str(length))
        self.end_headers()

    def _get_chunk_size(self):
        data = self.rfile.read(2)
        while data[-2:] != b"\r\n":
            data += self.rfile.read(1)
        return int(data[:-2], 16)

    def _get_chunk_data(self, chunk_size):
        data = self.rfile.read(chunk_size)
        self.rfile.read(2)
        return data

    def _write_wav(self, data, rates, bits, ch):
        t = datetime.datetime.utcnow()
        time = t.strftime("%Y%m%dT%H%M%SZ")
        filename = str.format("{}_{}_{}_{}.wav", time, rates, bits, ch)

        wavfile = wave.open(filename, "wb")
        wavfile.setparams((ch, int(bits / 8), rates, 0, "NONE", "NONE"))
        wavfile.writeframesraw(bytearray(data))
        wavfile.close()
        return filename

    def do_POST(self):
        global audio_queue, speaking, listening

        speaking = True

        urlparts = parse.urlparse(self.path)

        request_file_path = urlparts.path.strip("/")
        total_bytes = 0
        sample_rates = 0
        bits = 0
        channel = 0
        print("Do Post......")
        if (
            request_file_path == "upload"
            and self.headers.get("Transfer-Encoding", "").lower() == "chunked"
        ):
            rec = KaldiRecognizer(model, 16000)
            rec.SetWords(True)
            rec.SetPartialWords(True)
            data = []
            sample_rates = self.headers.get("x-audio-sample-rates", "").lower()
            bits = self.headers.get("x-audio-bits", "").lower()
            channel = self.headers.get("x-audio-channel", "").lower()
            sample_rates = self.headers.get("x-audio-sample-rates", "").lower()

            print(
                "Audio information, sample rates: {}, bits: {}, channel(s): {}".format(
                    sample_rates, bits, channel
                )
            )
            # https://stackoverflow.com/questions/24500752/how-can-i-read-exactly-one-response-chunk-with-pythons-http-client
            while True:
                chunk_size = self._get_chunk_size()  # 4096
                total_bytes += chunk_size
                #  print("Total bytes received: {}".format(total_bytes))
                #  sys.stdout.write("\033[F") #for live update.
                if chunk_size == 0:
                    break
                else:
                    chunk_data = self._get_chunk_data(chunk_size)
                    data += chunk_data
                    if listening:
                        audio_queue.put(chunk_data)
                    if rec.AcceptWaveform(chunk_data):
                        result = json.loads(rec.Result())["text"]
                        print("Result:", result)
                        if result:
                            pass
                            #  try:
                                #  r = requests.post(
                                    #  f"http://{self.client_address[0]}:80/transcription",
                                    #  data=result,
                                #  )
                            #  except:
                                #  print("POST /transcription failed")
                    else:
                        partialResult = json.loads(rec.PartialResult())["partial"]
            print("____________")
            self.send_response(200)
            self.send_header("Content-type", "text/html;charset=utf-8")
            #  self.send_header("Content-Length", len(partialResult))
            self.end_headers()
            self.wfile.write(partialResult.encode("utf-8"))
            self._write_wav(data, 16000, 16, 1)

            speaking = False

    def do_GET(self):
        global audio_queue, speaking, listening

        listening = True

        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()

        while speaking:
            chunk = audio_queue.get()
            print(f"Chunk size: {len(chunk)}, queue length: {audio_queue.qsize()}")
            size = f"{len(chunk):X}".encode("utf-8")
            buf = size + b"\r\n" + chunk + b"\r\n"
            self.wfile.write(buf)
        
        self.wfile.write(b"0\r\n\r\n")

        listening = False


def get_host_ip():
    # https://www.cnblogs.com/z-x-y/p/9529930.html
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    finally:
        s.close()
    return ip


parser = argparse.ArgumentParser(
    description="HTTP Server save pipeline_raw_http example voice data to wav file"
)
parser.add_argument("--ip", "-i", nargs="?", type=str)
parser.add_argument("--port", "-p", nargs="?", type=int)
args = parser.parse_args()
if not args.ip:
    args.ip = get_host_ip()
if not args.port:
    args.port = PORT

httpd = ThreadingHTTPServer((args.ip, args.port), Handler)

print("Serving HTTP on {} port {}".format(args.ip, args.port))
httpd.serve_forever()
