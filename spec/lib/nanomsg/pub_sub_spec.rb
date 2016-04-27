require 'spec_helper'

require 'timeout'

describe 'PUB/SUB sockets' do
  describe 'PUB socket' do
    let!(:pub) { NanoMsg::PubSocket.new }
    
    it 'has no #recv method' do
      NanoMsg::Errno::ENOTSUP.assert.raised? do
        pub.recv
      end
    end
  end
  describe 'SUB socket' do
    let!(:sub) { NanoMsg::SubSocket.new }
    it 'has no #send method' do
      NanoMsg::Errno::ENOTSUP.assert.raised? do
        sub.send 'foo'
      end
    end
    
    it 'allows unsubscribing to channels' do
      sub.subscribe 'foo'
      sub.unsubscribe 'foo'
    end
    it 'allows unsubscribing to channels' do
      # NOTE unsubscribe without previous subscribe currently provokes an 
      #   access protection (null pointer deref) in nn_setsockopt/NN_SUB_UNSUBSCRIBE
      #   We told you it was alpha software. 
      # sub.unsubscribe 'foo'
    end
  end

  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      let(:pub) { NanoMsg::PubSocket.new }
      let(:sub1) { NanoMsg::SubSocket.new }
      let(:sub2) { NanoMsg::SubSocket.new }

      after(:each) {
        pub.close
        sub1.close
        sub2.close
      }

      around(:each) { |example| 
        timeout(1) { example.run }}

      it 'allows simple pub/sub' do
        pub.bind(tbind)
        sub1.connect(tbind)
        sub2.connect(tbind)

        sub1.subscribe 'foo'
        sub2.subscribe 'foo'

        sub1.subscribe 'bar'
        sleep 0.1

        pub.send 'foo1234'
        sub1.recv.assert == 'foo1234'
        sub2.recv.assert == 'foo1234'

        pub.send 'bar4567'
        pub.send 'foo9999'
        sub1.recv.assert == 'bar4567'
        sub2.recv.assert == 'foo9999'
      end      
    end
  end

  examples_for_transport "ipc:///tmp/pubsub.ipc"
  examples_for_transport "tcp://127.0.0.1:5557"
  examples_for_transport "inproc://pubsub"
end