import threading
import time
import xbee
import serial

# Note that while only one XBee will be used to send data to multiple
# controllers, there will need to be 'n' serial cables for 'n' controllers.
# So this lock will only be used in case of XBee
xbeeLock = threading.Lock()

class SpeedSender (threading.Thread):
	STOP = 0
	SLOW = 1
	NORM = 2
	FAST = 3

	SERIAL = 5
	XBEE = 6

	DEST2 = "\x00\x02"
	DEST3 = "\x00\x03"

	def __init__(self, commType, interval, lock, serialSender, xbeeSender, destination):
		self.speed = self.STOP
		self.commType = commType
		self.interval = interval
		self.lock = lock
		if commType == self.SERIAL:
			self.serialSender = serialSender
		elif commType == self.XBEE:
			self.xbeeSender = xbeeSender
			self.destination = destination

		threading.Thread.__init__(self)

	# Constructs a version of this class for serial communication, given the
	# interval in seconds (a float) and a serial port number (i.e. 0 for
	# "/dev/ttyUSB0")
	@classmethod
	def forSerial(cls, interval, serialPort):
		commType = SpeedSender.SERIAL
		# Note that one lock is created for each sender (as opposed to one
		# global lock) because each sender has its own serial cable
		serialLock = threading.Lock()
		serialSender = serial.Serial('/dev/ttyUSB' + str(serialPort), 9600)

		return cls(commType, interval, serialLock, serialSender, 0, 0)

	# Constructs a version of this class for serial communication, given the
	# interval in seconds (a float), a pre-constructed xbee.XBee object for
	# sending, and either DEST2 or DEST3 for the destination, representing
	# XBee modules labeled "2" and "3" respectively.
	#
	# To construct an XBee sender object, use:
	# 
	# import xbee
	# import serial
	# 
	# xbeeSender = xbee.XBee(serial.Serial("/dev/ttyUSB0", 9600))
	#
	@classmethod
	def forXBee(cls, interval, xbeeSender, destination):
		global xbeeLock

		commType = SpeedSender.XBEE

		return cls(commType, interval, xbeeLock, 0, xbeeSender, destination)

	# This method can be used to change the speed of the car asynchronously
	# from outside the thread.
	def changeSpeed(self, speed):
		if speed != self.speed:
			self.speed = speed
			self.__sendSpeed()

	def __sendSpeed(self):
		global xbeeLock

		self.lock.acquire()

		if self.commType == self.SERIAL:
			self.serialSender.write(chr(self.speed))
		elif self.commType == self.XBEE:
			self.xbeeSender.send('tx', dest_addr=self.destination, data=chr(self.speed))

		self.lock.release()

	def run(self):
		t1 = time.time() + self.interval

		while True:
			self.__sendSpeed()
			
			time.sleep(t1 - time.time())
			t1 = time.time() + self.interval
