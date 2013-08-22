
$:.unshift File.dirname(__FILE__) + "/../ext"
require 'nanomsg.bundle'

module NanoMsg
  class Socket
  end
end

require 'nanomsg/pair_socket'
