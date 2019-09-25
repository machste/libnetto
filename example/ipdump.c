#include <netto.h>
#include <masc.h>


int main(int argc, char *argv[])
{
	int ret = -1;
	// Setup logging
	log_init(LOG_INFO);
	log_add_console();
	// Setup netlink socket
	NlSocket *sock = new(NlSocket, NETLINK_ROUTE);
	if (!nlsocket_connect(sock, NLSOCKET_PORT_AUTO, NLSOCKET_GROUP_NONE)) {
		log_error("unable to connect to netlink socket!");
		goto cleanup;
	}
	// Get data of all IP addresses
	Map *addrs = ipaddr_get(sock, AF_UNSPEC);
	if (addrs == NULL) {
		log_error("unable to get IP addresses!");
		goto cleanup;
	}
	// Print data of all IP addresses
	pretty_print(addrs);
	delete(addrs);
	ret = 0;
cleanup:
	delete(sock);
	return ret;
}
