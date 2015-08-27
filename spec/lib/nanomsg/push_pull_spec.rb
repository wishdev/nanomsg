require 'spec_helper'

describe 'PUSH/PULL pipeline sockets' do
  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      describe 'fanout' do
        let(:producer)  { NanoMsg::PushSocket.new }
        let(:consumer1) { NanoMsg::PullSocket.new }
        let(:consumer2) { NanoMsg::PullSocket.new }

        after(:each) do
          [producer, consumer2, consumer1].each(&:close)
        end

        it 'distributes load evenly' do
          producer.bind(tbind)
          consumer1.connect(tbind)          
          consumer2.connect(tbind)
          sleep 0.1

          100.times do |i|
            producer.send i.to_s
          end

          n = [0, 0]
          50.times do
            [consumer1, consumer2].each_with_index do |consumer, idx|
              v = consumer.recv.to_i
              n[idx] += 1
            end
          end

          # This is about a 5% level of the binominal distribution
          (n.first - n.last).abs.assert < 10
        end
      end
      describe 'fanin' do
        let(:producer1) { NanoMsg::PushSocket.new }
        let(:producer2) { NanoMsg::PushSocket.new }
        let(:consumer) { NanoMsg::PullSocket.new }

        after(:each) do
          [producer1, producer2, consumer].each(&:close)
        end

        it 'consumes results fairly' do
          consumer.bind(tbind)
          producer1.connect(tbind)
          producer2.connect(tbind)

          100.times do |i|
            producer = [producer1, producer2][i%2]

            producer.send i.to_s
          end

          100.times do |i|
            consumer.recv.to_i.assert == i
          end
        end
      end
    end
  end

  examples_for_transport "ipc:///tmp/reqrep.ipc"
  examples_for_transport "tcp://127.0.0.1:5555"
  examples_for_transport "inproc://reqrep"
end