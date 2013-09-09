
```ruby
require 'nanomsg'

socket = NanoMsg::PairSocket.new
socket.bind 'inproc://b'
socket.recv # => 'm1'
```
