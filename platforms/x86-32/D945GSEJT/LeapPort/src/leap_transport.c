/*
 * leap_transport.c — RTEMS libbsd BPF raw Ethernet transport.
 *
 * Copyright (c) 2026 Adam G. Sweeney <agsweeney@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "leap_transport.h"
#include "leap_config.h"
#include "leap_time.h"

#include "leap/leap_frame.h"
#include "leap/leap_protocol.h"

#include <rtems/bsd/util.h>
#include <rtems/bspIo.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LEAP_RTEMS_BPF_DEVICE "/dev/bpf"
#define LEAP_RTEMS_ETH_HDR_LEN 14u
#define LEAP_RTEMS_MIN_FRAME 60u

static int
iface_set_up(LeapRtemsTransport *transport)
{
	struct ifreq ifr;
	int error;

	if (transport == NULL || transport->ioctl_fd < 0)
	{
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, transport->ifname, sizeof(ifr.ifr_name));

	error = ioctl(transport->ioctl_fd, SIOCGIFFLAGS, &ifr);
	if (error == 0)
	{
		ifr.ifr_flags |= IFF_UP;
		error = ioctl(transport->ioctl_fd, SIOCSIFFLAGS, &ifr);
	}

	return error;
}

static int
iface_wait_running(LeapRtemsTransport *transport, int seconds)
{
	struct ifreq ifr;
	int i;

	if (transport == NULL || transport->ioctl_fd < 0)
	{
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, transport->ifname, sizeof(ifr.ifr_name));

	for (i = 0; i < seconds; ++i)
	{
		if (ioctl(transport->ioctl_fd, SIOCGIFFLAGS, &ifr) == 0 &&
		    (ifr.ifr_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
		{
			return 0;
		}

		sleep(1);
	}

	return -1;
}

static int
mac_is_broadcast(const uint8_t *mac)
{
	static const uint8_t bcast[LEAP_RTEMS_MAC_LEN] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

	return (memcmp(mac, bcast, LEAP_RTEMS_MAC_LEN) == 0);
}

static int
frame_dest_ok(const LeapRtemsTransport *transport, const uint8_t *dst_mac)
{
	if (memcmp(dst_mac, transport->local_mac, LEAP_RTEMS_MAC_LEN) == 0)
	{
		return 1;
	}

	return mac_is_broadcast(dst_mac);
}

static uint16_t
frame_ethertype(const uint8_t *frame)
{
	return (uint16_t)(((uint16_t)frame[12] << 8) | frame[13]);
}

static int
bpf_open_on_iface(const char *ifname, uint16_t ethertype)
{
	struct ifreq ifr;
	struct bpf_version pv;
	int fd;
	int buf_len;
	int rv;
	(void)ethertype;

	fd = open(LEAP_RTEMS_BPF_DEVICE, O_RDWR);
	if (fd < 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: open %s: %s" LEAP_ANSI_RESET "\n",
		    LEAP_RTEMS_BPF_DEVICE, strerror(errno));
		return -1;
	}

	rv = ioctl(fd, BIOCVERSION, &pv);
	if (rv != 0 || pv.bv_major != BPF_MAJOR_VERSION ||
	    pv.bv_minor < BPF_MINOR_VERSION)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: BPF version mismatch" LEAP_ANSI_RESET "\n");
		close(fd);
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	rv = ioctl(fd, BIOCSETIF, &ifr);
	if (rv != 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: BIOCSETIF %s: %s" LEAP_ANSI_RESET "\n",
		    ifname, strerror(errno));
		close(fd);
		return -1;
	}

	buf_len = 1;
	rv = ioctl(fd, BIOCIMMEDIATE, &buf_len);
	if (rv != 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: BIOCIMMEDIATE: %s" LEAP_ANSI_RESET "\n", strerror(errno));
		close(fd);
		return -1;
	}

	rv = ioctl(fd, BIOCGBLEN, &buf_len);
	if (rv != 0 || buf_len <= 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: BIOCGBLEN: %s" LEAP_ANSI_RESET "\n", strerror(errno));
		close(fd);
		return -1;
	}

	return fd;
}

int
leap_rtems_transport_init(
    LeapRtemsTransport *transport,
    const char *ifname,
    uint16_t ethertype)
{
	if (transport == NULL || ifname == NULL)
	{
		return -1;
	}

	memset(transport, 0, sizeof(*transport));
	transport->bpf_fd = -1;
	transport->ioctl_fd = -1;
	transport->bpf_buf_len = 0;
	transport->bpf_buf = NULL;
	transport->ethertype = ethertype;
	transport->link_up = 0;
	(void)strncpy(transport->ifname, ifname, sizeof(transport->ifname) - 1u);

	transport->ioctl_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (transport->ioctl_fd < 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: ioctl socket: %s" LEAP_ANSI_RESET "\n", strerror(errno));
		return -1;
	}

	if (iface_set_up(transport) != 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: failed to bring %s up" LEAP_ANSI_RESET "\n", ifname);
		leap_rtems_transport_close(transport);
		return -1;
	}

	if (iface_wait_running(transport, 10) != 0)
	{
		printf(LEAP_ANSI_WARN "LEAP transport: %s not RUNNING after 10s" LEAP_ANSI_RESET "\n", ifname);
	}

	if (rtems_bsd_get_ethernet_addr(ifname, transport->local_mac) != 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: MAC lookup failed on %s" LEAP_ANSI_RESET "\n", ifname);
		leap_rtems_transport_close(transport);
		return -1;
	}

	transport->bpf_fd = bpf_open_on_iface(ifname, ethertype);
	if (transport->bpf_fd < 0)
	{
		leap_rtems_transport_close(transport);
		return -1;
	}
	if (ioctl(transport->bpf_fd, BIOCGBLEN, &transport->bpf_buf_len) != 0 ||
	    transport->bpf_buf_len <= 0)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: BIOCGBLEN(open): %s" LEAP_ANSI_RESET "\n", strerror(errno));
		leap_rtems_transport_close(transport);
		return -1;
	}
	transport->bpf_buf = malloc((size_t)transport->bpf_buf_len);
	if (transport->bpf_buf == NULL)
	{
		printf(LEAP_ANSI_ERR "LEAP transport: malloc(%d) failed" LEAP_ANSI_RESET "\n", transport->bpf_buf_len);
		leap_rtems_transport_close(transport);
		return -1;
	}

	transport->link_up = 1;
	printf(
	    LEAP_TS_FMT LEAP_ANSI_OK "LEAP transport: %s MAC %02x:%02x:%02x:%02x:%02x:%02x EtherType 0x%04X" LEAP_ANSI_RESET "\n",
	    leap_rtems_uptime_str(),
	    transport->ifname,
	    transport->local_mac[0], transport->local_mac[1],
	    transport->local_mac[2], transport->local_mac[3],
	    transport->local_mac[4], transport->local_mac[5],
	    (unsigned)ethertype);
	fflush(stdout);
	return 0;
}

void
leap_rtems_transport_close(LeapRtemsTransport *transport)
{
	if (transport == NULL)
	{
		return;
	}

	if (transport->bpf_fd >= 0)
	{
		close(transport->bpf_fd);
		transport->bpf_fd = -1;
	}
	if (transport->bpf_buf != NULL)
	{
		free(transport->bpf_buf);
		transport->bpf_buf = NULL;
		transport->bpf_buf_len = 0;
	}

	if (transport->ioctl_fd >= 0)
	{
		close(transport->ioctl_fd);
		transport->ioctl_fd = -1;
	}
}

int
leap_rtems_transport_recv(
    LeapRtemsTransport *transport,
    uint8_t *src_mac_out,
    uint8_t *payload_out,
    size_t payload_capacity,
    size_t *payload_len_out,
    int timeout_ms)
{
	struct bpf_hdr bph;
	struct timeval tv;
	fd_set rfds;
	ssize_t n;
	size_t offset;
	size_t caplen;
	size_t leap_len;

	if (transport == NULL || transport->bpf_fd < 0 ||
	    payload_out == NULL || payload_len_out == NULL ||
	    transport->bpf_buf == NULL || transport->bpf_buf_len <= 0)
	{
		return -1;
	}

	*payload_len_out = 0u;

	for (;;)
	{
		FD_ZERO(&rfds);
		FD_SET(transport->bpf_fd, &rfds);
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = (timeout_ms % 1000) * 1000;

		switch (select(transport->bpf_fd + 1, &rfds, NULL, NULL, &tv))
		{
		case 0:
			return -1;
		case -1:
			if (errno == EINTR)
			{
				continue;
			}
			return errno;
		default:
			break;
		}

		n = read(transport->bpf_fd, transport->bpf_buf, (size_t)transport->bpf_buf_len);
		if (n < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return errno;
		}

		if ((size_t)n < sizeof(bph))
		{
			continue;
		}

		memcpy(&bph, transport->bpf_buf, sizeof(bph));
		offset = (size_t)bph.bh_hdrlen;
		caplen = (size_t)bph.bh_caplen;

		if (offset + caplen > (size_t)n || caplen < LEAP_RTEMS_ETH_HDR_LEN)
		{
			continue;
		}

		{
			const uint8_t *eth = transport->bpf_buf + offset;

			if (frame_ethertype(eth) != transport->ethertype)
			{
				continue;
			}

			if (!frame_dest_ok(transport, eth))
			{
				continue;
			}

			if (memcmp(eth + 6, transport->local_mac,
			    LEAP_RTEMS_MAC_LEN) == 0)
			{
				continue;
			}

			leap_len = caplen - LEAP_RTEMS_ETH_HDR_LEN;
			if (leap_len > payload_capacity)
			{
				return EMSGSIZE;
			}

			if (src_mac_out != NULL)
			{
				memcpy(src_mac_out, eth + 6, LEAP_RTEMS_MAC_LEN);
			}

			memcpy(payload_out, eth + LEAP_RTEMS_ETH_HDR_LEN, leap_len);
			*payload_len_out = leap_len;
			return 0;
		}
	}
}

int
leap_rtems_transport_send_leap(
    LeapRtemsTransport *transport,
    const uint8_t *dst_mac,
    uint8_t flags,
    uint16_t service_id,
    uint16_t message_type,
    uint32_t session_id,
    uint32_t sequence,
    uint32_t ack_sequence,
    const uint8_t *payload,
    size_t payload_length)
{
	uint8_t frame_buf[LEAP_MAX_FRAME_BYTES];
	uint8_t wire[LEAP_RTEMS_RX_BUF_SIZE];
	size_t frame_len = 0u;
	size_t total;
	size_t wire_total;
	ssize_t sent;

	if (transport == NULL || transport->bpf_fd < 0 || dst_mac == NULL)
	{
		return -1;
	}

	if (leap_frame_write(
		frame_buf,
		sizeof(frame_buf),
		&frame_len,
		flags,
		service_id,
		message_type,
		session_id,
		sequence,
		ack_sequence,
		payload,
		payload_length) != 0)
	{
		return -1;
	}

	total = LEAP_RTEMS_ETH_HDR_LEN + frame_len;
	wire_total = total < LEAP_RTEMS_MIN_FRAME ? LEAP_RTEMS_MIN_FRAME : total;
	if (wire_total > sizeof(wire))
	{
		return -1;
	}

	memcpy(wire, dst_mac, LEAP_RTEMS_MAC_LEN);
	memcpy(wire + 6, transport->local_mac, LEAP_RTEMS_MAC_LEN);
	wire[12] = (uint8_t)((transport->ethertype >> 8) & 0xFFu);
	wire[13] = (uint8_t)(transport->ethertype & 0xFFu);
	memcpy(wire + LEAP_RTEMS_ETH_HDR_LEN, frame_buf, frame_len);
	if (wire_total > total)
	{
		memset(wire + total, 0, wire_total - total);
	}

	sent = write(transport->bpf_fd, wire, wire_total);
	if (sent != (ssize_t)wire_total)
	{
		return -1;
	}

	return 0;
}

int
leap_rtems_transport_poll_link(LeapRtemsTransport *transport)
{
	struct ifmediareq ifmr;
	int error;
	int link_up;

	if (transport == NULL || transport->ioctl_fd < 0)
	{
		return -1;
	}

	link_up = 0;
	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, transport->ifname, sizeof(ifmr.ifm_name));
	error = ioctl(transport->ioctl_fd, SIOCGIFMEDIA, &ifmr);

	if (error == 0 && (ifmr.ifm_status & IFM_AVALID) != 0)
	{
		link_up = (ifmr.ifm_status & IFM_ACTIVE) != 0;
	}

	transport->link_up = link_up;
	return link_up;
}
