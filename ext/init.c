
#include "ruby/ruby.h"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pair.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/survey.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/bus.h>

#include "constants.h"

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
  int abort; 
};

static VALUE
sock_send_no_gvl(void* data)
{
  struct ioop *pio = data; 

  // TODO This is buggy. I cannot make the difference between 
  // 'socket gone away' (=EAGAIN) and 'socket busy' (=EAGAIN). So I err on the
  // side of 'socket busy' and do not raise the error. As a consequence, we'll
  // get stuck in an endless loop when the socket is just not answering. 

  while (pio->nn_errno == EAGAIN && !pio->abort) {
    pio->return_code = nn_send(pio->sock, pio->buffer, pio->len, NN_DONTWAIT /* flags */);

    if (pio->return_code < 0)
      pio->nn_errno = nn_errno(); 
    else
      break;
  }

  return Qnil;
}

static void
sock_send_abort(void* data)
{
  struct ioop *pio = data; 
  pio->abort = Qtrue;  
}

static VALUE
sock_send(VALUE socket, VALUE buffer)
{
  struct ioop io; 

  io.sock = sock_get(socket);
  io.nn_errno = EAGAIN;
  io.buffer = StringValuePtr(buffer);
  io.len = RSTRING_LEN(buffer);
  io.abort = Qfalse; 

  rb_thread_blocking_region(sock_send_no_gvl, &io, sock_send_abort, &io);

  // Unclear what to do in this situation, but we'll simply return nil, 
  // leaving Ruby to handle the abort. 
  if (io.abort) 
    return Qnil; 

  if (io.return_code < 0)
    sock_raise_error(io.nn_errno);

  return INT2NUM(io.return_code);
}

static VALUE
sock_recv_no_gvl(void* data)
{
  struct ioop *pio = data; 

  // TODO This is buggy. I cannot make the difference between 
  // 'socket gone away' (=EAGAIN) and 'socket busy' (=EAGAIN). So I err on the
  // side of 'socket busy' and do not raise the error. As a consequence, we'll
  // get stuck in an endless loop when the socket is just not answering. 

  while (pio->nn_errno == EAGAIN && !pio->abort) {
    pio->return_code = nn_recv(pio->sock, &pio->buffer, NN_MSG, NN_DONTWAIT /* flags */);

    if (pio->return_code < 0)
      pio->nn_errno = nn_errno(); 
    else
      break;
  }

  return Qnil; 
}

static void
sock_recv_abort(void* data) 
{
  struct ioop *pio = data; 

  pio->abort = Qtrue; 
}

static VALUE
sock_recv(VALUE socket)
{
  VALUE result; 
  struct ioop io; 

  io.sock = sock_get(socket);
  io.buffer = (char*) 0; 
  io.abort  = Qfalse; 
  io.nn_errno = EAGAIN;

  rb_thread_blocking_region(sock_recv_no_gvl, &io, sock_recv_abort, &io); 

  if (io.abort) 
    return Qnil; 

  if (io.return_code < 0)
    sock_raise_error(io.nn_errno);

  result = rb_str_new(io.buffer, io.return_code);
  nn_freemsg(io.buffer); io.buffer = (char*) 0;

  return result;
}

static VALUE
sock_close_no_gvl(void* data)
{
  struct ioop *pio = (struct ioop*) data; 

  pio->return_code = nn_close(pio->sock);
  if (pio->return_code < 0)
    pio->nn_errno = nn_errno();

  return Qnil;
}

static VALUE
sock_close(VALUE socket)
{
  struct ioop io; 

  io.sock = sock_get(socket);

  // I've no idea on how to abort a close (which may block for NN_LINGER 
  // seconds), so we'll be uninterruptible. 
  rb_thread_blocking_region(sock_close_no_gvl, &io, NULL, NULL);

  if (io.return_code < 0)
    sock_raise_error(io.nn_errno);

  return Qnil;
}

static VALUE 
pair_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_PAIR);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE 
req_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_REQ);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE 
rep_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_REP);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE 
pub_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_PUB);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE 
sub_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_SUB);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE
sub_sock_subscribe(VALUE socket, VALUE channel)
{
  int sock = sock_get(socket);
  int err; 

  err = nn_setsockopt(sock, NN_SUB, NN_SUB_SUBSCRIBE, 
    StringValuePtr(channel), 
    RSTRING_LEN(channel));

  if (err < 0) 
    RAISE_SOCK_ERROR;

  return socket;
}

static VALUE 
srvy_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_SURVEYOR);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

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
resp_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_RESPONDENT);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE 
push_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_PUSH);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE 
pull_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_PULL);
  if (psock->socket < 0) {
    RAISE_SOCK_ERROR;
  }

  return socket; 
}

static VALUE 
bus_sock_init(int argc, VALUE *argv, VALUE self)
{
  VALUE domain; 

  rb_scan_args(argc, argv, "01", &domain);

  if (NIL_P(domain)) 
    sock_init(self, AF_SP, NN_BUS); 
  else
    sock_init(self, FIX2INT(domain), NN_BUS);

  return self; 
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

static VALUE
nanomsg_run_device_no_gvl(void* data)
{
  struct device_op *pop = (struct device_op*) data;

  pop->err = nn_device(pop->sa, pop->sb);

  return Qnil; 
}

static VALUE
nanomsg_run_device(VALUE self, VALUE a, VALUE b)
{
  struct device_op dop; 

  dop.sa = sock_get(a);
  dop.sb = sock_get(b);

  rb_thread_blocking_region(nanomsg_run_device_no_gvl, &dop, NULL, NULL);
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

  rb_thread_blocking_region(nanomsg_run_device_no_gvl, &dop, NULL, NULL);
  if (dop.err < 0)
    RAISE_SOCK_ERROR;

  return Qnil; 
}

void
Init_nanomsg(void)
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

  rb_define_method(cPairSocket, "initialize", pair_sock_init, 0);

  rb_define_method(cReqSocket, "initialize", req_sock_init, 0);
  rb_define_method(cRepSocket, "initialize", rep_sock_init, 0);

  rb_define_method(cPubSocket, "initialize", pub_sock_init, 0);
  rb_define_method(cSubSocket, "initialize", sub_sock_init, 0);
  rb_define_method(cSubSocket, "subscribe", sub_sock_subscribe, 1);

  rb_define_method(cSurveySocket, "initialize", srvy_sock_init, 0);
  rb_define_method(cSurveySocket, "deadline=", srvy_set_deadline, 1);
  rb_define_method(cSurveySocket, "deadline", srvy_get_deadline, 0);
  rb_define_method(cRespondSocket, "initialize", resp_sock_init, 0);

  rb_define_method(cPushSocket, "initialize", push_sock_init, 0);
  rb_define_method(cPullSocket, "initialize", pull_sock_init, 0);

  rb_define_method(cBusSocket, "initialize", bus_sock_init, -1);

  Init_constants(mNanoMsg);
}

