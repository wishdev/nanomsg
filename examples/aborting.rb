$:.unshift File.dirname(__FILE__) + "/../lib"
require 'nanomsg'
include NanoMsg

class AbortBehaviourChecker
  attr_reader :t
  attr_reader :q, :p

  def initialize
    @q, @p = PairSocket.new, PairSocket.new
    @adr = 'tcp://127.0.0.1:4000'

    @q.bind(@adr)
    @p.connect(@adr)

    @q.setsockopt(NanoMsg::NN_SOL_SOCKET, NanoMsg::NN_RCVTIMEO, 5000)
  end

  def run_recv_thread
    @t = Thread.start { 
      begin 
        @q.recv 
      rescue NanoMsg::Errno::EAGAIN
        puts "EAGAIN"
        # Gets raised when the read operation times out after 5 seconds
      end
    }
    @t.abort_on_exception = false
  end

  def kill
    @t.kill
  end
  def join
    s = Time.now
    @t.join
    Time.now - s
  end

  def send msg
    @p.send msg
  end
  def recv 
    @q.recv
  end
end 


# ------------------------------------------------- NN_RCVTIMEO behaviour test

abc = AbortBehaviourChecker.new
s = Time.now

abc.run_recv_thread

# During these first 10 seconds, CPU usage needs to stay low, since we're 
# just waiting for a message to come in. 
loop do
  p abc.t
  sleep 1
  break if Time.now - s > 7
end

# -------------------------------------------------------- kill behaviour test

# Resets the EAGAIN behaviour
abc.send 'test'
p abc.recv

abc.run_recv_thread
abc.kill

puts "Joining..."
t = abc.join.round
puts "Join took #{t} seconds."
