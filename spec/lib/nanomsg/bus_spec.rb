require 'spec_helper'

describe 'BUS sockets' do
  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      describe "using a broker topology" do
        let!(:a) { NanoMsg::BusSocket.new }
        let!(:b) { NanoMsg::BusSocket.new }

        after(:each) do
          [a, b].each(&:close)
        end

        it 'forwards messages to everyone (except the producer)' do
          a.bind(tbind)
          b.connect(tbind)
          sleep 0.01

          a.send 'test'
          b.recv.should == 'test'

          b.send 'ing'
          a.recv.should == 'ing'
        end
      end
    end
  end

  examples_for_transport "ipc:///tmp/bus1.ipc"
  examples_for_transport "tcp://127.0.0.1:5558"
  examples_for_transport "inproc://bus1"
end