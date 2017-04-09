#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

from telegram.ext import Updater, CommandHandler, MessageHandler, Filters, CallbackQueryHandler
from telegram import InlineKeyboardButton, InlineKeyboardMarkup
import serial

updater = Updater(token='317355728:AAG02xTWXrL_Qr5QFE2SR22APSnMZn0RSew')  # @SEDIrrigationBot
dispatcher = updater.dispatcher

arduino = serial.Serial('COM7', 9600)

SERIAL_DELIM = ";"

OP_NOTHING = 0
OP_LESS = 1
OP_GREATER = 2

# ------------------------------------------------------------------------------------

# Default command (executed on bot init)
def start(bot, update):
	update.message.reply_text("Hola :)")

# To handle text (that doesn't start with '/')
def text(bot, update):
	text = update.message.text
	print("\nReceived: %s" % text)
	update.message.reply_text(text)

# To handle unknown commands
def unknown(bot, update):
	#print update  # Uncomment to view message info
	update.message.reply_text("Comando no reconocido.")

	
# Set start condition for a line
def set_start_condition(bot, update):
	print("\nReceived (start): %s" % update.message.text)
	
	try:
		cmd = "I;%d;%d;%.2f;0;0" % parse_condition(update.message.text)
		print(cmd)
		
		arduino.write(cmd)
	except Exception as e:
		print(e)
	
	
# Set stop condition for a line
def set_stop_condition(bot, update):
	print("\nReceived (stop): %s" % update.message.text)
	
	try:
		cmd = "S;%d;%d;%.2f;0;0" % parse_condition(update.message.text)
		print(cmd)
		
		arduino.write(cmd)
	except Exception as e:
		print(e)
	
# Request sensor updates (for all the lines)
def get_sensor_updates(bot, update):
	try:
		arduino.write("U") 
	except Exception as e:
		print(e)
	
# Handlers
dispatcher.add_handler(CommandHandler('start', start))
dispatcher.add_handler(CommandHandler('startCondition', set_start_condition))
dispatcher.add_handler(CommandHandler('stopCondition', set_stop_condition))
dispatcher.add_handler(CommandHandler('updates', get_sensor_updates))
dispatcher.add_handler(MessageHandler(Filters.text, text))
dispatcher.add_handler(MessageHandler(Filters.command, unknown))

updater.start_polling()  # Starts the bot

# ------------------------------------------------------------------------------------

def parse_condition(text):
	textSp = text.split()
	if len(textSp) < 3:
		raise Exception()
		
	num_line = textSp[1]
	condition = " ".join(textSp[2:]).replace(" ", "")
	
	if("<" in condition):
		temp_op = OP_LESS
		_, temp_value = condition.split("<")
	elif(">" in condition):
		temp_op = OP_GREATER
		_, temp_value = condition.split(">")
	else:
		raise Exception()
	
	print("Condition: " + condition)
	return int(num_line), temp_op, float(temp_value)

# ------------------------------------------------------------------------------------

# Loop to read arduino data via serial
while 1:
	try:
		input = arduino.readline().replace("\r\n", "")
		print(input)
		
		if input.startswith("U;"):  # Update
			updates = input.split(SERIAL_DELIM)[1:]
			print(updates)
			pairs = zip(*[updates[i::3] for i in range(3)]) 
			
			for line, temp, hum in pairs:
				print(line, temp, hum)
			
	except serial.SerialTimeoutException:
		print('Data could not be read')