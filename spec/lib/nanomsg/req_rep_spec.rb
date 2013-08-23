require 'spec_helper'

describe 'REQ/REP sockets' do
  let(:req) { NanoMsg::ReqSocket.new }
  let(:rep) { NanoMsg::RepSocket.new }

  after(:each) do
    req.close
    rep.close
  end

  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      it 'allows simple req/rep' do
        req.bind(tbind)
        rep.connect(tbind)

        req.send 'req1'

        rep.recv.should == 'req1'
        rep.send 'rep1'
        
        req.recv.should == 'rep1'
      end      
    end
  end

  examples_for_transport "ipc:///tmp/reqrep.ipc"
  examples_for_transport "tcp://127.0.0.1:5555"
  examples_for_transport "inproc://reqrep"
end