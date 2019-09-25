#include <errno.h>
#include <sys/socket.h>

#include <netto/nlsocket.h>


void nlsocket_init(NlSocket *self, int bus)
{
	object_init(self, NlSocketCls);
	self->fd = socket(AF_NETLINK, SOCK_RAW, bus);
	self->addr.nl_family = AF_NETLINK;
}

static void _vinit(NlSocket *self, va_list va)
{
	int bus = va_arg(va, int);
	nlsocket_init(self, bus);
}

void nlsocket_destroy(NlSocket *self)
{
	close(self);
}

bool nlsocket_connect(NlSocket *self, int port, int groups)
{
	self->addr.nl_groups = groups;
	self->addr.nl_pid = port;
	// Bind the netlink socket
	if (bind(self->fd, (struct sockaddr *)&self->addr,
			sizeof(self->addr)) < 0) {
		return false;
	}
	socklen_t addr_len = sizeof(self->addr);
	if (getsockname(self->fd, (struct sockaddr *)&self->addr, &addr_len) < 0) {
		return false;
	}
	if (addr_len != sizeof(self->addr)) {
		errno = EINVAL;
		return false;
	}
	if (self->addr.nl_family != AF_NETLINK) {
		self->addr.nl_family = AF_NETLINK;
		errno = EINVAL;
		return false;
	}
	return true;
}

ssize_t nlsocket_read(NlSocket *self, void *data, size_t size)
{
	struct sockaddr_nl addr;
	struct iovec iov = { .iov_base = data, .iov_len = size };
	struct msghdr msg = {
		.msg_name = &addr,
		.msg_namelen = sizeof(addr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0
	};
	// Receive a netlink message
	ssize_t ret = recvmsg(self->fd, &msg, 0);
	if (ret < 0) {
		return -1;
	}
	if (msg.msg_flags & MSG_TRUNC) {
		errno = ENOSPC;
		return -1;
	}
	if (msg.msg_namelen != sizeof(addr)) {
		errno = EINVAL;
		return -1;
	}
	return ret;
}

ssize_t nlsocket_write(NlSocket *self, const void *data, size_t size)
{
	static const struct sockaddr_nl a = {
		.nl_family = AF_NETLINK
	};
	return sendto(self->fd, data, size, 0, (struct sockaddr *)&a, sizeof(a));
}

static io_class _NlSocketCls = {
	.name = "NlSocket",
	.size = sizeof(NlSocket),
	.vinit = (vinit_cb)_vinit,
	.init_copy = (init_copy_cb)io_init_copy,
	.destroy = (destroy_cb)io_destroy,
	.cmp = (cmp_cb)io_cmp,
	.repr = (repr_cb)io_to_cstr,
	.to_cstr = (to_cstr_cb)io_to_cstr,
	// Io Class
	.get_fd = (get_fd_cb)io_get_fd,
	.__read__ = (read_cb)nlsocket_read,
	.readstr = (readstr_cb)io_readstr,
	.readline = (readline_cb)io_readline,
	.__write__ = (write_cb)nlsocket_write,
	.__close__ = (close_cb)io_close,
};

const io_class *NlSocketCls = &_NlSocketCls;
