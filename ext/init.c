
#include "ruby/ruby.h"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pair.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/survey.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/bus.h>

#include "constants.h"

/* 
  This API was essentially renamed in Ruby 2.2. Let's provide a small layer 
  that allows us to compile in both <2.2 and >=2.2 versions. 
*/
#if defined(HAVE_RB_THREAD_CALL_WITHOUT_GVL)
/* Ruby 2.0+ */
#  include <ruby/thread.h>
#  define WITHOUT_GVL(fn,a,ubf,b) \
  rb_thread_call_without_gvl((fn),(a),(ubf),(b))
#elif defined(HAVE_RB_THREAD_BLOCKING_REGION)
typedef VALUE (*blocking_fn_t)(void*);
#  define WITHOUT_GVL(fn,a,ubf,b) \
  rb_thread_blocking_region((blocking_fn_t)(fn),(a),(ubf),(b))
#endif

static VALUE mNanoMsg;
static VALUE cSocket; 

static VALUE cPairSocket; 

static VALUE cReqSocket;
static VALUE cRepSocket;

static VALUE cPubSocket;
static VALUE cSubSocket;

static VALUE cSurveySocket;
static VALUE cRespondSocket;

static VALUE cPushSocket;
static VALUE cPullSocket;

static VALUE cBusSocket;

static VALUE ceSocketError;

struct nmsg_socket {
  int socket; 
};


static void
sock_mark(void *ptr)
{
  // struct nmsg_socket *sock = ptr;
}

static void
sock_free(void *ptr)
{
  struct nmsg_socket *sock = ptr;
  xfree(sock);
}

static size_t
sock_memsize(const void *ptr)
{
  return ptr ? sizeof(struct nmsg_socket) : 0;
}

static const rb_data_type_t socket_data_type = {
  "nanomsg_socket",
  {sock_mark, sock_free, sock_memsize,},
};

static struct nmsg_socket*
sock_get_ptr(VALUE socket) 
{
  struct nmsg_socket *psock; 
  TypedData_Get_Struct(socket, struct nmsg_socket, &socket_data_type, psock); 

  return psock; 
}

static int 
sock_get(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket);
  return psock->socket;
}

static VALUE
sock_alloc(VALUE klass) 
{
  VALUE obj;
  struct nmsg_socket *psock;

  obj = TypedData_Make_Struct(klass, struct nmsg_socket, &socket_data_type, psock);
  return obj;
}

static void
sock_raise_error(int nn_errno)
{
  VALUE error = errno_lookup(nn_errno);

  if (error != Qnil) {
    VALUE exc = rb_class_new_instance(0, NULL, error);
    rb_exc_raise(exc);
  } 

  rb_raise(ceSocketError, "General failure, no such error code %d found.", nn_errno);
}
#define RAISE_SOCK_ERROR { sock_raise_error(nn_errno()); }

static VALUE
sock_bind(VALUE socket, VALUE bind)
{
  int sock = sock_get(socket);
  int endpoint; 

  endpoint = nn_bind(sock, StringValueCStr(bind));
  if (endpoint < 0) 
    RAISE_SOCK_ERROR; 

  // TODO do something with the endpoint, returning it in a class for example. 
  return Qnil; 
}

static VALUE
sock_connect(VALUE socket, VALUE connect)
{
  int sock = sock_get(socket);
  int endpoint; 

  endpoint = nn_connect(sock, StringValueCStr(connect));
  if (endpoint < 0) 
    RAISE_SOCK_ERROR; 

  // TODO do something with the endpoint, returning it in a class for example. 
  return Qnil; 
}

static void
sock_init(VALUE socket, int domain, int protocol)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(domain, protocol);
  if (psock->socket < 0)
    RAISE_SOCK_ERROR;
}

struct ioop {
  int nn_errno; 
  int return_code; 
  int sock; 
  char* buffer; 
  long len; 
};

static void*
sock_send_blocking(void* data)
{
  struct ioop *pio = data; 

  pio->return_code = nn_send(pio->sock, pio->buffer, pio->len, 0 /* flags */);
  if (pio->return_code < 0)
    pio->nn_errno = nn_errno(); 

  return (void*) 0;   
}

static VALUE
sock_send(VALUE socket, VALUE buffer)
{
  struct ioop io; 

  io.sock = sock_get(socket);
  io.buffer = StringValuePtr(buffer);
  io.len = RSTRING_LEN(buffer);

  WITHOUT_GVL(sock_send_blocking, &io, RUBY_UBF_IO, 0);

  if (io.return_code < 0)
    sock_raise_error(io.nn_errno);

  return INT2NUM(io.return_code);
}

static void*
sock_recv_blocking(void* data)
{
  struct ioop *pio = data; 

  pio->return_code = nn_recv(pio->sock, &pio->buffer, NN_MSG, 0 /* flags */);

  if (pio->return_code < 0)
    pio->nn_errno = nn_errno(); 

  return (void*) 0; 
}

static VALUE
sock_recv(VALUE socket)
{
  VALUE result; 
  struct ioop io; 

  io.sock = sock_get(socket);

  WITHOUT_GVL(sock_recv_blocking, &io, RUBY_UBF_IO, 0);

  if (io.return_code < 0)
    sock_raise_error(io.nn_errno);

  result = rb_str_new(io.buffer, io.return_code);
  nn_freemsg(io.buffer); io.buffer = (char*) 0;

  return result;
}

static void*
sock_close_no_gvl(void* data)
{
  struct ioop *pio = (struct ioop*) data; 

  pio->return_code = nn_close(pio->sock);
  if (pio->return_code < 0)
    pio->nn_errno = nn_errno();

  return (void*) 0;
}

static VALUE
sock_close(VALUE socket)
{
  struct ioop io; 

  io.sock = sock_get(socket);

  // I've no idea on how to abort a close (which may block for NN_LINGER 
  // seconds), so we'll be uninterruptible. 
  WITHOUT_GVL(sock_close_no_gvl, &io, RUBY_UBF_IO, 0);

  if (io.return_code < 0)
    sock_raise_error(io.nn_errno);

  return Qnil;
}

static VALUE
sock_setsockopt(VALUE self, VALUE level, VALUE option, VALUE val)
{
  int sock = sock_get(self);
  int level_arg = FIX2INT(level); 
  int option_arg = FIX2INT(option);
  int err; 
  int i; 
  void* v; 
  size_t vlen = 0;

  switch (TYPE(val)) {
    case T_FIXNUM:
      i = FIX2INT(val);
      goto numval;
    case T_FALSE:
      i = 0;
      goto numval;
    case T_TRUE:
      i = 1;
    numval: 
      v = (void*)&i; vlen = sizeof(i);
      break;
    
    default:
      StringValue(val);
      v = RSTRING_PTR(val);
      vlen = RSTRING_LEN(val);
      break;
  }

  err = nn_setsockopt(sock, level_arg, option_arg, v, vlen);
  if (err < 0)
    RAISE_SOCK_ERROR; 

  return self; 
}

static VALUE 
sock_getsockopt(VALUE self, VALUE level, VALUE option)
{
  int sock = sock_get(self);
  int level_arg = FIX2INT(level); 
  int option_arg = FIX2INT(option);
  int err; 
 
  int out; 
  size_t sz; 

  if (level_arg == NN_SOL_SOCKET) {
    // This list of supported options is not complete yet. However, having
    // a few supported sockets here is more useful than none at all. WIP. 
    // Yes, pr please ;) 
    switch (option_arg) {
      case NN_SNDFD: 
      case NN_RCVFD: 
        // Return will be int, which is a system fd. 
        sz = sizeof(out);
        err = nn_getsockopt(sock, level_arg, option_arg, &out, &sz);
        if (err < 0)
          RAISE_SOCK_ERROR;

        return INT2NUM(out);
      default: 
        // Unsupported option, returning nil. 
        return Qnil;
    }  
  }
  else {
    // And yet more code here. 
    return Qnil; 
  }
}

#define SOCK_INIT_FUNC(name, type) \
static VALUE \
name(int argc, VALUE *argv, VALUE self) \
{ \
  VALUE domain; \
  \
  rb_scan_args(argc, argv, "01", &domain); \
  if (NIL_P(domain)) \
    sock_init(self, AF_SP, (type)); \
  else \
    sock_init(self, FIX2INT(domain), (type)); \
  \
  return self; \
}

SOCK_INIT_FUNC(pair_sock_init, NN_PAIR);
SOCK_INIT_FUNC(req_sock_init, NN_REQ);
SOCK_INIT_FUNC(rep_sock_init, NN_REP);
SOCK_INIT_FUNC(pub_sock_init, NN_PUB);
SOCK_INIT_FUNC(sub_sock_init, NN_SUB);
SOCK_INIT_FUNC(srvy_sock_init, NN_SURVEYOR);
SOCK_INIT_FUNC(resp_sock_init, NN_RESPONDENT);
SOCK_INIT_FUNC(push_sock_init, NN_PUSH);
SOCK_INIT_FUNC(pull_sock_init, NN_PULL);
SOCK_INIT_FUNC(bus_sock_init, NN_BUS);

static VALUE
sub_sock_subscribe(VALUE socket, VALUE channel)
{
  int sock = sock_get(socket);
  int err;

  err = nn_setsockopt(
    sock, NN_SUB, NN_SUB_SUBSCRIBE,
    StringValuePtr(channel),
    RSTRING_LEN(channel)
  );
  if (err < 0)
    RAISE_SOCK_ERROR;

  return socket;
}

static VALUE
sub_sock_unsubscribe(VALUE socket, VALUE channel)
{
  int sock = sock_get(socket);
  int err;

  err = nn_setsockopt(
    sock, NN_SUB, NN_SUB_UNSUBSCRIBE,
    StringValuePtr(channel),
    RSTRING_LEN(channel)
  );
  if (err < 0)
    RAISE_SOCK_ERROR;

  return socket;
}

static VALUE
srvy_set_deadline(VALUE self, VALUE deadline)
{
  int sock = sock_get(self);
  VALUE miliseconds = rb_funcall(deadline, rb_intern("*"), 1, INT2NUM(1000));
  int timeout = FIX2INT(miliseconds);
  int err; 

  err = nn_setsockopt(sock, NN_SURVEYOR, NN_SURVEYOR_DEADLINE, &timeout, sizeof(int));
  if (err < 0)
    RAISE_SOCK_ERROR;

  return deadline; 
}

static VALUE
srvy_get_deadline(VALUE self)
{
  int sock = sock_get(self);
  int deadline; 
  size_t size = sizeof(int);

  int err; 

  err = nn_getsockopt(sock, NN_SURVEYOR, NN_SURVEYOR_DEADLINE, &deadline, &size);
  if (err < 0)
    RAISE_SOCK_ERROR; 

  return rb_funcall(INT2NUM(deadline), rb_intern("/"), 1, rb_float_new(1000));  
}

static VALUE
nanomsg_terminate(VALUE self)
{
  nn_term(); 

  return Qnil; 
}

struct device_op {
  int sa, sb; 
  int err; 
};

static void*
nanomsg_run_device_no_gvl(void* data)
{
  struct device_op *pop = (struct device_op*) data;

  pop->err = nn_device(pop->sa, pop->sb);

  return (void*) 0; 
}

static VALUE
nanomsg_run_device(VALUE self, VALUE a, VALUE b)
{
  struct device_op dop; 

  dop.sa = sock_get(a);
  dop.sb = sock_get(b);

  WITHOUT_GVL(nanomsg_run_device_no_gvl, &dop, RUBY_UBF_IO, 0);
  if (dop.err < 0)
    RAISE_SOCK_ERROR;

  return Qnil; 
}

static VALUE
nanomsg_run_loopback(VALUE self, VALUE a)
{
  struct device_op dop; 

  dop.sa = sock_get(a);
  dop.sb = -1;          // invalid socket, see documentation

  WITHOUT_GVL(nanomsg_run_device_no_gvl, &dop, RUBY_UBF_IO, 0);
  if (dop.err < 0)
    RAISE_SOCK_ERROR;

  return Qnil; 
}

void
Init_nanomsg_ext(void)
{
  mNanoMsg = rb_define_module("NanoMsg"); 
  cSocket = rb_define_class_under(mNanoMsg, "Socket", rb_cObject);
  cPairSocket = rb_define_class_under(mNanoMsg, "PairSocket", cSocket);
  
  cReqSocket = rb_define_class_under(mNanoMsg, "ReqSocket", cSocket);
  cRepSocket = rb_define_class_under(mNanoMsg, "RepSocket", cSocket);
  
  cPubSocket = rb_define_class_under(mNanoMsg, "PubSocket", cSocket);
  cSubSocket = rb_define_class_under(mNanoMsg, "SubSocket", cSocket);

  cSurveySocket = rb_define_class_under(mNanoMsg, "SurveySocket", cSocket);
  cRespondSocket = rb_define_class_under(mNanoMsg, "RespondSocket", cSocket);

  cPushSocket = rb_define_class_under(mNanoMsg, "PushSocket", cSocket);
  cPullSocket = rb_define_class_under(mNanoMsg, "PullSocket", cSocket);

  cBusSocket = rb_define_class_under(mNanoMsg, "BusSocket", cSocket);

  ceSocketError = rb_define_class_under(mNanoMsg, "SocketError", rb_eIOError);

  rb_define_singleton_method(mNanoMsg, "terminate", nanomsg_terminate, 0);
  rb_define_singleton_method(mNanoMsg, "run_device", nanomsg_run_device, 2);
  rb_define_singleton_method(mNanoMsg, "run_loopback", nanomsg_run_loopback, 1);

  rb_define_alloc_func(cSocket, sock_alloc);
  rb_define_method(cSocket, "bind", sock_bind, 1);
  rb_define_method(cSocket, "connect", sock_connect, 1);
  rb_define_method(cSocket, "send", sock_send, 1);
  rb_define_method(cSocket, "recv", sock_recv, 0);
  rb_define_method(cSocket, "close", sock_close, 0);
  rb_define_method(cSocket, "setsockopt", sock_setsockopt, 3);
  rb_define_method(cSocket, "getsockopt", sock_getsockopt, 2);

  rb_define_method(cPairSocket, "initialize", pair_sock_init, -1);

  rb_define_method(cReqSocket, "initialize", req_sock_init, -1);
  rb_define_method(cRepSocket, "initialize", rep_sock_init, -1);

  rb_define_method(cPubSocket, "initialize", pub_sock_init, -1);
  rb_define_method(cSubSocket, "initialize", sub_sock_init, -1);
  rb_define_method(cSubSocket, "subscribe", sub_sock_subscribe, 1);
  rb_define_method(cSubSocket, "unsubscribe", sub_sock_unsubscribe, 1);

  rb_define_method(cSurveySocket, "initialize", srvy_sock_init, -1);
  rb_define_method(cSurveySocket, "deadline=", srvy_set_deadline, 1);
  rb_define_method(cSurveySocket, "deadline", srvy_get_deadline, 0);
  rb_define_method(cRespondSocket, "initialize", resp_sock_init, -1);

  rb_define_method(cPushSocket, "initialize", push_sock_init, -1);
  rb_define_method(cPullSocket, "initialize", pull_sock_init, -1);

  rb_define_method(cBusSocket, "initialize", bus_sock_init, -1);

  Init_constants(mNanoMsg);
}

