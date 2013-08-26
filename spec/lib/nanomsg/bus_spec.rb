require 'spec_helper'

describe 'BUS sockets' do
  def self.examples_for_transport tbind1, tbind2
    name = tbind1.split('://').first
    
    describe "transport #{name}" do
      describe "using a broker topology" do
        let(:participants) {
          3.times.map { NanoMsg::BusSocket.new(NanoMsg::AF_SP_RAW) }
        }

        after(:each) do
          participants.each(&:close)
        end

        it 'forwards messages to everyone (except the producer)' do
          participants.zip([tbind1, tbind2]).each_cons(2) do |(sock1, adr), (sock2, _)|
            puts "#{sock1.object_id} <- #{sock2.object_id}"
            sock1.bind(adr)
            sock2.connect(adr)
          end
          sleep 0.1

          participants.each_with_index do |p, i|
            puts "#{p.object_id}: #{i}"
            p.send i.to_s
          end

          participants[0].recv.to_i.should == 1
          participants[0].recv.to_i.should == 2

          participants[1].recv.to_i.should == 0
          participants[1].recv.to_i.should == 2

          participants[2].recv.to_i.should == 0
          participants[2].recv.to_i.should == 1
        end
      end
    end
  end

  examples_for_transport "ipc:///tmp/bus1.ipc", "ipc:///tmp/bus2.ipc"
  examples_for_transport "tcp://127.0.0.1:5555", "tcp://127.0.0.1:5556"
  examples_for_transport "inproc://bus1", "inproc://bus2"
end