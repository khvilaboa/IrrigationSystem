#!/usr/bin/env python
# -*- coding: iso-8859-1 -*-

from telegram.ext import Updater, CommandHandler, MessageHandler, Filters, CallbackQueryHandler
from telegram import InlineKeyboardButton, InlineKeyboardMarkup
import pdb, re, serial, time

updater = Updater(token='317355728:AAG02xTWXrL_Qr5QFE2SR22APSnMZn0RSew')  # @SEDIrrigationBot
dispatcher = updater.dispatcher

#arduino = serial.Serial('COM7', 9600)

# ------------------------------------------------------------------------------------

SERIAL_DELIM = ";"

SPANISH_DIC = {
	"unknown_cmd": "Comando no reconocido",
	"cond_success": "Condición establecida",
	"comm_error": "No se pudo establecer la conexión",
	"line": "Línea",
	"user_not_allowed": "No estás autorizado a interactuar con el sistema",
	"allowed_users": "Usuarios autorizados:",
	"user_added": "Usuario añadido correctamente",
	"user_removed": "Usuario eliminado correctamente",
	"user_already_added": "El usuario especificado ya tiene privilegios",
	"user_unknown": "El usuario especificado no está en la lista de usuarios con privilegios"
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
			
	def encode(self, delimiter = ";"):
		return "%d%s%.2f%s%d%s%d%s%.2f" % (self.temp_op, delimiter, self.temp_thr, delimiter, \
											 self.mid_op, delimiter, self.hum_op, delimiter, self.hum_thr)

class IrrigationSystem:

	CMD_SET_INIT = 'I';
	CMD_SET_STOP = 'S';
	CMD_REQUEST_COMMANDS = 'R';
	CMD_UPDATE = 'U';
	CMD_ENABLE_GREENHOUSE = 'G';
	CMD_DISABLE_GREENHOUSE = 'H';

	class IrrigationLine:
		def __init__(self, start_cond, stop_cond):
			self.start_cond = start_cond
			self.stop_cond = stop_cond
	
	def __init__(self, port):
		self.serial = serial.Serial(port, 9600)
		self.lines = {}

	def __str__(self):
		pass
		
	def start_condition(self, num_line, start_cond, delimiter = ";"):
		print(start_cond.encode())
		if num_line in self.lines:
			self.lines[num_line].start_cond = start_cond
		else:
			self.lines[num_line] = self.IrrigationLine(start_cond, start_cond)
			
		cmd = "%s%s%d%s%s" % (IrrigationSystem.CMD_SET_INIT, delimiter, num_line, delimiter, start_cond.encode(delimiter))
		print(cmd)
		self.serial.write(cmd)
		
	def stop_condition(self, num_line, stop_cond, delimiter = ";"):
		if num_line in self.lines:
			self.lines[num_line].stop_cond = stop_cond
		else:
			self.lines[num_line] = self.IrrigationLine(stop_cond, stop_cond)
			
		cmd = "%s%s%d%s%s" % (IrrigationSystem.CMD_SET_STOP, delimiter, num_line, delimiter, stop_cond.encode(delimiter))
		print(cmd)
		self.serial.write(cmd)
		
	def conditions(self):
		res = ""
		for num_line in self.lines:
			res += "Line %s:\n- Start: %s\n- Stop: %s\n\n" % (num_line, self.lines[num_line].start_cond, self.lines[num_line].stop_cond)
		return res
		
	def request_lines(self):
		self.serial.write(IrrigationSystem.CMD_REQUEST_COMMANDS)
		
	def update(self):
		self.serial.write(IrrigationSystem.CMD_UPDATE)
		
	def update_line(self, num_line, start_cond, cond):
		print(num_line, start_cond, cond)
		#pdb.set_trace()
		if num_line in self.lines:
			if start_cond:
				self.lines[num_line].start_cond = cond
			else:
				self.lines[num_line].stop_cond = cond
		else:
			if start_cond:
				self.lines[num_line] = self.IrrigationLine(cond, cond)
			else:
				self.lines[num_line] = self.IrrigationLine(cond, cond)
				
	def enable_greenhouse(self, num_line):
		self.serial.write("%s;%d" % (IrrigationSystem.CMD_ENABLE_GREENHOUSE, num_line))
		
	def disable_greenhouse(self, num_line):
		self.serial.write("%s;%d" % (IrrigationSystem.CMD_DISABLE_GREENHOUSE, num_line))
		
# ------------------------------------------------------------------------------------

message = None
irrigation = IrrigationSystem("COM7");

allowed_users = ["khvilaboa"]

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
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	print("\nReceived (start): %s" % update.message.text)
	
	try:
		textSp = update.message.text.split()
		num_line = int(textSp[1])
		cond_text = " ".join(textSp[2:])
		
		start_cond = Condition.parse(cond_text)
		irrigation.start_condition(num_line, start_cond)
		#cmd = "I;%d;%d;%.2f;0;0" % num_line, temp_op, temp_thr, hum_op, hum_thr
		#print(cmd)
		#irrigation.serial.write(cmd)
		
		update.message.reply_text(DIC["cond_success"])
	except Exception as e:
		update.message.reply_text(DIC["comm_error"])
		print(e)
	
def show_conditions(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	conds = irrigation.conditions()
	update.message.reply_text(conds or "No data")
	
def request_lines(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	irrigation.request_lines()
	
# Set stop condition for a line
def set_stop_condition(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	print("\nReceived (stop): %s" % update.message.text)
	
	try:
		textSp = update.message.text.split()
		num_line = int(textSp[1])
		cond_text = " ".join(textSp[2:])
		stop_cond  = Condition.parse(cond_text)
		irrigation.stop_condition(num_line, stop_cond)
		
		#irrigation.serial.write(cmd)
		update.message.reply_text(DIC["cond_success"])
	except Exception as e:
		update.message.reply_text(DIC["comm_error"])
		print(e)
	
# Request sensor updates (for all the lines)
def get_sensor_updates(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	global message
	message = update.message
	
	try:
		irrigation.update() 
	except Exception as e:
		update.message.reply_text(DIC["comm_error"])
		print(e)

def list_users(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	s = DIC["allowed_users"]
	for user in allowed_users:
		s += "\n- %s" % user
		
	update.message.reply_text(s)
	
def add_user(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	try:
		textSp = update.message.text.split()
		if textSp[1] not in allowed_users:
			update.message.reply_text(DIC["user_added"])
			allowed_users.append(textSp[1])
		else:
			update.message.reply_text(DIC["user_already_added"])
	except:
		update.message.reply_text(DIC["comm_error"])
		print(e)
		
	update.message.reply_text(s)
	
def remove_user(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	try:
		textSp = update.message.text.split()
		if textSp[1] in allowed_users:
			update.message.reply_text(DIC["user_removed"])
			allowed_users.remove(textSp[1])
		else:
			update.message.reply_text(DIC["user_unknown"])
	except:
		update.message.reply_text(DIC["comm_error"])
		print(e)
		
	update.message.reply_text(s)
	
def enable_greenhouse(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	try:
		textSp = update.message.text.split()
		irrigation.enable_greenhouse(int(textSp[1]))
	except:
		update.message.reply_text(DIC["comm_error"])
		
def disable_greenhouse(bot, update):
	if update.message.chat.username not in allowed_users:
		update.message.reply_text(DIC["user_not_allowed"])
		return
		
	try:
		textSp = update.message.text.split()
		irrigation.disable_greenhouse(int(textSp[1]))
	except:
		update.message.reply_text(DIC["comm_error"])
	
	
# ------------------------------------------------------------------------------------
		
if __name__ == "__main__":
	# Handlers
	dispatcher.add_handler(CommandHandler('start', start))
	dispatcher.add_handler(CommandHandler('startCondition', set_start_condition))
	dispatcher.add_handler(CommandHandler('stopCondition', set_stop_condition))
	dispatcher.add_handler(CommandHandler('conditions', show_conditions))
	dispatcher.add_handler(CommandHandler('updates', get_sensor_updates))
	dispatcher.add_handler(CommandHandler('requestLines', request_lines))
	
	dispatcher.add_handler(CommandHandler('users', list_users))
	dispatcher.add_handler(CommandHandler('addUser', add_user))
	dispatcher.add_handler(CommandHandler('removeUser', remove_user))
	
	dispatcher.add_handler(CommandHandler('enableGreenhouse', enable_greenhouse))
	dispatcher.add_handler(CommandHandler('disableGreenhouse', disable_greenhouse))
	
	dispatcher.add_handler(MessageHandler(Filters.text, text))
	dispatcher.add_handler(MessageHandler(Filters.command, unknown))

	updater.start_polling()  # Start the bot

	time.sleep(1)
	irrigation.request_lines()

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
					updatesResp += DIC["line"] + " %s : %.2f C, %.2f %%\n" % (line, float(temp), float(hum))
					
				if updatesResp != "" and message is not None:
					message.reply_text(updatesResp[:-1])
					message = None
				
			if input.startswith("I;"):  # Init (Start)
				elems = input.split(SERIAL_DELIM)
				cond = Condition(*map(float,elems[-5:]))
				irrigation.update_line(int(elems[1]), True, cond)
				
			if input.startswith("S;"):  # Stop
				elems = input.split(SERIAL_DELIM)
				cond = Condition(*map(float,elems[-5:]))
				irrigation.update_line(int(elems[1]), False, cond)
				
		except serial.SerialTimeoutException:
			print('Data could not be read')
		except Exception as e:
			print(e)