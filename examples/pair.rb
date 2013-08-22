
$:.unshift File.dirname(__FILE__) + "/../lib"
require 'nanomsg'

sock1 = NanoMsg::PairSocket.new(NanoMsg::AF_SP)
sock1.bind("ipc:///tmp/test.ipc")

sock2 = NanoMsg::PairSocket.new(NanoMsg::AF_SP)
sock2.connect("ipc:///tmp/test.ipc")

sock1.send 'test'
sock1.send 'nanomsg'

puts sock2.recv
puts sock2.recv

