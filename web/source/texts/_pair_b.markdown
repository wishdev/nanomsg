
```ruby
require 'nanomsg'

socket = NanoMsg::PairSocket.new
socket.connect 'tcp://127.0.0.1:4567'
socket.recv # => 'm1'
```
