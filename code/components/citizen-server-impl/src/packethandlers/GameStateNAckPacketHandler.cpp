#include "StdInc.h"

#include <ServerInstanceBase.h>

#include <Client.h>
#include <state/ServerGameStatePublic.h>

#include "packethandlers/GameStateNAckPacketHandler.h"

#include <KeyedRateLimiter.h>
#include "GameServer.h"

#include "ByteReader.h"
#include <ClientDropReasons.h>

bool fx::ServerDecorators::GameStateNAckPacketHandler::Process(fx::ServerInstanceBase* instance, const fx::ClientSharedPtr& client, net::ByteReader& reader, ENetPacketPtr& packet)
{
	static fx::RateLimiterStore<uint32_t, false> gameStateNackLimiterStore{ instance->GetComponent<console::Context>().GetRef() };
	static auto gameStateNackRateLimiter = gameStateNackLimiterStore.GetRateLimiter(
		"gameStateNAck", fx::RateLimiterDefaults{ 200.f, 300.f }
	);

	if (!gameStateNackRateLimiter->Consume(client->GetNetId()))
	{
		instance->GetComponent<fx::GameServer>()->DropClientWithReason(client, fx::serverDropResourceName, fx::ClientDropReason::SERVER_PACKET_OVERFLOW, "Reliable gameStateNAck overflow.");
		return false;
	}

	return ProcessPacket(reader, [](net::packet::ClientGameStateNAck& clientGameStateNAck, fx::ServerInstanceBase* instance, const fx::ClientSharedPtr& client, ENetPacketPtr& packet)
	{
		gscomms_execute_callback_on_sync_thread([clientGameStateNAck, client, instance, packet]() mutable 
		{
			auto sgs = instance->GetComponent<fx::ServerGameStatePublic>();
			sgs->HandleGameStateNAck(instance, client, clientGameStateNAck);
			// packet needs to be moved to prevent packet memory from being freed
			(void)packet;
		});
	}, instance, client, packet);
}
