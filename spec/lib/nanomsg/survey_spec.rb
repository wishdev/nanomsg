require 'spec_helper'

describe 'SURVEY sockets' do
  let(:surveyor)  { NanoMsg::SurveySocket.new }
  let(:resp1)     { NanoMsg::RespondSocket.new }
  let(:resp2)     { NanoMsg::RespondSocket.new }

  after(:each) do
    [surveyor, resp2, resp1].each(&:close)
  end

  def self.examples_for_transport tbind
    name = tbind.split('://').first
    
    describe "transport #{name}" do
      it 'allows simple surveys' do
        surveyor.deadline.assert == 1 # default: 1 second
        surveyor.deadline = 0.1 # 100ms
        surveyor.deadline.assert == 0.1

        surveyor.bind(tbind)

        resp1.connect(tbind)
        resp2.connect(tbind)

        sleep 0.1
        surveyor.send 'u there?'
        resp1.recv.assert == 'u there?'
        resp2.recv.assert == 'u there?'

        resp1.send 'KTHNXBYE'
        surveyor.recv.assert == 'KTHNXBYE'

        # nanomsg/tests/survey.c says this should be EFSM, the documentation 
        # says it should be ETIMEDOUT. Reality wins. 
        begin
          surveyor.recv
        rescue NanoMsg::Errno::EFSM
          # Old nanomsg version
        rescue NanoMsg::Errno::ETIMEDOUT
          # New version
        end
      end      
    end
  end

  examples_for_transport "ipc:///tmp/reqrep.ipc"
  examples_for_transport "tcp://127.0.0.1:5555"
  examples_for_transport "inproc://reqrep"
end