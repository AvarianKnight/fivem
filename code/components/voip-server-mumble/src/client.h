/* Copyright (C) 2009-2014, Martin Johansson <martin@fatbob.nu>
   Copyright (C) 2005-2014, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
	 this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
	 this list of conditions and the following disclaimer in the documentation
	 and/or other materials provided with the distribution.
   - Neither the name of the Developers nor the names of its contributors may
	 be used to endorse or promote products derived from this software without
	 specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef CLIENT_H_45786678
#define CLIENT_H_45786678

#include <TcpServer.h>

#include <stdint.h>

#include "list.h"
#include "types.h"
#include "messages.h"
#include "crypt.h"
#include "timer.h"

#include <UdpInterceptor.h>
#include <NetAddress.h>
#include "StreamByteReader.h"

// The maximum size a UDP packet can be as defined by the mumble spec
constexpr uint16_t UDP_MAX_PACKET_SIZE = 1024;

// The maximum size the receive buffer should be for TCP for incoming and outgoing commands
constexpr uint16_t TCP_BUFSIZE = 8192;

constexpr uint16_t READ_BUFFER_SIZE = (TCP_BUFSIZE + 6 + 4) * 2;

// NOTE: This was reduced from 60 -> 15 since we do frequent pings on the client
// The maximum amount of time in seconds a client can be inactive before getting removed
constexpr uint16_t INACTIVITY_TIMEOUT = 15;

constexpr uint16_t MAX_TOKENSIZE = 64;

// TODO: have it so that the client sends its connection token
constexpr uint16_t MAX_TOKENS = 1;

#define MAX_TOKENSIZE 64
#define MAX_TOKENS 1
#define KEY_LENGTH sizeof(uint16_t) + 4 * sizeof(uint32_t)

#define IS_AUTH(_a_) ((_a_)->m_authenticated)

#ifdef __linux__
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

struct Version 
{
	char *os, *release, *os_version;
	
};

class client_t
{
public:
	fwRefContainer<net::TcpServerStream> stream;
	uint32_t sessionId;
	// used any time we read/write to the client
	std::shared_mutex m_clientMutex;
	
	std::atomic<bool> m_hasUdp = false;
	std::atomic<bool> m_mute = false;
	std::atomic<bool> m_authenticated = false;
	std::atomic<bool> m_deaf = false;
	std::atomic<bool> m_selfDeaf = false;
	std::atomic<bool> m_selfMute = false;
	
	std::atomic<bool> m_shouldShutdown = false;
	
	cryptState_t cryptState;
	
	net::PeerAddress remote_tcp;
	net::PeerAddress remote_udp;
	
	uint8_t rcvbuf[READ_BUFFER_SIZE];
	net::StreamByteReader streamByteReader{ rcvbuf, READ_BUFFER_SIZE };
	uint8_t txbuf[TCP_BUFSIZE];
	uint32_t rxcount, txcount, txsize;
	
	uint8_t key[KEY_LENGTH];
	char* username;
	char *os, *release, *os_version;
	uint32_t version;
	int availableBandwidth;
	etimer_t lastActivity, connectTime, idleTime;
	struct dlist node;
	struct dlist txMsgQueue;
	int txQueueCount;
	void* channel; /* Ugly... */
	char* context;
	struct dlist chan_node;
	struct dlist voicetargets;
	struct dlist tokens;
	int tokencount;
	uint8_t hash[20];
	bool_t isAdmin;
	bool_t isSuppressed;
	float UDPPingAvg, UDPPingVar, TCPPingAvg, TCPPingVar;
	uint32_t UDPPackets, TCPPackets;
	fx::UdpInterceptor* interceptor;
};

typedef struct
{
	char* token;
	struct dlist node;
} token_t;

void Client_init();
int Client_getfds(struct pollfd* pollfds);
bool Client_is_player_muted(const int serverId);
void Client_set_player_muted(const int serverId, bool muted);
void Client_janitor();
int Client_add(fwRefContainer<net::TcpServerStream> stream, client_t** client);
int Client_read_fd(int fd);
int Client_write_fd(int fd);
int Client_send_message(client_t* client, message_t* msg);
int Client_send_message_ver(client_t* client, message_t* msg, uint32_t version);
int Client_send_message_except_ver(client_t* client, message_t* msg, uint32_t version);
int Client_count(void);
void Client_close(client_t* client);
client_t* Client_iterate(client_t** client);
int Client_send_message_except(client_t* client, message_t* msg);
int Client_read_udp(int udpsock);
void Client_disconnect_all();
int Client_voiceMsg(client_t* client, uint8_t* data, int len);
void Client_textmessage(client_t* client, char* text);
bool_t Client_token_match(client_t* client, char const* str);
void Client_token_free(client_t* client);
void Client_token_add(client_t* client, char* token_string);
void Client_free(client_t* client);
int Client_getPlayerId(client_t* client, char* username);

#endif
