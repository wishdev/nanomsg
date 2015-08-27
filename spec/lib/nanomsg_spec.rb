
require 'spec_helper'

describe NanoMsg do
  it 'has NN_LINGER constant set to an integer value' do
    NanoMsg::NN_LINGER.assert >= 0
  end  
  it 'allows shutting down using nn_term' do
    NanoMsg.assert.respond_to?(:terminate)
  end
end