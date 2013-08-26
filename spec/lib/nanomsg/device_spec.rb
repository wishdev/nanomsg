require 'spec_helper'

require 'timeout'

describe 'Devices' do
  describe "connecting two BUSes" do
    let(:a) { NanoMsg::BusSocket.new(NanoMsg::AF_SP_RAW) }
    let(:b) { NanoMsg::BusSocket.new(NanoMsg::AF_SP_RAW) }
    let(:c) { NanoMsg::BusSocket.new }
    let(:d) { NanoMsg::BusSocket.new }

    let(:adr1) { 'inproc://a' }
    let(:adr2) { 'inproc://b' }

    before(:each) do
      a.bind(adr1)
      b.bind(adr2)

      c.connect adr1
      d.connect adr2
    end

    let!(:thread) {
      Thread.start do
        begin
          NanoMsg.run_device(a, b)
        rescue NanoMsg::Errno::ETERM
          # Ignore, spec shutdown
        end
      end
    }
    
    it "forwards messages" do
      sleep 0.01
      c.send 'test'
      timeout(1) do
        d.recv.should == 'test'
      end
    end
  end
end