
# NAME

NanoMsg - Simple IPC using topologies, not transports.

# VERSION

This document describes version 0.3.2 of the nanomsg rubygem and the version 0.1-alpha of the nanomsg C library.

# SYNOPSIS

```ruby
require 'nanomsg'

a = NanoMsg::PairSocket.new
b = NanoMsg::PairSocket.new

a.connect('inproc://b')
b.bind('inproc://b')

a.send 'nanomsg for IPC'
b.recv # => 'nanomsg for IPC'
```

# AUTHOR

Kaspar Schiess (kaspar.schiess@[absurd.li](http://absurd.li))

# STATUS

Early in development, this hasn't received a lot of testing. This gem may still bite you. Please report all errors you may find. 

# LICENSE AND COPYRIGHT

Copyright (c) 2013, Kaspar Schiess, All rights reserved.

This module is free software; you can redistribute it and/or modify it under the terms of the MIT license. (See [LICENSE](https://bitbucket.org/kschiess/nanomsg/src/4e9ca30c2af5c336380715a4a0a0f0d522078eab/LICENSE?at=master) file)

This website is under a [CC Attribution](http://creativecommons.org/licenses/by/3.0/) license. 