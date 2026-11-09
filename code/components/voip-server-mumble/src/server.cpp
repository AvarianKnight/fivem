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
#include "StdInc.h"
#include "server.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

#include "client.h"
#include "ClientRegistry.h"
#include "conf.h"
#include "log.h"
#include "memory.h"
#include "timer.h"
#include "version.h"
#include "util.h"
#include "sharedmemory.h"
#include "MumbleMessage.h"

extern char* bindaddr;
extern char* bindaddr6;
extern int bindport;
extern int bindport6;
int* udpsocks;
bool_t hasv4 = true, hasv6 = true;

const int on = 1;
int nofServerSocks = 4;

void Server_run()
{
	// checkIPversions();

	/* max clients + server sokets + client connecting that will be disconnected */
	// pollfds = Memory_safeCalloc((getIntConf(MUMBLE_MAX_CLIENTS) + nofServerSocks + 1) , sizeof(struct pollfd));

	/* Figure out bind address and port */
	// struct sockaddr_storage** addresses = Server_setupAddressesAndPorts();

	/* Prepare TCP sockets */
	// Server_setupTCPSockets(addresses, pollfds);

	/* Prepare UDP sockets */
	// Server_setupUDPSockets(addresses, pollfds);

	Log_info("uMurmur version %s ('%s') protocol version %d.%d.%d",
	UMURMUR_VERSION, UMURMUR_CODENAME, PROTVER_MAJOR, PROTVER_MINOR, PROTVER_PATCH);
	Log_info("Visit https://github.com/umurmur/umurmur");

	/* Main server loop */
	// Server_runLoop(pollfds);

	/* Disconnect clients and cleanup memory */
	Client_disconnect_all();
}

#include <HttpServerManager.h>
#include <TcpServerManager.h>
#include <MultiplexTcpServer.h>

#include <ServerInstanceBase.h>
#include <UdpInterceptor.h>

#include "messages.h"

#undef PROTOCOL_VERSION
#include <TLSServer.h>

#include "client.h"
#include "messagehandler.h"

#include <thread>

bool_t checkDecrypt(client_t* client, const uint8_t* encrypted, uint8_t* plain, unsigned int len);
int Client_send_udp(client_t* client, uint8_t* data, int len);

// #TODO: Once correctness is guaranteed have servers create ETW traces to
// measure lock contention.
static std::shared_mutex mumblePairsMutex;
static std::map<net::PeerAddress, bool> mumblePairs;

static std::map<net::PeerAddress, int> retryPairs;

static std::map<std::string, int> clientsPerIP;

static void SetMumbleAddress(const net::PeerAddress& address, bool isMumble)
{
	std::unique_lock _(mumblePairsMutex);
	mumblePairs.insert({ address, isMumble });
}

static void ClearMumbleAddress(const net::PeerAddress& address)
{
	std::unique_lock _(mumblePairsMutex);
	mumblePairs.erase(address);
}

static bool IsMumbleAddress(const net::PeerAddress& address, bool& isMumble)
{
	std::shared_lock _(mumblePairsMutex);
	if (auto it = mumblePairs.find(address); it != mumblePairs.end())
	{
		// if this is already known to not be a Mumble client, ignore
		if (!it->second)
		{
			return false;
		}

		isMumble = true;
	}
	return true;
}

static void CleanupMumbleAddresses()
{
	{
		std::unique_lock _(mumblePairsMutex);
		for (auto it = mumblePairs.begin(); it != mumblePairs.end();)
		{
			if (!it->second)
			{
				it = mumblePairs.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}

static bool mumbleServerInitialized;

static void EnsureServerInitialized()
{
	if (!mumbleServerInitialized)
	{
		Chan_init();
		Client_init();
		// Ban_init();
		mumbleServerInitialized = true;
	}
}

std::shared_ptr<ConVar<bool>> mumble_disableServer;
std::shared_ptr<ConVar<int>> mumble_maxClientsPerIP;
std::shared_ptr<ConVar<bool>> mumble_allowExternalConnections;
std::shared_ptr<MumbleServer> g_mumbleServer;

static InitFunction initFunction([]()
{
	mumble_disableServer = std::make_shared<ConVar<bool>>("mumble_disableServer", ConVar_None, false);
	mumble_maxClientsPerIP = std::make_shared<ConVar<int>>("mumble_maxClientsPerIP", ConVar_None, 32);
	mumble_allowExternalConnections = std::make_shared<ConVar<bool>>("mumble_allowExternalConnections", ConVar_None, false);
	g_mumbleServer = std::make_shared<MumbleServer>();

	fx::ServerInstanceBase::OnServerCreate.Connect([](fx::ServerInstanceBase* instance)
	{
		g_clientRegistry = instance->GetComponent<fx::ClientRegistry>();
	});

	OnCreateTlsMultiplex.Connect([=](fwRefContainer<net::MultiplexTcpServer> multiplex)
	{
		if (mumble_disableServer->GetValue())
			return;

		EnsureServerInitialized();

		auto server = multiplex->CreateServer([](const std::vector<uint8_t>& bytes)
		{
			if (bytes.size() > 6)
			{
				if (bytes[0] == '\0' && bytes[1] == '\0')
				{
					uint32_t len = ntohl(*(uint32_t*)&bytes[2]);

					if (len > 8192) {
						return net::MultiplexPatternMatchResult::NoMatch;
					}

					// hacky, actually should check parsing
					if (bytes.size() < (static_cast<size_t>(len) + 6))
					{
						return net::MultiplexPatternMatchResult::InsufficientData;
					}

					auto version = std::make_unique<MumbleProto::Version>();
					return (version->ParseFromArray(&bytes[6], len)) ? net::MultiplexPatternMatchResult::Match : net::MultiplexPatternMatchResult::NoMatch;
				}

				return net::MultiplexPatternMatchResult::NoMatch;
			}

			return net::MultiplexPatternMatchResult::InsufficientData;
		});

		server->SetConnectionCallback([=](fwRefContainer<net::TcpServerStream> stream)
		{
			client_t* client;

			{
				// NOT SAFE: NEED SOME LOCKING HERE
				//std::lock_guard _(g_mumbleClientMutex);

				auto hostIP = stream->GetPeerAddress().GetHost();
				auto mapEntry = clientsPerIP.find(hostIP);
				if (mapEntry != clientsPerIP.end())
				{
					auto maxClientCount = mumble_maxClientsPerIP->GetValue();
					if (mapEntry->second >= maxClientCount)
					{
						trace("IP %s already has %d Mumble connections active, rejecting client. This limit can be changed using the mumble_maxClientsPerIP ConVar.\n",
						hostIP.c_str(), maxClientCount);
						stream->Close();
						return;
					}
					mapEntry->second++;
				}
				else
				{
					clientsPerIP.insert({ hostIP, 1 });
				}

				Client_add(stream, &client);
			}

			stream->SetCloseCallback([=]()
			{
				std::shared_lock _(client->m_clientMutex);
				ClearMumbleAddress(client->remote_udp);
				if (!g_mumbleServer->AreExternalConnectionsAllowed())
				{
					int playerId = Client_getPlayerId(client, client->username);
					if (playerId != -1)
					{
						g_mumbleServer->ResetServerId(playerId);
					}
				}

				auto mapEntry = clientsPerIP.find(client->remote_tcp.GetHost());
				if (mapEntry != clientsPerIP.end())
				{
					if (mapEntry->second == 1)
						clientsPerIP.erase(mapEntry);
					else
						mapEntry->second--;
					}
				// It is assumed Client_free is only ever called from this location.
				Client_free(client);
			});

			stream->SetReadCallback([=](const std::vector<uint8_t>& data)
			{
				std::unique_lock _(client->m_clientMutex);
				if (data.empty())
				{
					return;
				}

				if (data.size() > READ_BUFFER_SIZE)
				{
					stream->Close();
					return;
				}

				net::Span dataSpan{ const_cast<uint8_t*>(data.data()), data.size() };
				const bool result = client->streamByteReader.Push<net::packet::ClientMumbleMessage>(dataSpan, [client](net::packet::ClientMumbleMessage& message)
				{
					if (message_t* msg = Msg_networkToMessage(message.data.GetValue().data() - 6 /*mumble needs start at message type*/, message.data.GetValue().size() + 6))
					{
						Mh_handle_message(client, msg);
					}

					client->rxcount = 0;
				});

				// close stream if shutting down or if the message was not successfully parsed
				if (!result || client->shutdown_wait)
				{
					stream->Close();
				}

				if (client->authenticated)
				{
					Timer_restart(&client->lastActivity);
				}
			});
		});

		std::thread([=]()
		{
			using namespace std::chrono_literals;
			SetThreadName(-1, "[Mumble] Worker thread");

			size_t i = 0;

			while (true)
			{
				std::this_thread::sleep_for(1000ms);

				Client_janitor();

				++i;

				// clean up pairs every ~20 seconds
				if (i > 20)
				{
					CleanupMumbleAddresses();
					retryPairs.clear();
					i = 0;
				}
			}
		})
		.detach();
	});

	fx::ServerInstanceBase::OnServerCreate.Connect([](fx::ServerInstanceBase* instance)
	{
		if (mumble_disableServer->GetValue())
			return;

		EnsureServerInitialized();

		auto interceptor = instance->GetComponent<fx::UdpInterceptor>();

		interceptor->OnIntercept.Connect([interceptor](const net::PeerAddress& address, const uint8_t* data, size_t len, bool* intercepted)
		{
			// 1024 is the max udp length for mumble, if its bigger we don't need to check it.
			if (len > UDP_MAX_PACKET_SIZE) {
				return;
			}

			if (len < 5)
			{
				return;
			}

			bool known = false;

			if (!IsMumbleAddress(address, known))
			{
				return;
			}

			if (g_mumbleServer->AreExternalConnectionsAllowed())
			{
				// is this a Mumble ping?
				if (len == 12 && *(uint32_t*)data == 0)
				{
					uint32_t ping[6];
					memcpy(ping, data, len);

					ping[0] = htonl((uint32_t)((PROTVER_MAJOR << 16) | (PROTVER_MINOR << 8) | (PROTVER_PATCH)));
					ping[3] = htonl((uint32_t)Client_count());
					ping[4] = htonl((uint32_t)getIntConf(MUMBLE_MAX_CLIENTS));
					ping[5] = htonl((uint32_t)getIntConf(MAX_BANDWIDTH));

					*intercepted = true;

					interceptor->Send(address, ping, sizeof(ping));

					return;
				}
			}

			auto fromAddress = address.GetHostBytes();

			// Mumble clients are expected to be connected to TCP already - if this is not a known Mumble TCP pair, mark and drop
			client_t* client = nullptr;

			uint8_t buffer[UDP_MAX_PACKET_SIZE] = { 0 };

			if (!known)
			{
				// try finding a client for whom the packet will decrypt correctly
				client_t* itr = nullptr;
				bool found = false;

				g_mumbleServer->ForAllSocketlessClients([&fromAddress, &address, &data, &known, &buffer, len, &found, &itr](client_t* client) {
					std::shared_lock _(client->m_clientMutex);

					if (client->remote_udp == net::PeerAddress{} && (client->remote_tcp.GetHostBytes() == fromAddress || client->remote_tcp.GetHostBytesV6() == fromAddress))
					{
						known = true;

						// try decrypting the packet, maybe it'll be mumble
						if (checkDecrypt(client, data, buffer, std::min(len, sizeof(buffer))))
						{
							// it's mumble!
							retryPairs[address] = 0;

							found = true;

							itr = client;

							return true;
						}
					}
					return false;
				});

				// if no client matched even on IP, mark this pair as dead
				if (!known)
				{
					SetMumbleAddress(address, false);
					return;
				}

				// wasn't mumble, note down a failure and return
				if (!found)
				{
					// quite certainly not mumble
					if (retryPairs[address] > 5)
					{
						// permanently note down this pair as not-mumble
						SetMumbleAddress(address, false);

						return;
					}

					// note this client has been mumble
					retryPairs[address]++;
					return;
				}

				// mumble! let's mark it
				SetMumbleAddress(address, true);

				std::unique_lock _(itr->m_clientMutex);
				itr->remote_udp = address;
			}
			else
			{
				client_t* client = g_mumbleServer->GetClientFromAddress(address);

				if (!client)
				{
					return;
				}

				if (!checkDecrypt(client, data, buffer, std::min(len, sizeof(buffer))))
				{
					*intercepted = true;
					return;
				}
			}

			if (!client)
			{
				return;
			}

			std::unique_lock _(client->m_clientMutex);
			client->interceptor = interceptor.GetRef();

			*intercepted = true;

			client->bUDP = true;
			len -= 4; /* Adjust for crypt header */
			auto msgType = (UDPMessageType_t)((buffer[0] >> 5) & 0x7);

			switch (msgType)
			{
				case UDPVoiceSpeex:
				case UDPVoiceCELTAlpha:
				case UDPVoiceCELTBeta:
					break;
				case UDPVoiceOpus:
					Client_voiceMsg(client, buffer, len);
					break;
				case UDPPing:
					Log_debug("UDP Ping reply len %d", len);
					Client_send_udp(client, buffer, len);
					break;
				default:
				{
					auto clientAddressString = Util_clientAddressToString(client);
					Log_debug("Unknown UDP message type from %s port %d", clientAddressString, address.GetPort());
					free(clientAddressString);
					break;
				}
			}
		});
	},
	999);
});
