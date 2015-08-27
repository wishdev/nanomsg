require 'mkmf'

have_library 'nanomsg'

# define either HAVE_RB_THREAD_CALL_WITHOUT_GVL or
# HAVE_RB_THREAD_BLOCKING_REGION. We cannot compile without atm.
have_func('rb_thread_call_without_gvl', 'ruby/thread.h')
have_func('rb_thread_blocking_region', 'ruby.h')

create_makefile('nanomsg_ext')