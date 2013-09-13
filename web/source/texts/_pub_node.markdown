
```ruby
require 'nanomsg'

pub = NanoMsg::PubSocket.new
pub.bind('tcp://127.0.0.1:4569')

loop do
  pub.send 'dog 12345'
  pub.send 'cat 45678'

  sleep 1
end
```