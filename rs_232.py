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

HOST = '127.0.0.1'  
PORT = 10500 
MAX_CLIENTS = 100 


s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
fd = serial.Serial(
                   port = file_rs232,
                   baudrate = 57600,
                   )
try: 

    fd.open()
    s.bind((HOST, PORT))
    s.listen(MAX_CLIENTS)
    
except Exception, e:

    print "Error init port: " + str(e)

    exit()
    
 
#------------------------RS232-------------------------------------------------------------------        
        
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
                answer_status = get_status(word_1)
                parse_status(answer_status)
                answer_word2 = get_status(get_status(word_2))
                answer_word3 = get_status(get_status(word_3))  
                   
        except Exception, e:
            print "exit from thread", e
            exit()
                    

thread_sdk = threading.Thread(target = run, args = ())
thread_sdk.start()



        

#------------------------socket server-------------------------------------------------------------------       


def clientthread(conn):
    while True:   
        data = conn.recv(1024)
        #reply = 'OK...' + data
        if not data:
            break
        
        if str(data) == 'get_hw':
            answer = str(params_sdk['hw'])
        elif str(data) == 'get_sw':
            answer = str(params_sdk['sw'])
        elif str(data) == 'get_self_temp':
            answer = str(params_sdk['self_temp'])
        elif str(data) == 'get_relay':
            answer = str(params_sdk['relay'])
        elif str(data) == 'get_optical_relay':
            answer = str(params_sdk['optical_relay'])
        elif str(data) == 'get_dry_contact':
            answer = str(params_sdk['dry_contact'])
            
        else:
            answer = str(('get_hw', 'get_sw', 'get_self_temp', 'get_relay', 'get_optical_relay', 'get_dry_contact' ))    
            
        conn.send(answer)
    conn.close()
 

while 1:
    conn, addr = s.accept()
    thread_socet = threading.Thread(target = clientthread ,args = (conn,))
    thread_socet.start()
 
s.close()

