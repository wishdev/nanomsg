

$:.unshift File.dirname(__FILE__) + "/../ext"
require 'nanomsg'

RSpec.configure do |rspec|
  rspec.after(:all) {
    NanoMsg.terminate
  }
end