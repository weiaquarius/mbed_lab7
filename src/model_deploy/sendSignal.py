import numpy as np
import serial
import time

serdev = '/dev/ttyACM0'
s = serial.Serial(serdev)

waitTime = 0.1

# read which song should py play 
song_num = s.readline()
SONG = int(song_num)
print("SONG = %d" %(SONG))
# generate the signal table
if SONG == 0:   
    #TWINKLE
    signalLength = 48
    signalTable = [261, 261, 392, 392, 440, 440, 392, 392, \
                   349, 349, 330, 330, 294, 294, 261, 261, \
                   392, 392, 349, 349, 330, 330, 294, 294, \
                   392, 392, 349, 349, 330, 330, 294, 294, \
                   261, 261, 392, 392, 440, 440, 392, 392, \
                   349, 349, 330, 330, 294, 294, 261, 261]
elif SONG == 1:
    # BIRTHDAY
    signalLength = 28
    signalTable = [261, 261, 294, 261, 330, 294, 294, \
                   261, 261, 294, 261, 349, 330, 330, \
                   261, 261, 392, 349, 330, 294, 261, \
                   330, 330, 294, 261, 330, 294, 294]


# output formatter
formatter = lambda x: "%.3f" % x

# send the waveform table to K66F

print("Sending signal ...")
print("It may take about %d seconds ..." % (int(signalLength * waitTime)))
for data in signalTable:
    s.write(bytes(formatter(data), 'UTF-8'))
    time.sleep(waitTime)
s.close()

print("Signal sended")