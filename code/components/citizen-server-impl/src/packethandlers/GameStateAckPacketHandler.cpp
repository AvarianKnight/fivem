#include "StdInc.h"

#include <ServerInstanceBase.h>

#include <Client.h>
#include <state/ServerGameStatePublic.h>

#include "packethandlers/GameStateAckPacketHandler.h"

#include <KeyedRateLimiter.h>
#include <GameServer.h>
#include <ClientDropReasons.h>

#include "ByteReader.h"

bool fx::ServerDecorators::GameStateAckPacketHandler::Process(fx::ServerInstanceBase* instance, const fx::ClientSharedPtr& client, net::ByteReader& reader, ENetPacketPtr& packet)
{
	static fx::RateLimiterStore<uint32_t, false> gameStateAckLimiterStore { instance->GetComponent<console::Context>().GetRef() };
	static auto gameStateAckRateLimiter = gameStateAckLimiterStore.GetRateLimiter(
		"gameStateAck", fx::RateLimiterDefaults{ 200.0f, 300.0f }
	);

	if (!gameStateAckRateLimiter->Consume(client->GetNetId()))
	{
		instance->GetComponent<fx::GameServer>()->DropClientWithReason(client, fx::serverDropResourceName, fx::ClientDropReason::SERVER_PACKET_OVERFLOW, "Reliable gameStateAck overflow.");
		return false;
	}

	return ProcessPacket(reader, [](net::packet::ClientGameStateAck& clientGameStateAck, fx::ServerInstanceBase* instance, const fx::ClientSharedPtr& client, ENetPacketPtr& packet)
	{
		gscomms_execute_callback_on_sync_thread([clientGameStateAck, client, instance, packet]() mutable 
		{
			auto sgs = instance->GetComponent<fx::ServerGameStatePublic>();
			sgs->HandleGameStateAck(instance, client, clientGameStateAck);
			// packet needs to be moved to the sync thread to prevent it from being freed
			(void)packet;
		});
	}, instance, client, packet);
}
