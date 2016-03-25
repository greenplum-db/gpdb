import datetime, sys

print "=============== RUN PIE, RUN ========================="

print datetime.__file__


try:
	import paramiko
except:
	print "failed to import paramiko"
try:
	import _posixsubprocess
	print _posixsubprocess.__file__
except:
	print "failed to import posixsubprocess"

try:
	import Crypto
	print Crypto.__file__
except:
	print "failed to import Crypto"

sys.path.append("/usr/lib/python2.7/dist-packages")

try:
	import paramiko
except:
	print "failed to import paramiko"

try:
	import Crypto
	print Crypto.__file__
except:
	print "failed to import Crypto"

try:
	import _posixsubprocess
	print _posixsubprocess.__file__
except:
	print "failed to import posixsubprocess"

print "=============== DONE ========================="
