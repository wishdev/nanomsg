
```ruby
require 'nanomsg'

rep = NanoMsg::RepSocket.new
rep.bind('tcp://127.0.0.1:4568')

rep.recv # => 'req 1'
rep.send 'rep 1'
```