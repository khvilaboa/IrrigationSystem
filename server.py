#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

from telegram.ext import Updater, CommandHandler, MessageHandler, Filters, CallbackQueryHandler
from telegram import InlineKeyboardButton, InlineKeyboardMarkup
import serial

updater = Updater(token='317355728:AAG02xTWXrL_Qr5QFE2SR22APSnMZn0RSew')  # @SEDIrrigationBot
dispatcher = updater.dispatcher

arduino = serial.Serial('COM7', 9600)

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
def set_start(bot, update):
	print("\nReceived (start): %s" % update.message.text)
	
	try:
		cmd = "I;%d;%d;%.2f;0;0" % parse_condition(update.message.text)
		print(cmd)
		
		arduino.write(cmd)
	except Exception as e:
		print(e)
	
	
# Set stop condition for a line
def set_stop(bot, update):
	print("\nReceived (stop): %s" % update.message.text)
	
	try:
		cmd = "S;%d;%d;%.2f;0;0" % parse_condition(update.message.text)
		print(cmd)
		
		arduino.write(cmd)
	except Exception as e:
		print(e)
	
	
# Handlers
dispatcher.add_handler(CommandHandler('start', start))
dispatcher.add_handler(CommandHandler('setStartCondition', set_start))
dispatcher.add_handler(CommandHandler('setStopCondition', set_stop))
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
		
	return int(num_line), temp_op, float(temp_value)
		
	print("Condition: " + condition)


# ------------------------------------------------------------------------------------

# Loop to read arduino data via serial
while 1:
	try:
		print(arduino.readline())
	except serial.SerialTimeoutException:
		print('Data could not be read')