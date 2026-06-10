/**
 * LeapOS net-probe — libbsd bring-up (re0 Realtek, em0/em1 Intel GbE).
 */

#include <rtems.h>
#include <rtems/bsd/bsd.h>

#include <machine/rtems-bsd-commands.h>

#include <ifaddrs.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static void
list_ifaces(void)
{
	struct ifaddrs *ifap;
	struct ifaddrs *ifa;

	if (getifaddrs(&ifap) != 0) {
		printf("getifaddrs failed: %s\n", strerror(errno));
		return;
	}

	printf("interfaces (getifaddrs):\n");
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		printf("  %s flags=0x%lx", ifa->ifa_name,
		    (unsigned long)ifa->ifa_flags);
		if ((ifa->ifa_flags & IFF_UP) != 0) {
			printf(" UP");
		}
		if ((ifa->ifa_flags & IFF_RUNNING) != 0) {
			printf(" RUNNING");
		}
		if (ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_LINK) {
			const struct sockaddr_dl *sdl;

			sdl = (const struct sockaddr_dl *)ifa->ifa_addr;
			if (sdl->sdl_alen == ETHER_ADDR_LEN) {
				const uint8_t *ea = LLADDR(sdl);

				printf(" hw=%02x:%02x:%02x:%02x:%02x:%02x",
				    ea[0], ea[1], ea[2], ea[3], ea[4], ea[5]);
			}
		}
		if (ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_INET) {
			char buf[INET_ADDRSTRLEN];

			inet_ntop(AF_INET,
			    &((const struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
			    buf, sizeof(buf));
			printf(" inet=%s", buf);
		}
		printf("\n");
	}
	freeifaddrs(ifap);
}

static int
iface_flags(const char *ifname, unsigned int *flags)
{
	struct ifreq ifr;
	int s;
	int error;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		printf("socket(AF_INET): %s\n", strerror(errno));
		return -1;
	}

	error = ioctl(s, SIOCGIFFLAGS, &ifr);
	if (error == 0) {
		*flags = (ifr.ifr_flags & 0xffffU) |
		    ((unsigned int)ifr.ifr_flagshigh << 16);
	} else {
		printf("SIOCGIFFLAGS %s: %s\n", ifname, strerror(errno));
	}

	close(s);
	return error;
}

static int
iface_set_up(const char *ifname)
{
	struct ifreq ifr;
	unsigned int flags;
	int s;
	int error;

	if (iface_flags(ifname, &flags) != 0) {
		return -1;
	}

	flags |= IFF_UP;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_flags = flags & 0xffff;
	ifr.ifr_flagshigh = flags >> 16;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		printf("socket(AF_INET): %s\n", strerror(errno));
		return -1;
	}

	error = ioctl(s, SIOCSIFFLAGS, &ifr);
	if (error != 0) {
		printf("SIOCSIFFLAGS %s: %s\n", ifname, strerror(errno));
	}

	close(s);
	return error;
}

static void
fill_sin(struct sockaddr_in *sin, const struct in_addr *in)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = *in;
}

static int
iface_set_ipv4_once(const char *ifname, const char *addr, const char *mask)
{
	struct in_aliasreq ifra;
	struct in_addr in_addr;
	struct in_addr in_mask;
	int s;
	int error;

	if (inet_pton(AF_INET, addr, &in_addr) != 1) {
		printf("invalid address %s\n", addr);
		return -1;
	}
	if (inet_pton(AF_INET, mask, &in_mask) != 1) {
		printf("invalid netmask %s\n", mask);
		return -1;
	}

	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	fill_sin(&ifra.ifra_addr, &in_addr);
	fill_sin(&ifra.ifra_mask, &in_mask);
	fill_sin(&ifra.ifra_broadaddr, &in_addr);
	ifra.ifra_broadaddr.sin_addr.s_addr =
	    in_addr.s_addr | ~in_mask.s_addr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		printf("socket(AF_INET): %s\n", strerror(errno));
		return -1;
	}

	error = ioctl(s, SIOCAIFADDR, &ifra);
	if (error != 0) {
		printf("SIOCAIFADDR %s: %s\n", ifname, strerror(errno));
	}

	close(s);
	return error;
}

static int
iface_set_ipv4(const char *ifname, const char *addr, const char *mask)
{
	int attempt;

	for (attempt = 0; attempt < 10; ++attempt) {
		if (iface_set_ipv4_once(ifname, addr, mask) == 0) {
			return 0;
		}
		if (attempt + 1 < 10) {
			printf("%s: retry IPv4 assign in 1s (%d/10)\n", ifname,
			    attempt + 2);
			sleep(1);
		}
	}
	return -1;
}

static int
iface_has_ipv4(const char *ifname, const char *addr)
{
	struct ifaddrs *ifap;
	struct ifaddrs *ifa;
	struct in_addr want;
	int found;

	if (inet_pton(AF_INET, addr, &want) != 1) {
		return 0;
	}

	if (getifaddrs(&ifap) != 0) {
		return 0;
	}

	found = 0;
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		const struct sockaddr_in *sin;

		if (strcmp(ifa->ifa_name, ifname) != 0) {
			continue;
		}
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_INET) {
			continue;
		}
		sin = (const struct sockaddr_in *)ifa->ifa_addr;
		if (sin->sin_addr.s_addr == want.s_addr) {
			found = 1;
			break;
		}
	}

	freeifaddrs(ifap);
	return found;
}

static int
iface_media_link_up(const char *ifname, bool *link_up)
{
	struct ifmediareq ifmr;
	int s;
	int error;

	*link_up = false;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		return -1;
	}

	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	error = ioctl(s, SIOCGIFMEDIA, &ifmr);
	close(s);

	if (error != 0) {
		return -1;
	}

	if ((ifmr.ifm_status & IFM_AVALID) != 0) {
		*link_up = (ifmr.ifm_status & IFM_ACTIVE) != 0;
	}

	return 0;
}

static int
iface_is_up_running(const char *ifname)
{
	unsigned int flags;

	if (iface_flags(ifname, &flags) != 0) {
		return 0;
	}

	return ((flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING));
}

static int
wait_for_running(const char *ifname, int seconds)
{
	int i;

	for (i = 0; i < seconds; ++i) {
		if (iface_is_up_running(ifname)) {
			printf("%s: IFF_RUNNING set (after %ds)\n", ifname, i);
			return 0;
		}
		sleep(1);
	}
	printf("%s: IFF_RUNNING not set after %ds\n", ifname, seconds);
	return -1;
}

static void
wait_for_link(const char *ifname, int seconds)
{
	int i;

	for (i = 0; i < seconds; ++i) {
		bool link_up = false;

		if (iface_media_link_up(ifname, &link_up) == 0 && link_up) {
			printf("%s: carrier detected (after %ds)\n", ifname, i);
			fflush(stdout);
			return;
		}
		sleep(1);
	}
	printf("%s: no carrier after %ds (may still ping on L2/L3)\n", ifname,
	    seconds);
	fflush(stdout);
}

static int
probe_interface(const char *ifname)
{
	unsigned int flags;
	static const char *addr = "192.168.1.2";
	static const char *mask = "255.255.255.0";

	printf("%s: if_nametoindex=%u\n", ifname, if_nametoindex(ifname));
	fflush(stdout);

	if (iface_flags(ifname, &flags) == 0) {
		printf("%s: kernel flags=0x%x\n", ifname, flags);
		fflush(stdout);
	}

	if (iface_set_up(ifname) != 0) {
		return 0;
	}
	printf("%s: UP via SIOCSIFFLAGS\n", ifname);
	fflush(stdout);

	if (wait_for_running(ifname, 10) != 0) {
		return 0;
	}

	wait_for_link(ifname, 5);

	if (iface_set_ipv4(ifname, addr, mask) == 0 &&
	    iface_has_ipv4(ifname, addr)) {
		printf("%s: IPv4 %s/24 assigned\n", ifname, addr);
		printf("Ping this board from another host: ping %s\n", addr);
	} else {
		printf("%s: WARNING — IPv4 assign failed (L2 still up)\n", ifname);
	}

	fflush(stdout);

	if (iface_is_up_running(ifname)) {
		printf("\n*** SUCCESS: %s is UP RUNNING ***\n\n", ifname);
		fflush(stdout);
		return 1;
	}

	printf("%s: UP set but not RUNNING\n", ifname);
	return 0;
}

static void
probe_interfaces(void)
{
	static const char *candidates[] = { "re0", "em0", "em1", NULL };
	int i;
	int any_up = 0;

	printf("Probing: re0 (Realtek), em0/em1 (Intel GbE)\n\n");
	list_ifaces();

	for (i = 0; candidates[i] != NULL; ++i) {
		printf("\n--- try %s ---\n", candidates[i]);
		if (probe_interface(candidates[i])) {
			any_up = 1;
		} else {
			printf("%s not usable\n", candidates[i]);
		}
	}

	printf("\n");
	list_ifaces();

	if (!any_up) {
		printf("FAIL: no interface came up — check cable / NIC in BIOS\n");
	} else {
		printf("At least one NIC is UP RUNNING (re0 and/or em0/em1).\n");
		printf("(ifconfig/netstat CLI may still exit non-zero on pc386 — ignore if UP RUNNING above.)\n");
	}
}

rtems_task
Init(rtems_task_argument ignored)
{
	rtems_status_code sc;

	(void) ignored;

	printf(
		"\n"
		"*** LeapOS net-probe ***\n"
		"Network stack bring-up (libbsd)\n\n"
	);

	sc = rtems_bsd_initialize();
	if (sc != RTEMS_SUCCESSFUL) {
		printf("network init failed: %s\n", rtems_status_text(sc));
		exit(1);
	}

	sc = rtems_task_wake_after(2);
	if (sc != RTEMS_SUCCESSFUL) {
		exit(1);
	}

	if (rtems_bsd_ifconfig_lo0() != EX_OK) {
		printf("warning: lo0 setup failed\n");
	}

	sleep(3);
	probe_interfaces();

	printf(
	    "\n*** LeapOS net-probe complete — staying up (ping 192.168.1.2) ***\n");
	fflush(stdout);

	for (;;) {
		sleep(3600);
	}
}

#define RTEMS_BSD_CONFIG_DOMAIN_PAGE_MBUFS_SIZE (64 * 1024 * 1024)
#define RTEMS_BSD_CONFIG_NET_PF_UNIX
#define RTEMS_BSD_CONFIG_NET_IP_MROUTE
#define RTEMS_BSD_CONFIG_NET_IF_BRIDGE
#define RTEMS_BSD_CONFIG_NET_IF_LAGG
#define RTEMS_BSD_CONFIG_NET_IF_VLAN
#define RTEMS_BSD_CONFIG_BSP_CONFIG
#define RTEMS_BSD_CONFIG_INIT

#include <machine/rtems-bsd-config.h>

#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_STUB_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_ZERO_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_LIBBLOCK

#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 64
#define CONFIGURE_MAXIMUM_USER_EXTENSIONS 1
#define CONFIGURE_UNLIMITED_ALLOCATION_SIZE 32
#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_BDBUF_BUFFER_MAX_SIZE (64 * 1024)
#define CONFIGURE_BDBUF_MAX_READ_AHEAD_BLOCKS 4
#define CONFIGURE_BDBUF_CACHE_MEMORY_SIZE (1 * 1024 * 1024)

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_INIT_TASK_STACK_SIZE (128 * 1024)
#define CONFIGURE_INIT_TASK_INITIAL_MODES RTEMS_DEFAULT_MODES
#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT
#define CONFIGURE_INIT

#include <rtems/confdefs.h>
