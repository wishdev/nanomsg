
#include "ruby/ruby.h"

#include <nanomsg/nn.h>
#include <nanomsg/pair.h>

static VALUE cNanoMsg;
static VALUE cSocket; 
static VALUE cPairSocket; 
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
sock_raise_error(int code)
{
  printf("socket error %d errno %d\n", code, errno);

  // TODO allow querying of the errno at the very least.
  switch (errno) {
    case EBADF:
      rb_raise(ceSocketError, "The provided socket is invalid.");
      break; 
    case ENOTSUP:
      rb_raise(ceSocketError, "The operation is not supported by this socket type.");
      break; 
    case EFSM:
      rb_raise(ceSocketError, "The operation cannot be performed on this socket at the moment because the socket is not in the appropriate state.");
      break; 
    case EAGAIN: 
      rb_raise(ceSocketError, "Non-blocking mode was requested and the message cannot be sent at the moment.");
      break; 
    case EINTR: 
      rb_raise(ceSocketError, "The operation was interrupted by delivery of a signal before the message was sent.");
      break; 
    case ETIMEDOUT: 
      rb_raise(ceSocketError, "Individual socket types may define their own specific timeouts. If such timeout is hit, this error will be returned.");
      break; 
    case EAFNOSUPPORT: 
      rb_raise(ceSocketError, "Specified address family is not supported.");
      break; 
    case EINVAL: 
      rb_raise(ceSocketError, "Unknown protocol.");
      break; 
    case EMFILE: 
      rb_raise(ceSocketError, "The limit on the total number of open SP sockets or OS limit for file descriptors has been reached.");
      break; 
    case ETERM: 
      rb_raise(ceSocketError, "The library is terminating.");
      break; 
    default: 
      rb_raise(ceSocketError, "Unknown error code %d", errno);
  }
}

static VALUE
sock_bind(VALUE socket, VALUE bind)
{
  int sock = sock_get(socket);
  int endpoint; 

  endpoint = nn_bind(sock, StringValueCStr(bind));
  if (endpoint < 0) 
    sock_raise_error(endpoint); 

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
    sock_raise_error(endpoint); 

  // TODO do something with the endpoint, returning it in a class for example. 
  return Qnil; 
}

struct ioop {
  int last_code; 
  int sock; 
  char* buffer; 
  long len; 
  int abort; 
};

static VALUE
sock_send_no_gvl(void* data)
{
  struct ioop *pio = data; 

  while (pio->last_code == EAGAIN && !pio->abort) {
    pio->last_code = nn_send(pio->sock, pio->buffer, pio->len, NN_DONTWAIT /* flags */);

    if (pio->last_code < 0)
      pio->last_code = errno; 
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
  io.last_code = EAGAIN; 
  io.buffer = StringValuePtr(buffer);
  io.len = RSTRING_LEN(buffer);
  io.abort = Qfalse; 

  rb_thread_blocking_region(sock_send_no_gvl, &io, sock_send_abort, &io);

  // Unclear what to do in this situation, but we'll simply return nil, 
  // leaving Ruby to handle the abort. 
  if (io.abort) 
    return Qnil; 

  if (io.last_code < 0)
    sock_raise_error(io.last_code);

  return INT2NUM(io.last_code);
}

static VALUE
sock_recv_no_gvl(void* data)
{
  struct ioop *pio = data; 

  while (pio->last_code == EAGAIN && !pio->abort) {
    pio->last_code = nn_recv(pio->sock, &pio->buffer, NN_MSG, NN_DONTWAIT /* flags */);

    if (pio->last_code < 0)
      pio->last_code = errno; 
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
  io.last_code = EAGAIN;

  rb_thread_blocking_region(sock_recv_no_gvl, &io, sock_recv_abort, &io); 

  if (io.abort) 
    return Qnil; 

  if (io.last_code < 0)
    sock_raise_error(io.last_code);

  result = rb_str_new(io.buffer, io.last_code);
  nn_freemsg(io.buffer); io.buffer = (char*) 0;

  return result;
}

static VALUE 
pair_sock_init(VALUE socket)
{
  struct nmsg_socket *psock = sock_get_ptr(socket); 

  psock->socket = nn_socket(AF_SP, NN_PAIR);
  if (psock->socket < 0) {
    sock_raise_error(psock->socket);
  }

  return socket; 
}

void
Init_nanomsg(void)
{
  printf("loading nanomsg extension\n");

  cNanoMsg = rb_define_module("NanoMsg"); 
  cSocket = rb_define_class_under(cNanoMsg, "Socket", rb_cObject);
  cPairSocket = rb_define_class_under(cNanoMsg, "PairSocket", cSocket);

  ceSocketError = rb_define_class_under(cNanoMsg, "SocketError", rb_eIOError);

  rb_define_method(cSocket, "bind", sock_bind, 1);
  rb_define_method(cSocket, "connect", sock_connect, 1);
  rb_define_method(cSocket, "send", sock_send, 1);
  rb_define_method(cSocket, "recv", sock_recv, 0);

  rb_define_alloc_func(cPairSocket, sock_alloc);
  rb_define_method(cPairSocket, "initialize", pair_sock_init, 0);
}

