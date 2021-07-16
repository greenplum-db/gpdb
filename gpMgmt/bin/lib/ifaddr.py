#!/usr/bin/env python

import socket
import sys

class Address:
  def __init__(self, address):
    self.address = address
    try:
      tup = socket.getaddrinfo(address, 0, socket.AF_UNSPEC, socket.SOCK_DGRAM, 0, socket.AI_NUMERICHOST)
      self.family, _, _, canonname, sockaddr = tup[0]
      self.extra = {
          'canonname': canonname,
          'sockaddr': sockaddr,
          }
    except:
      # it means the address is not an IP address
      self.family = 0

  def __str__(self):
    return '''Address('%s'):type=%d''' % (self.address, self.getAddressType())

  def getAddressType(self):
    if self.family == 0:
      return 0
    if self.family == socket.AF_INET:
      return 4
    if self.family == socket.AF_INET6:
      return 6
    raise Exception('unknown: bad state')

  def getCIDR4IP(self):
    if self.family == 0:
      return self.address
    if self.family == socket.AF_INET:
      return self.address + "/32"
    if self.family == socket.AF_INET6:
      return self.address + "/128"
    raise Exception('unknown: bad state')


if __name__ == '__main__':
  if len(sys.argv) < 2:
    usage()
  addr = Address(sys.argv[1])
  print(addr.getCIDR4IP())
