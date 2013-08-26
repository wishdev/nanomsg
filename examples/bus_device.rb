$:.unshift File.dirname(__FILE__) + "/../lib"
require 'nanomsg'
include NanoMsg

a = 'tcp://127.0.0.1:5432'
b = 'tcp://127.0.0.1:5433'
c = 'tcp://127.0.0.1:5434'

one, two, three = 3.times.map { BusSocket.new }
four, five, six = 3.times.map { BusSocket.new(AF_SP_RAW) }

one.bind a
two.bind b
three.bind c

# one -> four -> four -> two
four.connect a
four.connect b
Thread.start do
  NanoMsg.run_loopback(four)
end.abort_on_exception = true

# one -> five -> six -> three
five.connect a
six.connect c
Thread.start do
  NanoMsg.run_device(five, six)
end.abort_on_exception = true

sleep 0.2
one.send 'A'

# Gets sent to three, which forwards it to two. 
puts two.recv

# Gets sent to five, which forwards it through six to three.
puts three.recv

NanoMsg.terminate