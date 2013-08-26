require 'spec_helper'

describe 'Devices' do
  describe "connecting two BUSes" do
    let(:a) { NanoMsg::BusSocket.new(NanoMsg::AF_SP_RAW).bind('inproc://a') }
    let(:b) { NanoMsg::BusSocket.new(NanoMsg::AF_SP_RAW).bind('inproc://b') }
    let(:c) { NanoMsg::BusSocket.new.connect('inproc://b') }

    after(:each) do
      [a, b, c].each(&:close)
    end

    let!(:thread) {
      Thread.start do
        NanoMsg.run_device(a, b)
      end.abort_on_exception = true
    }
    after(:each) do
      thread.kill
    end

    it "forwards messages" do
      c.send 'test'
      timeout(1) do
        a.recv.should == 'test'
      end
    end
  end
end