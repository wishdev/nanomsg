$:.unshift File.dirname(__FILE__) + "/../lib"
require 'nanomsg'
include NanoMsg

a = 'tcp://127.0.0.1:5432'
b = 'tcp://127.0.0.1:5433'

one, two, three = BusSocket.new, BusSocket.new, 
  BusSocket.new(AF_SP_RAW)

one.bind a
two.bind b

three.connect a
three.connect b
Thread.start do
  Device.new(a, b)
end.abort_on_exception = true

sleep 0.1
one.send 'A'
# Gets sent to three, which forwards it to two. 
puts two.recv