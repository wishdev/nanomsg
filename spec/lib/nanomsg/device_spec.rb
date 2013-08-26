require 'spec_helper'

require 'timeout'

describe 'Devices' do
  describe "connecting two BUSes" do
    let(:a) { NanoMsg::BusSocket.new(NanoMsg::AF_SP_RAW) }
    let(:b) { NanoMsg::BusSocket.new(NanoMsg::AF_SP_RAW) }
    let(:c) { NanoMsg::BusSocket.new }

    before(:each) do
      a
      b.bind('inproc://b')
      c.connect('inproc://b')
    end

    after(:each) do
      [a, b, c].each(&:close)
    end

    let!(:thread) {
      Thread.start do
        NanoMsg.run_device(a, b)
      end.abort_on_exception = true
    }
    
    it "forwards messages" do
      sleep 0.01
      c.send 'test'
      timeout(1) do
        a.recv.should == 'test'
      end
    end
  end
end