#!/usr/bin/python3
# _*_ coding: utf-8 _*_
#coding: utf-8

import os
import time
from datetime import datetime, timedelta
import RPi.GPIO as GPIO

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)

Powerbutton = 24
GPIO.setup(Powerbutton, GPIO.IN, pull_up_down = GPIO.PUD_UP)

def loop():																		# Handling the manual shutdown
	
	while(1):
		if GPIO.input(Powerbutton) == False:									# If someone has been pushed it...
			os.system("sudo shutdown now &")									# I shuts down
		print("Polling the power button..")
		time.sleep(0.5)															# Reduce CPU usage
	
def setwakeup():
	
	Wakeuptime = "06:28"
			
	Datum=datetime.strftime(datetime.now(),"%Y-%m-%d")							# Getting the actual day in string format.
	Actualday = datetime.strftime(datetime.now(),"%w")							# Checking wether it is friday
	if (Actualday=='5'):														# If yes,
		Nextstartday = datetime.strptime(Datum,"%Y-%m-%d") + timedelta(days=3) 	# Skipping the wekkend days, and set it for Monday
	else:
		Nextstartday = datetime.strptime(Datum,"%Y-%m-%d") + timedelta(days=1) 	# If no set it for the next day
	Stringnextstartday = datetime.strftime(Nextstartday,"%m/%d/%Y")				# Converting from date to string fromat
	Wakeuptime=Wakeuptime+":00"
	Stringtorun="sudo rtcctl set alarm1"+" \""+Stringnextstartday+" "+Wakeuptime+"\""
	os.system(Stringtorun)
	Stringtorun="sudo rtcctl set alarm2"+" \""+Stringnextstartday+" "+Wakeuptime+"\"" # It's better to set up the alarm2 is regardless it's not currently use, because after replacing the battery it contains uninitalized data that bothers the alarm1 too.
	os.system(Stringtorun)
	os.system("sudo rtcctl on alarm1")
	print("The next weekup has been set!")

setwakeup()
loop()
