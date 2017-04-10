#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

from telegram.ext import Updater, CommandHandler, MessageHandler, Filters, CallbackQueryHandler
from telegram import InlineKeyboardButton, InlineKeyboardMarkup
import pdb, re, serial

updater = Updater(token='317355728:AAG02xTWXrL_Qr5QFE2SR22APSnMZn0RSew')  # @SEDIrrigationBot
dispatcher = updater.dispatcher

#arduino = serial.Serial('COM7', 9600)

# ------------------------------------------------------------------------------------

SERIAL_DELIM = ";"

SPANISH_DIC = {
	"unknown_cmd": "Comando no reconocido",
	"start_cond_success": "Condicion establecida",
	"comm_error": "Condicion establecida",
	"line": "Línea"
}

DIC = SPANISH_DIC

# ------------------------------------------------------------------------------------

class Condition:
	TOKEN_TEMP = "TEMP"
	TOKEN_HUM = "HUM"
	
	OPERATORS = {"none": 0, "<": 1, ">": 2, "&&": 3, "||": 4}

	def __init__(self, temp_op, temp_thr, mid_op, hum_op, hum_thr):
		self.temp_op = temp_op
		self.temp_thr = temp_thr
		self.hum_op = hum_op
		self.hum_thr = hum_thr
		self.mid_op = mid_op
	
	@classmethod
	def parse(cls, text):
		temp_op, hum_op = cls.OPERATORS["none"], cls.OPERATORS["none"]
		temp_thr, hum_thr = 0, 0
		
		part_patt = r"(TEMP|HUM)\s*(<|>)\s*([0-9.]+)"
		full_patt = r"^\s*%s\s*((&&|\|\|)\s*%s)?\s*$" % (part_patt, part_patt)
		match = re.match(full_patt, text)
		
		if match:
			token1, op1, value1, _, mid_op, token2, op2, value2 = match.groups()
			print(match.groups())
			if token1 == token2:
				raise Exception()
				
			if token1 == cls.TOKEN_TEMP:
				temp_op, temp_thr = cls.OPERATORS[op1], float(value1)
				if token2 is not None:
					hum_op, hum_thr = cls.OPERATORS[op2], float(value2)
			else:
				hum_op, hum_thr = cls.OPERATORS[op1], float(value1)
				if token2 is not None:
					temp_op, temp_thr = cls.OPERATORS[op2], float(value2)
		else:
			raise Exception()
		#pdb.set_trace()
		mid_op = cls.OPERATORS[mid_op] if mid_op is not None else cls.OPERATORS["none"]
		return cls(temp_op, temp_thr, mid_op, hum_op, hum_thr)
		
	def __str__(self):
		inv_op = {v: k for k, v in self.OPERATORS.iteritems()}
		print(inv_op)
		print(self.temp_op, self.mid_op)
		temp = "TEMP %s %.2f" % (inv_op[self.temp_op], self.temp_thr) if self.temp_op != Condition.OPERATORS["none"] else None
		hum = "HUM %s %.2f" % (inv_op[self.hum_op], self.hum_thr) if self.hum_op != Condition.OPERATORS["none"] else None
		
		if self.mid_op != Condition.OPERATORS["none"]:
			return "%s %s %s" % (temp, inv_op[self.mid_op], hum)
		else:
			return temp if temp is not None else hum

class IrrigationSystem:

	class IrrigationLine:
		def __init__(self, start_cond, stop_cond):
			self.start_cond = start_cond
			self.stop_cond = stop_cond
	
	def __init__(self, port):
		self.serial = serial.Serial(port, 9600)
		self.lines = {}

	def __str__(self):
		pass
		
	def start_condition(self, num_line, start_cond):
		if num_line in self.lines:
			self.lines[num_line].start_cond = start_cond
		else:
			self.lines[num_line] = IrrigationLine(start_cond, start_cond)
		
	def stop_condition(self, num_line, stop_cond):
		if num_line in self.lines:
			self.lines[num_line].stop_cond = stop_cond
		else:
			self.lines[num_line] = IrrigationLine(stop_cond, stop_cond)
		
# ------------------------------------------------------------------------------------

message = None
irrigation = IrrigationSystem("COM7");

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
	update.message.reply_text(DIC["unknown_cmd"])

	
# Set start condition for a line
def set_start_condition(bot, update):
	print("\nReceived (start): %s" % update.message.text)
	
	try:
		num_line, temp_op, temp_thr, hum_op, hum_thr = parse_condition(update.message.text)
		irrigation.start_condition(num_line, temp_op, temp_thr, hum_op, hum_thr)
		cmd = "I;%d;%d;%.2f;0;0" % num_line, temp_op, temp_thr, hum_op, hum_thr
		print(cmd)
		
		irrigation.serial.write(cmd)
		update.message.reply_text(DIC["start_cond_success"])
	except Exception as e:
		update.message.reply_text(DIC["comm_error"])
		print(e)
	
	
# Set stop condition for a line
def set_stop_condition(bot, update):
	print("\nReceived (stop): %s" % update.message.text)
	
	try:
		cmd = "S;%d;%d;%.2f;0;0" % parse_condition(update.message.text)
		print(cmd)
		
		irrigation.serial.write(cmd)
		update.message.reply_text(DIC["start_cond_success"])
	except Exception as e:
		update.message.reply_text(DIC["comm_error"])
		print(e)
	
# Request sensor updates (for all the lines)
def get_sensor_updates(bot, update):
	global message
	message = update.message
	
	try:
		irrigation.serial.write("U") 
	except Exception as e:
		update.message.reply_text(DIC["comm_error"])
		print(e)

# ------------------------------------------------------------------------------------
		
if __name__ == "__main__":
	# Handlers
	dispatcher.add_handler(CommandHandler('start', start))
	dispatcher.add_handler(CommandHandler('startCondition', set_start_condition))
	dispatcher.add_handler(CommandHandler('stopCondition', set_stop_condition))
	dispatcher.add_handler(CommandHandler('updates', get_sensor_updates))
	dispatcher.add_handler(MessageHandler(Filters.text, text))
	dispatcher.add_handler(MessageHandler(Filters.command, unknown))

	updater.start_polling()  # Start the bot


	# Loop to read arduino data via serial
	while 1:
		try:
			input = irrigation.serial.readline().replace("\r\n", "")
			print(input)
			
			if input.startswith("U;"):  # Update
				updates = input.split(SERIAL_DELIM)[1:]
				print(updates)
				pairs = zip(*[updates[i::3] for i in range(3)]) 
				
				updatesResp = ""
				for line, temp, hum in pairs:
					print(line, temp, hum)
					updatesResp += DIC["line"] + " %s : %.2f C, %.2f\n" % (line, float(temp), float(hum))
					
				if updatesResp != "" and message is not None:
					message.reply_text(updatesResp[:-1])
					message = None
				
		except serial.SerialTimeoutException:
			print('Data could not be read')
		except Exception as e:
			print(e)