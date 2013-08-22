
# Example adapted from nn-cores roundtrip_latency. 
# (https://raw.github.com/chuckremes/nn-core/master/examples/roundtrip_latency.rb)

$:.unshift File.dirname(__FILE__) + "/../lib"
require 'nanomsg'

# Within a single process, we start up two threads. One thread has a REQ (request)
# socket and the second thread has a REP (reply) socket. We measure the
# *round-trip* latency between these sockets. Only *one* message is in flight at
# any given moment.
#
#  % ruby roundtrip_latency.rb tcp://127.0.0.1:5555 1024 1_000_000
#
#  % ruby roundtrip_latency.rb inproc://lm_sock 1024 1_000_000
#

if ARGV.length < 3
  puts "usage: ruby roundtrip_latency.rb <connect-to> <message-size> <roundtrip-count>"
  exit
end

link = ARGV[0]
message_size = ARGV[1].to_i
roundtrip_count = ARGV[2].to_i

def set_signal_handler(receiver, transmitter)
  trap(:INT) do
    puts "got ctrl-c"
    receiver.terminate
    transmitter.terminate
  end
end

class Node
  def initialize(endpoint, size, count)
    @endpoint = endpoint
    @size = size
    @count = count
    @msg = 'a' * @size

    setup_socket
    set_endpoint
  end

  def setup_socket
    @socket = NanoMsg::PairSocket.new
  end

  def send_msg
    nbytes = @socket.send(@msg)
    assert(nbytes)
    nbytes
  end

  def recv_msg
    msg = @socket.recv
    nbytes = msg.bytesize
    assert(nbytes)
    nbytes
  end

  def terminate
    # TODO
    # assert(NNCore::LibNanomsg.nn_close(@socket))
  end

  def assert(rc)
    raise "Last API call failed at #{caller(1)}" unless rc >= 0
  end
end

class Receiver < Node
  def set_endpoint
    @socket.bind(@endpoint)
  end

  def run
    @count.times do
      nbytes = recv_msg

      raise "Message size doesn't match, expected [#{@size}] but received [#{string.size}]" if @size != nbytes

      send_msg
    end

    terminate
  end
end

class Transmitter < Node
  def set_endpoint
    @socket.connect(@endpoint)
  end

  def run
    elapsed = elapsed_microseconds do
      @count.times do
        send_msg

        nbytes = recv_msg

        raise "Message size doesn't match, expected [#{@size}] but received [#{nbytes}]" if @size != nbytes
      end
    end

    latency = elapsed / @count / 2

    puts "message size: %i [B]" % @size
    puts "roundtrip count: %i" % @count
    puts "throughput (msgs/s): %i" % (@count / (elapsed / 1_000_000))
    puts "mean latency: %.3f [us]" % latency
    terminate
  end

  def elapsed_microseconds(&blk)
    start = Time.now
    yield
    value = ((Time.now - start) * 1_000_000)
  end
end

threads = []
receiver = transmitter = nil

threads << Thread.new do
  receiver = Receiver.new(link, message_size, roundtrip_count)
  receiver.run
end

sleep 1

threads << Thread.new do
  transmitter = Transmitter.new(link, message_size, roundtrip_count)
  transmitter.run
end

set_signal_handler(receiver, transmitter)

threads.each {|t| t.join}