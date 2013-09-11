
```ruby
require 'nanomsg'

socket = NanoMsg::PairSocket.new
socket.bind 'tcp://127.0.0.1:4567'
socket.send 'm1'
```
