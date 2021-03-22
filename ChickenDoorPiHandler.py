#!/usr/bin/env python3

from smbus import SMBus
from time import sleep

# I2C channel 1 is connected to the GPIO pins
channel = 1

address = 0x08

# Initialize I2C (SMBus)
bus = SMBus(channel)

def StringToBytes(val: str):
    retVal = []
    for c in val:
            retVal.append(ord(c))
    return retVal

def writeData(data):
    bus.write_byte(address, data)

def readData():
    return bus.read_byte_data(address, 1)

def setup():
    pass

def loop():
    try:
        x = bus.read_byte(address)
        if (x == 1):
            print(x)
            bus.write_byte(address, 1)
            sleep(0.2)
            data_received_from_Arduino = bus.read_i2c_block_data(address,  0,12)
            print(data_received_from_Arduino)
            bus.write_byte(address, 3)
            print(StringToBytes("time"))
            bus.write_i2c_block_data(address, 0x00, StringToBytes("time"))

if __name__ == "__main__":
    setup()

    while True:
        loop()