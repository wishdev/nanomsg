
# How to use the BUS socket. 

$:.unshift File.dirname(__FILE__) + "/../lib"
require 'nanomsg'
include NanoMsg

buses = 3.times.map { BusSocket.new }

a = 'tcp://127.0.0.1:5432'
b = 'tcp://127.0.0.1:5433'

one, two, three = *buses

one.bind a

two.bind b
two.connect a

three.connect a 
three.connect b

sleep 0.1

one.send 'A'
p two.recv
p three.recv

two.send 'B'
p one.recv
p three.recv

three.send 'C'
p one.recv
p two.recv
