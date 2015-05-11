require 'mkmf'

have_library 'nanomsg'

create_makefile('nanomsg_ext')