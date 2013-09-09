
```ruby
require 'nanomsg'

socket = NanoMsg::PairSocket.new
socket.connect 'inproc://b'
socket.send 'm1'
```
