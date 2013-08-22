require 'spec_helper'

describe NanoMsg::PairSocket do
  let(:sock1) { NanoMsg::PairSocket.new }
  let(:sock2) { NanoMsg::PairSocket.new }

  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      it 'allows simple send/recv' do
        sock1.bind(tbind)
        sock2.connect(tbind)

        sock1.send('1234').should == 4
        sock2.recv.should == '1234'

        sock2.send('5678').should == 4
        sock1.recv.should == '5678'    
      end      
    end
  end

  examples_for_transport "ipc:///tmp/test.ipc"
  examples_for_transport "tcp://127.0.0.1:5555"
  examples_for_transport "inproc://test"
end