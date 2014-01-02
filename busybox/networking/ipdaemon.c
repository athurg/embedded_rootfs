/*
::::    :::: ::::::::::::    .::::::    Company    : NTS-intl
 :::     ::   ::  ::  ::   ::      ::   Author     : Athurg.Feng
 ::::    ::       ::        ::          Maintainer : Athurg.Feng
 :: ::   ::       ::         ::         Project    : 
 ::  ::  ::       ::           :::      FileName   : ipdaemon.c
 ::   :: ::       ::             ::     Generate   : 
 ::    ::::       ::       ::      ::   Update     : 2011-02-28 13:38:13
::::    :::     ::::::      ::::::::    Version    : 0.0.1

Description
	IP探测器服务端

	用于在系统IP未知时，通过将系统和PC对联，然后获取系统IP地址。

Changelog:
	2011-02-24	v0.0.1
				版本初次发布

	2011-02-28	v0.0.2
				将发送UDP包由直接发UDP数据包改为发原始数据包，
			使之在系统没有设置默认网关时仍然可用。
*/

#include "libbb.h"

#include "libiproute/utils.h"
#include "libiproute/ip_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>//For struct ip IPVERSION IP_DEFTTL
#include <netinet/udp.h>//For struct updhdr
#include <net/ethernet.h>//For ETH_P_IP
#include <netpacket/packet.h>//For struct sockaddr_ll PACKET_BROADCAST
#include <net/if.h>//For ETH_P_IP
#include <sys/ioctl.h>
#include <netdb.h>

#define CMD_GET_IP	"AT:GET IP"
#define CMD_OK		"AT:OK"
#undef	IP_DAEMON_DEBUG

#define IP_DAEMON_PORT	1987

short calc_cksum(unsigned short *buff, int size);
int raw_udp_broadcast(const char *ifname, char *dat, int size, int srcport, int dstport);

#ifdef IP_DAEMON_DEBUG
int main(int argc, char **argv)
#else
int ipdaemon_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ipdaemon_main(int argc UNUSED_PARAM, char **argv)
#endif
{
	int sockfd, cnt=0, ret;
	unsigned int len=sizeof(struct sockaddr_in);
	char buf[50]={0};
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_addr = {.s_addr = INADDR_ANY},
		.sin_port = htons(IP_DAEMON_PORT),
	};

	printf("IP Daemon\n\n\tversion 0.0.2\n\n");

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd<0) {
		perror("Fail to create socket!\n");
		return -1;
	}

	if (bind(sockfd, (struct sockaddr *)&addr, len)) {
		perror("Fail to bind port\n");
		close(sockfd);
		return 0;
	}

	while(1) {
		ret = recvfrom(sockfd, buf, 50, 0, (struct sockaddr *)&addr, &len);
		if ((ret>0) && !strcmp(buf, CMD_GET_IP)) {
			printf("%4d : request from %s\n", ++cnt, inet_ntoa(addr.sin_addr));
			raw_udp_broadcast("eth0", (char *)CMD_OK, strlen(CMD_OK)+1, IP_DAEMON_PORT, IP_DAEMON_PORT);
		}
	}

	close(sockfd);

	return 0;
}

int raw_udp_broadcast(const char *ifname, char *dat, int size, int srcport, int dstport)
{
	int sockfd, ret;
	struct sockaddr_ll addr={0};//用于发送的目的地
	struct sockaddr_in *myaddr;
	char *buff;
	struct iphdr *iphdr=NULL;
	struct udphdr *udphdr=NULL;
	int total_size;
	struct ifreq ifr;

	//创建套接字
	sockfd = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
	if (sockfd<0) {
		perror("Fail to create RAW socket");
		return sockfd;
	}

	//获取本机IP地址
	strncpy(ifr.ifr_name, ifname, strlen(ifname)+1);
	ioctl(sockfd, SIOCGIFADDR, &ifr);
	myaddr = (struct sockaddr_in *)(&ifr.ifr_addr);

	//申请内存
	total_size = sizeof(struct iphdr)+sizeof(struct udphdr)+size;
	buff = malloc(total_size);
	iphdr = (struct iphdr *)buff;
	udphdr = (struct udphdr *) (buff+sizeof(struct iphdr));
	memset(buff, 0, total_size);

	//复制数据
	memcpy(buff+total_size-size, dat, size);

	//IP头
	//	tos/id/frag_off are don't care
	iphdr->version  = IPVERSION;
	iphdr->ihl      = sizeof(struct iphdr)>>2;//IP头长度
	iphdr->tot_len  = htons(total_size);//IP包总长度
	iphdr->ttl      = IPDEFTTL;
	iphdr->protocol = IPPROTO_UDP;
	iphdr->saddr    = myaddr->sin_addr.s_addr;
	iphdr->daddr    = INADDR_BROADCAST;
	iphdr->check    = calc_cksum((unsigned short *)iphdr, sizeof(*iphdr));


	//UDP头
	udphdr->source = htons(srcport);
	udphdr->dest   = htons(dstport);
	udphdr->len    = htons(size+sizeof(*udphdr));
	udphdr->check  = 0; //FIXME:UDP校验值怎么算的涅？

	//发送
	addr.sll_family   = PF_PACKET;
	addr.sll_protocol = htons(ETH_P_IP);
	addr.sll_pkttype  = PACKET_BROADCAST;
	addr.sll_ifindex  = if_nametoindex(ifname);//网卡
	memset(addr.sll_addr, 0xFF, 8);//MAC为广播地址
	ret = sendto(sockfd, buff, total_size, 0, (struct sockaddr *)&addr, sizeof(addr));

	free(buff);

	return ret;
}

short calc_cksum(unsigned short *buff, int size)
{
	unsigned int ret=0;
	int i;
	size >>=1;
	for (i=0; i<size; i++){
		ret += htons(buff[i]);
	}

	ret = (ret&0xFFFF) + (ret>>16);
	ret += ret>>16;

	return htons(~ret);
}

