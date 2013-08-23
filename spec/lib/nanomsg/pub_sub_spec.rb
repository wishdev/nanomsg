require 'spec_helper'

require 'timeout'

describe 'PUB/SUB sockets' do
  describe 'PUB socket' do
    it 'has no #recv method'
  end
  describe 'SUB socket' do
    it 'has no #send method'
    it 'allows subscribing to channels'
    it 'allows unsubscribing to channels'
  end

  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      let(:pub) { NanoMsg::PubSocket.new }
      let(:sub1) { NanoMsg::SubSocket.new }
      let(:sub2) { NanoMsg::SubSocket.new }

      around(:each) { |example| 
        timeout(1) { example.run }}

      it 'allows simple pub/sub' do
        pub.bind(tbind)
        sub1.connect(tbind)
        sub2.connect(tbind)

        sub1.subscribe 'foo'
        sub2.subscribe 'foo'

        sub1.subscribe 'bar'

        pub.send 'foo1234'
        sub1.recv.should == 'foo1234'
        sub2.recv.should == 'foo1234'

        pub.send 'bar4567'
        pub.send 'foo9999'
        sub1.recv.should == 'bar4567'
        sub2.recv.should == 'foo9999'
      end      
    end
  end

  examples_for_transport "ipc:///tmp/pubsub.ipc"
  examples_for_transport "tcp://127.0.0.1:5557"
  examples_for_transport "inproc://pubsub"
end