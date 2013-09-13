```ruby
# The 'cat' subscriber
require 'nanomsg'

sub = NanoMsg::SubSocket.new
sub.connect('tcp://127.0.0.1:4569')

sub.subscribe 'cat'
p sub.recv # => 'cat 45678'
```

```ruby
# And here's the 'dog' subscriber
require 'nanomsg'

sub = NanoMsg::SubSocket.new
sub.connect('tcp://127.0.0.1:4569')

sub.subscribe 'dog'
p sub.recv # => 'dog 12345'
```