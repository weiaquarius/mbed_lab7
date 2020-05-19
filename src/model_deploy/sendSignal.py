import numpy as np
import serial
import time

serdev = '/dev/ttyACM0'
s = serial.Serial(serdev)

waitTime = 0.1
signalLength = 48*3
# generate the signal table
#TWINKLE
twinkle_signalTable = np.array([261, 261, 392, 392, 440, 440, 392, 392, \
                   349, 349, 330, 330, 294, 294, 261, 261, \
                   392, 392, 349, 349, 330, 330, 294, 294, \
                   392, 392, 349, 349, 330, 330, 294, 294, \
                   261, 261, 392, 392, 440, 440, 392, 392, \
                   349, 349, 330, 330, 294, 294, 261, 261])

# BIRTHDAY
birthday_signalTable = np.array([261, 261, 294, 261, 330, 294, 294, 294, \
                   261, 261, 294, 261, 349, 330, 330, 330, \
                   261, 261, 392, 349, 330, 294, 261, 261, \
                   330, 330, 294, 261, 330, 294, 294, 294, \
                   0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  , \
                   0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ])

# TWO TIGER
tiger_signalTable = np.array([261, 294, 330, 261, 261, 294, 330, 261, \
                   330, 349, 392, 392, 330, 349, 392, 392, \
                   392, 440, 392, 349, 330, 261, 261, 261, \
                   392, 440, 392, 349, 330, 261, 261, 261, \
                   261, 261, 191, 191, 191, 191, 261, 261, \
                   261, 261, 191, 191, 191, 191, 261, 261])


'''
do 261
re 294
mi 330
fa 349
so 392
la 440 
'''
signalTable = np.append(twinkle_signalTable, birthday_signalTable)
signalTable = np.append(signalTable, tiger_signalTable)
signalTable = signalTable/1000
formatter = lambda x: "%.3f" % x

# send the waveform table to K66F

print("Sending signal ...")
print("It may take about %d seconds ..." % (int(signalLength * waitTime)))
for data in signalTable:
    s.write(bytes(formatter(data), 'UTF-8'))
    time.sleep(waitTime)
s.close()

print("Signal sended")