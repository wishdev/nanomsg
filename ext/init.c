
#include "ruby/ruby.h"

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pair.h>
#include <nanomsg/pubsub.h>

#include "constants.h"

static VALUE mNanoMsg;
static VALUE cSocket; 

static VALUE cPairSocket; 

static VALUE cReqSocket;
static VALUE cRepSocket;

static VALUE cPubSocket;
static VALUE cSubSocket;

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
nanomsg_terminate(VALUE self)
{
  nn_term(); 

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

  ceSocketError = rb_define_class_under(mNanoMsg, "SocketError", rb_eIOError);

  rb_define_singleton_method(mNanoMsg, "terminate", nanomsg_terminate, 0);

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

  Init_constants(mNanoMsg);
}

