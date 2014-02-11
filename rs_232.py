#!/usr/bin/env python
# -*- coding: utf-8 -*- 

'''
Created on Feb 10, 2014

@author: nemirov
'''

import socket
import sys
import threading
import string
import serial
import time

file_rs232 = '/dev/ttyUSB0'
params_sdk = {'hw': 0, 'sw': 0, 'self_temp': 0, 'relay': 0, 'optical_relay': (), 'dry_contact': ()}

fd = serial.Serial(
                   port = file_rs232,
                   baudrate = 57600,
                   )
try: 

    fd.open()

except Exception, e:

    print "Error open serial port: " + str(e)

    exit()
    
 
        
        
def parse_status(value):
    params_sdk['hw'] = int(value[6:8], 16)
    params_sdk['sw'] = int(value[8:10], 16) 
    params_sdk['self_temp'] = int(value[40:42], 16) 
    params_sdk['relay'] = int(value[19:20])
    params_sdk['optical_relay'] = tuple("{0:4b}".format(int(value[15:16],16)))
    params_sdk['dry_contact'] = tuple(value[20:40])
            
       
def disconnect_rs232():
    fd.close()                         
                                    

def set_value(param):
    fd.write(request_status) 
        
        
def get_status(request_status):
    time.sleep(0.7)
    fd.write(request_status)
    time.sleep(0.15)
    while fd.inWaiting() > 0:
        out = fd.read(fd.inWaiting())
        if out != '':
            status = out
        else:
            status = 'Null'
                
    return status
        
        
def run():
    if fd.isOpen():
        
        try:
            word_1 = 'TSC10173\r\n'
            word_2 = 'TSC11576\r\n'
            word_3 = 'TSC1C105000000000005\r\n' 
        
            while 1:
                print "request"
                answer_status = get_status(word_1)
                parse_status(answer_status)
            
                answer_word2 = get_status(get_status(word_2))
                answer_word3 = get_status(get_status(word_3))  
                   
        except Exception, e:
            print "exit from thread", e
            exit()
                    

thread_sdk = threading.Thread(target = run, args = ())
thread_sdk.start()


if thread_sdk.isAlive():
    time.sleep(3)
    for key, value in params_sdk.items():
        print key + " : " + str(value)



