#include <string.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <masc/int.h>
#include <masc/ip.h>
#include <masc/list.h>
#include <masc/net.h>
#include <masc/macro.h>
#include <masc/str.h>
#include <net/if.h>
#include <sys/socket.h>


#include <netto/ipaddr.h>
#include <netto/nlsocket.h>

//TODO: These includes should be removed later
#include <time.h>
#include <masc/print.h>

#define BUFFER_SIZE 4096


typedef enum {
	NLMSG_STATE_UNKNOWN,
    NLMSG_STATE_ONGOING,
    NLMSG_STATE_DONE,
    NLMSG_STATE_ERROR,
} NlMsgState;


static struct {
	uint32_t flag;
	const char *name;
} _flag_name_map[] = {
	{ IFA_F_SECONDARY, "secondary" },
	{ IFA_F_NODAD, "nodad" },
	{ IFA_F_OPTIMISTIC, "optimistic" },
	{ IFA_F_DADFAILED, "dadfailed" },
	{ IFA_F_HOMEADDRESS, "home-address" },
	{ IFA_F_DEPRECATED, "deprecated" },
	{ IFA_F_TENTATIVE, "tentative" },
	{ IFA_F_PERMANENT, "permanent" },
	{ IFA_F_MANAGETEMPADDR, "manage-temp-address" },
	{ IFA_F_NOPREFIXROUTE, "no-prefix-route" },
	{ IFA_F_MCAUTOJOIN, "mc-auto-join" },
	{ IFA_F_STABLE_PRIVACY, "stable-privacy" }
};

static List *ipaddr_get_flag_list(uint32_t flags)
{
	List *flag_list = list_new();
	for (int i = 0; i < ARRAY_LEN(_flag_name_map); i++) {
		if (flags & _flag_name_map[i].flag) {
			list_append(flag_list, str_new_cstr(_flag_name_map[i].name));
		}
	}
	return flag_list;
}


static bool nlmsg_parse(Map *msg, void *data, size_t size)
{
	struct ifaddrmsg *ifa = data;
	uint32_t flags = ifa->ifa_flags;
	Map *addr = map_new();
	// Add device name
	char dev_name[IF_NAMESIZE];
	if_indextoname(ifa->ifa_index, dev_name);
	map_set(addr, "dev_name", str_new_cstr(dev_name));
	// Add device index
	map_set(addr, "dev_index", int_new(ifa->ifa_index));
	// Add address family
	map_set(addr, "family", str_new(net_af_to_cstr(ifa->ifa_family)));
	// Add prefix length
	map_set(addr, "prefixlen", int_new(ifa->ifa_prefixlen));
	// Parse route table attributes
	struct rtattr *rta = (void *)((uint8_t *)ifa + sizeof(struct ifaddrmsg));
	size -= RTA_ALIGN(sizeof(struct ifaddrmsg));
	while (RTA_OK(rta, size)) {
		if (rta->rta_type == IFA_LABEL) {
			map_set(addr, "label", str_new_cstr(RTA_DATA(rta)));
		} else if (rta->rta_type == IFA_ADDRESS) {
			Ip *ip = ip_new_bin(ifa->ifa_family, ifa->ifa_prefixlen,
					RTA_DATA(rta));
			// Add IP address
			map_set(addr, "addr", ip);
		} else if (rta->rta_type == IFA_BROADCAST) {
			Ip *brd = ip_new_bin(ifa->ifa_family, ifa->ifa_prefixlen,
					RTA_DATA(rta));
			map_set(addr, "brd", brd);
		} else if (rta->rta_type == IFA_LOCAL) {
			Ip *local = ip_new_bin(ifa->ifa_family, ifa->ifa_prefixlen,
					RTA_DATA(rta));
			map_set(addr, "local", local);
		} else if (rta->rta_type == IFA_ANYCAST) {
			Ip *anycast = ip_new_bin(ifa->ifa_family, ifa->ifa_prefixlen,
					RTA_DATA(rta));
			map_set(addr, "anycast", anycast);
		} else if (rta->rta_type == IFA_CACHEINFO) {
			Map *cache = map_new();
			struct ifa_cacheinfo *ci = RTA_DATA(rta);
			map_set(cache, "valid", int_new(ci->ifa_valid));
			map_set(cache, "preferred", int_new(ci->ifa_prefered));
			map_set(cache, "cstamp", int_new(ci->cstamp));
			map_set(cache, "tstamp", int_new(ci->tstamp));
			map_set(addr, "cache", cache);
		} else if (rta->rta_type == IFA_FLAGS) {
			// Override flags of the ifaddrmsg struct
			flags = *(uint32_t *)RTA_DATA(rta);
		}
		rta = RTA_NEXT(rta, size);
	}
	// Add flags
	map_set(addr, "flags", ipaddr_get_flag_list(flags));
	// Add address map to addresses list
	List *addrs = map_get(msg, "addrs");
	if (addrs == NULL) {
		addrs = list_new();
		map_set(msg, "addrs", addrs);
	}
	list_append(addrs, addr);
	return true;
}

static NlMsgState nlmsg_parse_data(Map *msg, void *data, size_t size)
{
	NlMsgState ret = NLMSG_STATE_ERROR;
	struct nlmsghdr *nl_hdr = data;
	while (NLMSG_OK(nl_hdr, size)) {
		if (nl_hdr->nlmsg_type == NLMSG_DONE) {
			return NLMSG_STATE_DONE;
		}
		if (nl_hdr->nlmsg_type == NLMSG_ERROR) {
			return NLMSG_STATE_ERROR;
		}
		if (!nlmsg_parse(msg, NLMSG_DATA(nl_hdr), NLMSG_PAYLOAD(nl_hdr, 0))) {
			return NLMSG_STATE_ERROR;
		}
		if (nl_hdr->nlmsg_flags & NLM_F_MULTI) {
			ret = NLMSG_STATE_ONGOING;
		} else {
			ret = NLMSG_STATE_DONE;
		}
		nl_hdr = NLMSG_NEXT(nl_hdr, size);
	}
	return ret;
}

Map *ipaddr_get(NlSocket *sock, uint8_t family)
{
	uint8_t data[BUFFER_SIZE];
	// Create netlink header
	struct nlmsghdr *nl_hdr = (void *)data;
	memset(nl_hdr, 0, NLMSG_HDRLEN);
	nl_hdr->nlmsg_type	= RTM_GETADDR;
	nl_hdr->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nl_hdr->nlmsg_seq = time(NULL);
	nl_hdr->nlmsg_len = NLMSG_HDRLEN;
	// Create route table message
	struct ifaddrmsg *ifa = (void *)((uint8_t *)nl_hdr + nl_hdr->nlmsg_len);
	size_t size = NLMSG_ALIGN(sizeof(struct ifaddrmsg));
	nl_hdr->nlmsg_len += size;
	memset(ifa, 0, size);
	ifa->ifa_family = family;
	// Send netlink message
	write(sock, data, nl_hdr->nlmsg_len);
	// Receive netlink messages
	Map *msg = map_new();
	NlMsgState msg_state = NLMSG_STATE_UNKNOWN;
	do {
		size = read(sock, data, sizeof(data));
		if (size <= 0) {
			msg_state = NLMSG_STATE_ERROR;
		}
		msg_state = nlmsg_parse_data(msg, data, size);
	} while (msg_state == NLMSG_STATE_ONGOING);
	// If something went wrong, delete invalid message
	if (msg_state != NLMSG_STATE_DONE) {
		map_delete(msg);
		msg = NULL;
	}
	return msg;
}
