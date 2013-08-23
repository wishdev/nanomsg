
#include "ruby/ruby.h"
#include "ruby/st.h"

#include <nanomsg/nn.h>

static st_table *syserr_tbl;

static VALUE
errno_initialize(VALUE self)
{
  VALUE error; 
  VALUE klass = rb_obj_class(self);

  const char *explanation; 
  VALUE message; 

  error = rb_const_get(klass, rb_intern("Errno"));
  explanation = nn_strerror(FIX2INT(error));

  message = rb_str_new(explanation, strlen(explanation));
  rb_call_super(1, &message);

  rb_iv_set(self, "errno", error);

  return self;
}

static VALUE
errno_errno(VALUE self)
{
    return rb_attr_get(self, rb_intern("errno"));
}

static VALUE
err_add(VALUE module, int n, const char *name)
{
  st_data_t error;

  if (!st_lookup(syserr_tbl, n, &error)) {
    error = rb_define_class_under(module, name, rb_eStandardError);

    rb_define_const(error, "Errno", INT2NUM(n));
    rb_define_method(error, "initialize", errno_initialize, 0);
    rb_define_method(error, "errno", errno_errno, 0);

    st_add_direct(syserr_tbl, n, error);
  }

  return error;
}

VALUE
errno_lookup(int n)
{
  st_data_t error;

  if (!st_lookup(syserr_tbl, n, &error)) 
    return Qnil; 

  return error;
}

void 
Init_constants(VALUE module)
{
  VALUE mErrno; 
  int i, value; 

  syserr_tbl = st_init_numtable();

  mErrno = rb_define_module_under(module, "Errno");

  // Define all constants that nanomsg knows about: 
  for (i = 0; ; ++i) {
    const char* name = nn_symbol (i, &value);
    if (name == NULL) break;

    // I see no point in declaring values other than those starting with NN_: 
    if (strncmp(name, "NN_", 3) == 0) 
      rb_const_set(module, rb_intern(name), INT2NUM(value));

    if (strncmp(name, "E", 1) == 0) 
      err_add(mErrno, value, name);
  }
}
