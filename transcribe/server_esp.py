import os, datetime, sys
import wave
import argparse
import socket
import json

import sounddevice as sd
import wave

from vosk import Model, KaldiRecognizer

global textBuffer, index
textBuffer = []
index = 0

print("###########Start loading Vosk model###########")
model = Model(lang="en-us")
print("###########Finish loading model###########")

if sys.version_info.major == 3:
    # Python3
    from urllib import parse
    from http.server import HTTPServer
    from http.server import BaseHTTPRequestHandler
else:
    # Python2
    import urlparse
    from BaseHTTPServer import HTTPServer
    from BaseHTTPServer import BaseHTTPRequestHandler

PORT = 8000

class Handler(BaseHTTPRequestHandler):
    def _set_headers(self, length):
        self.send_response(200)
        if length > 0:
            self.send_header('Content-length', str(length))
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
        time = t.strftime('%Y%m%dT%H%M%SZ')
        filename = str.format('{}_{}_{}_{}.wav', time, rates, bits, ch)

        wavfile = wave.open(filename, 'wb')
        wavfile.setparams((ch, int(bits/8), rates, 0, 'NONE', 'NONE'))
        wavfile.writeframesraw(bytearray(data))
        wavfile.close()
        return filename

    def do_POST(self):
        global textBuffer, index
        if sys.version_info.major == 3:
            urlparts = parse.urlparse(self.path)
        else:
            urlparts = urlparse.urlparse(self.path)
        request_file_path = urlparts.path.strip('/')
        total_bytes = 0
        sample_rates = 0
        bits = 0
        channel = 0
        print("Do Post......")
        if (request_file_path == 'upload'
            and self.headers.get('Transfer-Encoding', '').lower() == 'chunked'):
            rec = KaldiRecognizer(model, 16000)
            rec.SetWords(True)
            rec.SetPartialWords(True)
            data = []
            sample_rates = self.headers.get('x-audio-sample-rates', '').lower()
            bits = self.headers.get('x-audio-bits', '').lower()
            channel = self.headers.get('x-audio-channel', '').lower()
            sample_rates = self.headers.get('x-audio-sample-rates', '').lower()

            print("Audio information, sample rates: {}, bits: {}, channel(s): {}".format(sample_rates, bits, channel))
            # https://stackoverflow.com/questions/24500752/how-can-i-read-exactly-one-response-chunk-with-pythons-http-client
            while True:
                chunk_size = self._get_chunk_size() #4096
                total_bytes += chunk_size
                print("Total bytes received: {}".format(total_bytes))
                sys.stdout.write("\033[F") #for live update.
                if (chunk_size == 0):
                    break
                else:
                    chunk_data = self._get_chunk_data(chunk_size)
                    #data += chunk_data
                    if rec.AcceptWaveform(chunk_data):
                        #print(rec.Result())
                        pass
                    else:
                        #print(rec.PartialResult())
                        partialResult = json.loads(rec.PartialResult())["partial"]
                        '''
                        tmp = partialResult.split()
                        print(tmp)
                        if not textBuffer:
                            textBuffer = tmp
                        else:
                            comp = textBuffer[-1*len(tmp):]

                            for i in range(min(len(comp), len(tmp))):
                                if comp[i] != tmp[i]:
                                    textBuffer.extend(tmp[i:])
                                    break
                            textBuffer.extend(tmp[i:])
                        '''
                        currLen = len(textBuffer)
                        textBuffer.extend(partialResult.split()[currLen:])
            print("____________")
            print(len(textBuffer))
            for i in range(len(textBuffer)):
                print(textBuffer[i])
            self.send_response(200)
            self.send_header("Content-type", "text/html;charset=utf-8")
            #self.send_header("Content-Length", str(total_bytes))
            self.send_header("Content-Length", len(partialResult))
            self.end_headers()
            #body = 'File {} was written, size {}'.format(filename, total_bytes)
            body = partialResult
            self.wfile.write(body.encode('utf-8'))

    def do_GET(self):
        global textBuffer, index
        print("Do GET")
        if not textBuffer:
            body = "empty"
        else:
            if index < len(textBuffer):
                body = textBuffer[index]
                index += 1
            else:
                body = textBuffer[-1]
        self.send_response(200)
        self.send_header("Content-type", "text/html;charset=utf-8")
        self.send_header("Content-Length", len(body))
        self.end_headers()
        self.wfile.write(body.encode('utf-8'))

def get_host_ip():
    # https://www.cnblogs.com/z-x-y/p/9529930.html
    try:
        s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        s.connect(('8.8.8.8',80))
        ip=s.getsockname()[0]
    finally:
        s.close()
    return ip

parser = argparse.ArgumentParser(description='HTTP Server save pipeline_raw_http example voice data to wav file')
parser.add_argument('--ip', '-i', nargs='?', type = str)
parser.add_argument('--port', '-p', nargs='?', type = int)
args = parser.parse_args()
if not args.ip:
    args.ip = get_host_ip()
if not args.port:
    args.port = PORT

httpd = HTTPServer((args.ip, args.port), Handler)

print("Serving HTTP on {} port {}".format(args.ip, args.port));
httpd.serve_forever()

