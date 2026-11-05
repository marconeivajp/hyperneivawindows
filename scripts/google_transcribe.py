#!/usr/bin/env python
import sys
import speech_recognition as sr

if len(sys.argv) < 2:
    sys.exit(1)
path = sys.argv[1]
recognizer = sr.Recognizer()
with sr.AudioFile(path) as source:
    audio = recognizer.record(source)
try:
    text = recognizer.recognize_google(audio, language='pt-BR')
    print(text)
except sr.UnknownValueError:
    print('')
except sr.RequestError as e:
    print('')
