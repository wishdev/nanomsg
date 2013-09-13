
```ruby
require 'nanomsg'

req = NanoMsg::ReqSocket.new
req.connect('tcp://127.0.0.1:4568')

req.send 'req 1'
p req.recv # => "rep 1"
```