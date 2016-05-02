require 'spec_helper'

describe NanoMsg::PairSocket do
  let(:sock1) { NanoMsg::PairSocket.new }
  let(:sock2) { NanoMsg::PairSocket.new }

  after(:each) do
    sock1.close
    sock2.close
  end

  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      it 'allows simple send/recv' do
        sock1.bind(tbind)
        sock2.connect(tbind)

        sock1.send('1234').assert == 4
        sock2.recv.assert == '1234'

        sock2.send('5678').assert == 4
        sock1.recv.assert == '5678'    
      end
      it 'allows file descriptor retrieval' do
        sock1.bind(tbind)
        sock2.connect(tbind)

        sock1.send('1234').assert == 4

        # Now our socket should be ready: 
        int_fd = sock2.getsockopt(NanoMsg::NN_SOL_SOCKET, NanoMsg::NN_RCVFD)

        # Reconstruct a Ruby IO from the integer above. Note that if you 
        # #close this FD, things will be messed up for NanoMsg.
        io = IO.for_fd(int_fd)
        io.autoclose = false

        r, _, _ = IO.select([io])
        r.size.assert == 1
        r.first.assert == io
      end
    end
  end

  examples_for_transport "ipc:///tmp/test.ipc"
  examples_for_transport "tcp://127.0.0.1:5555"
  examples_for_transport "inproc://test"
end