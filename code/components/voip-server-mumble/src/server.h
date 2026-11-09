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

#ifndef SERVER_H_347563
#define SERVER_H_347563
#include <unordered_set>

#include "ClientRegistry.h"
#include "console/Console.VariableHelpers.h"
#include <memory>
#include <mutex>
#include <NetAddress.h>

#include "channel.h"
#include "client.h"

class MumbleServer
{
private:
	std::shared_mutex m_clientIdsMutex;
	std::unordered_map<const int, client_t*> m_clientIds;

	std::shared_mutex m_mumbleClientMutex;
	std::unordered_map<uint32_t, client_t*> m_mumbleClients;

	// all of the clients that have a UDP socket
	std::shared_mutex m_socketMapMutex;
	std::unordered_map<net::PeerAddress, client_t*> m_socketMap;

	// all of the clients that don't have a udp socket yet
	std::shared_mutex m_socketlessClientsMutex;
	std::unordered_map<uint32_t, client_t*> m_socketlessClients;

	std::shared_mutex m_mumbleChannelMutex;
	std::unordered_map<uint32_t, channel_t*> m_mumbleChannels;


	bool m_allowExternalConnections = false;
	
	MumbleServer()
	{
		static auto mumble_allowExternalConnections = std::make_shared<ConVar<bool>>("mumble_allowExternalConnections", ConVar_None, false, &m_allowExternalConnections);
	}

public:
	bool AreExternalConnectionsAllowed() const
	{
		return m_allowExternalConnections;
	}

	bool IsServerIdUsed(const int serverId)
	{
		std::shared_lock _(m_clientIdsMutex);
		return m_clientIds.find(serverId) != m_clientIds.end();
	}

	client_t* GetClientFromServerId(const int serverId)
	{
		std::shared_lock _(m_clientIdsMutex);
		if (auto client = m_clientIds.find(serverId); client != m_clientIds.end())
		{
			return client->second;
		}

		return nullptr;
	
	}

	void ResetServerId(const int serverId)
	{
		std::unique_lock _(m_clientIdsMutex);
		m_clientIds.erase(serverId);
	}

	void RegisterClientToServerId(const int serverId, client_t* client)
	{
		std::unique_lock _(m_clientIdsMutex);
		m_clientIds[serverId] = client;
	}

	client_t* GetClientFromAddress(const net::PeerAddress& addr)
	{
		std::shared_lock _(m_socketMap);
		if (auto client = m_socketMap.find(addr); client != m_socketMap.end())
		{
			return client->second;
		}

		return nullptr;
	}

	template<typename TFn>
	inline void ForAllMumbleClients(TFn&& cb)
	{
		std::shared_lock _(m_mumbleClientMutex);
		for (auto& [session_id, client] : m_mumbleClients)
		{
			if (cb(client))
			{
				return;
			}
		}
	}

	template<typename TFn>
	inline void ForAllSocketlessClients(TFn&& cb)
	{
		std::shared_lock _(m_socketlessClients);
		for (const auto& [session_id, client] : m_socketlessClients)
		{
			if (cb(client))
			{
				return;
			}
		}
	}
};

inline fwRefContainer<fx::ClientRegistry> g_clientRegistry;

#endif
