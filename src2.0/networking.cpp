#include "stdafx.h"

#include "winsock2.h"
#include "ws2tcpip.h" //IP_HDRINCL is here

#pragma comment(lib,"ws2_32.lib") //winsock 2.2 library

#include "core.h"
#include "client.h"
#include "networking.h"

#define MAX_PACKET 4096

typedef struct
{
	unsigned char IHL : 4;
	unsigned char Version : 4;
	unsigned char ECN : 2;
	unsigned char DSCP : 6;
	unsigned short Length;
	unsigned short ID;
	unsigned short FragOffset : 13;
	unsigned short Flags : 3;
	unsigned char TTL;
	unsigned char Protocol;
	unsigned short Checksum;
	unsigned int SourceIP;
	unsigned int DestIP;
} IP_HDR;

typedef struct
{
	unsigned short SourcePort;
	unsigned short DestPort;
	unsigned short Length;
	unsigned short Checksum;
} UDP_HDR;

struct psd_udp {
	struct in_addr src;
	struct in_addr dst;
	unsigned char pad;
	unsigned char proto;
	unsigned short udp_len;
	UDP_HDR udp;
};


/* Create winsock socket */
bool CreateSocket(Client *pClient)
{
	WSADATA data;
	char aBuf[1024];

	if (WSAStartup(MAKEWORD(2, 2), &data))
	{
		sprintf_s(aBuf, sizeof(aBuf), "WSAStartup() failed: %d\n", GetLastError());
		Output(aBuf);
		return false;
	}

	pClient->m_Networking.sock = WSASocket(AF_INET, SOCK_RAW, IPPROTO_UDP, NULL, 0, 0);

	if (pClient->m_Networking.sock == INVALID_SOCKET)
	{
		sprintf_s(aBuf, sizeof(aBuf), "WSASocket() failed: %d\n", WSAGetLastError());
		Output(aBuf);
		return false;
	}

	BOOL optvalue = TRUE;

	if (setsockopt(pClient->m_Networking.sock, IPPROTO_IP, IP_HDRINCL, (char *)&optvalue, sizeof(optvalue)) == SOCKET_ERROR)
	{
		sprintf_s(aBuf, sizeof(aBuf), "setsockopt(IP_HDRINCL) failed: %d\n", WSAGetLastError());
		Output(aBuf);
		return false;
	}

	//sprintf_s(aBuf, sizeof(aBuf), "socket created correctly\n");
	//Output(aBuf);

	return true;
}

/* Create winsock socket */
bool CreateSocket_d(Client *pClient, int id)
{
	WSADATA data;
	char aBuf[1024];

	if (WSAStartup(MAKEWORD(2, 2), &data))
	{
		sprintf_s(aBuf, sizeof(aBuf), "WSAStartup() failed: %d\n", GetLastError());
		Output(aBuf);
		return false;
	}

	pClient->m_Networking.sockDummies[id] = WSASocket(AF_INET, SOCK_RAW, IPPROTO_UDP, NULL, 0, 0);

	if (pClient->m_Networking.sockDummies[id] == INVALID_SOCKET)
	{
		sprintf_s(aBuf, sizeof(aBuf), "WSASocket() failed: %d\n", WSAGetLastError());
		Output(aBuf);
		return false;
	}

	BOOL optvalue = TRUE;

	if (setsockopt(pClient->m_Networking.sockDummies[id], IPPROTO_IP, IP_HDRINCL, (char *)&optvalue, sizeof(optvalue)) == SOCKET_ERROR)
	{
		sprintf_s(aBuf, sizeof(aBuf), "setsockopt(IP_HDRINCL) failed: %d\n", WSAGetLastError());
		Output(aBuf);
		return false;
	}

	//sprintf_s(aBuf, sizeof(aBuf), "socket created correctly\n");
	//Output(aBuf);

	return true;
}

/* Close the socket */
void CloseSocket(Client *pClient)
{
	closesocket(pClient->m_Networking.sock);
	WSACleanup();
}

/* Close the socket */
void CloseSocket_d(Client *pClient, int id)
{
	closesocket(pClient->m_Networking.sockDummies[id]);
	WSACleanup();
}

/* Communication with clients */
void send(SOCKET s, char *text)
{
	send(s, text, strlen(text), 0);
}


USHORT checksum(USHORT *buffer, int size)
{
	unsigned long cksum = 0;

	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	if (size)
	{
		cksum += *(UCHAR*)buffer;
	}
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);

	return (USHORT)(~cksum);
}

void SendData(Client *pClient, unsigned int srcIp, unsigned short srcPort, unsigned int dstIp, unsigned short dstPort, const char *pData, int Size)
{
	SOCKADDR_IN m_Sin;
	unsigned short m_ChecksumUDP;
	char m_aPacket[MAX_PACKET];

	memset(&m_aPacket, 0, MAX_PACKET);

	IP_HDR IP;
	IP.Version = 4;
	IP.IHL = sizeof(IP_HDR) / sizeof(unsigned long);
	IP.DSCP = 0;
	IP.ECN = 1;
	IP.Length = htons(sizeof(IP_HDR) + sizeof(UDP_HDR) + Size);
	IP.ID = 0;
	IP.Flags = 0;
	IP.FragOffset = 0;
	IP.TTL = 128;
	IP.Protocol = IPPROTO_UDP;
	IP.Checksum = 0;
	IP.SourceIP = srcIp;
	IP.DestIP = dstIp;
	IP.Checksum = checksum((USHORT *)&IP, sizeof(IP));

	UDP_HDR UDP;
	UDP.SourcePort = srcPort;
	UDP.DestPort = dstPort;
	UDP.Length = htons(sizeof(UDP_HDR) + Size);
	UDP.Checksum = 0;

	char *ptr = NULL;

	m_ChecksumUDP = 0;
	ptr = m_aPacket;
	memset(&m_aPacket, 0, MAX_PACKET);

	memcpy(ptr, &IP.SourceIP, sizeof(IP.SourceIP));
	ptr += sizeof(IP.SourceIP);
	m_ChecksumUDP += sizeof(IP.SourceIP);

	memcpy(ptr, &IP.DestIP, sizeof(IP.DestIP));
	ptr += sizeof(IP.DestIP);
	m_ChecksumUDP += sizeof(IP.DestIP);

	ptr++;
	m_ChecksumUDP++;

	memcpy(ptr, &IP.Protocol, sizeof(IP.Protocol));
	ptr += sizeof(IP.Protocol);
	m_ChecksumUDP += sizeof(IP.Protocol);

	memcpy(ptr, &UDP.Length, sizeof(UDP.Length));
	ptr += sizeof(UDP.Length);
	m_ChecksumUDP += sizeof(UDP.Length);

	memcpy(ptr, &UDP, sizeof(UDP));
	ptr += sizeof(UDP);
	m_ChecksumUDP += sizeof(UDP);

	for (int i = 0; i < Size; i++, ptr++)
		*ptr = pData[i];

	m_ChecksumUDP += Size;

	UDP.Checksum = checksum((USHORT *)m_aPacket, m_ChecksumUDP);

	memset(&m_aPacket, 0, MAX_PACKET);
	ptr = m_aPacket;

	memcpy(ptr, &IP, sizeof(IP));
	ptr += sizeof(IP);

	memcpy(ptr, &UDP, sizeof(UDP));
	ptr += sizeof(UDP);

	memcpy(ptr, pData, Size);

	m_Sin.sin_family = AF_INET;
	m_Sin.sin_port = dstPort;
	m_Sin.sin_addr.s_addr = dstIp;

	sendto(pClient->m_Networking.sock, m_aPacket, sizeof(IP) + sizeof(UDP) + Size, 0, (SOCKADDR *)&m_Sin, sizeof(m_Sin));
}

void SendData(Client *pClient, int id, unsigned int srcIp, unsigned short srcPort, unsigned int dstIp, unsigned short dstPort, const char *pData, int Size)
{
	SOCKADDR_IN m_Sin;
	unsigned short m_ChecksumUDP;
	char m_aPacket[MAX_PACKET];

	memset(&m_aPacket, 0, MAX_PACKET);

	IP_HDR IP;
	IP.Version = 4;
	IP.IHL = sizeof(IP_HDR) / sizeof(unsigned long);
	IP.DSCP = 0;
	IP.ECN = 1;
	IP.Length = htons(sizeof(IP_HDR) + sizeof(UDP_HDR) + Size);
	IP.ID = 0;
	IP.Flags = 0;
	IP.FragOffset = 0;
	IP.TTL = 128;
	IP.Protocol = IPPROTO_UDP;
	IP.Checksum = 0;
	IP.SourceIP = srcIp;
	IP.DestIP = dstIp;
	IP.Checksum = checksum((USHORT *)&IP, sizeof(IP));

	UDP_HDR UDP;
	UDP.SourcePort = srcPort;
	UDP.DestPort = dstPort;
	UDP.Length = htons(sizeof(UDP_HDR) + Size);
	UDP.Checksum = 0;

	char *ptr = NULL;

	m_ChecksumUDP = 0;
	ptr = m_aPacket;
	memset(m_aPacket, 0, MAX_PACKET);

	memcpy(ptr, &IP.SourceIP, sizeof(IP.SourceIP));
	ptr += sizeof(IP.SourceIP);
	m_ChecksumUDP += sizeof(IP.SourceIP);

	memcpy(ptr, &IP.DestIP, sizeof(IP.DestIP));
	ptr += sizeof(IP.DestIP);
	m_ChecksumUDP += sizeof(IP.DestIP);

	ptr++;
	m_ChecksumUDP++;

	memcpy(ptr, &IP.Protocol, sizeof(IP.Protocol));
	ptr += sizeof(IP.Protocol);
	m_ChecksumUDP += sizeof(IP.Protocol);

	memcpy(ptr, &UDP.Length, sizeof(UDP.Length));
	ptr += sizeof(UDP.Length);
	m_ChecksumUDP += sizeof(UDP.Length);

	memcpy(ptr, &UDP, sizeof(UDP));
	ptr += sizeof(UDP);
	m_ChecksumUDP += sizeof(UDP);

	for (int i = 0; i < Size; i++, ptr++)
		*ptr = pData[i];

	m_ChecksumUDP += Size;

	UDP.Checksum = checksum((USHORT *)m_aPacket, m_ChecksumUDP);

	memset(m_aPacket, 0, MAX_PACKET);
	ptr = m_aPacket;

	memcpy(ptr, &IP, sizeof(IP));
	ptr += sizeof(IP);

	memcpy(ptr, &UDP, sizeof(UDP));
	ptr += sizeof(UDP);

	memcpy(ptr, pData, Size);

	m_Sin.sin_family = AF_INET;
	m_Sin.sin_port = dstPort;
	m_Sin.sin_addr.s_addr = dstIp;

	sendto(pClient->m_Networking.sockDummies[id], m_aPacket, sizeof(IP) + sizeof(UDP) + Size, 0, (SOCKADDR *)&m_Sin, sizeof(m_Sin));
}
