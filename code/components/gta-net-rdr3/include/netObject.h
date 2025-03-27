#pragma once

#include <NetworkPlayerMgr.h>
#include <net/NetObjEntityType.h>
#include <CrossBuildRuntime.h>

using NetObjEntityType = fx::sync::NetObjEntityType;

inline int MapNetObjectMethod(int offset)
{
	if (offset >= 0x170 && xbr::IsGameBuildOrGreater<1491>())
	{
		offset += 0x8;
	}

	return offset;
}

namespace rage
{
class netBlender;
class netSyncTree;
class netObject;

struct netSyncDataBase
{
	uint32_t m_playerMasks[3];
	char pad_14[4];
	void *unk_18;
	char pad_20[8];
	_RTL_CRITICAL_SECTION m_lock;
	char pad_50[8];
};

class CNetworkSyncDataULBase
{
public:
	virtual ~CNetworkSyncDataULBase() = default;

	// dummy functions to satisfy compiler
	inline virtual void m_8() { }

	inline virtual void m_10() { }

	inline virtual void m_18() { }

	inline virtual void m_20() { }

	inline virtual void m_28() { }

	inline virtual void m_30() { }

	inline virtual void m_38() { }

	inline virtual void m_40() { }

	inline virtual void m_48() { }

	inline virtual void SetCloningFrequency(int player, int frequency) { }

public:
	netSyncDataBase* m_syncData; // +8
	uint8_t m_updateLevels[32]; // + 16
	uint8_t m_minimumUpdateLevel; // +48
	char padding[7]; // +49
};

struct UnkNetObject_B0
{
	netObject* m_selfObject;
	void* m_reassignProviderFlags;
	char padC0[28];
	char padDC[16];
	char padEC[4];
};

class netObject
{
public:
	virtual ~netObject() = 0;

	CNetworkSyncDataULBase m_syncData; // +8

	uint16_t m_objectType; // +64
	uint16_t m_objectId; // +66
	char pad_2[1]; // +68
	uint8_t m_ownerId; // +69
	uint8_t m_nextOwnerId; // +70
	uint8_t m_isRemote; // +71

	uint8_t m_wantsToDelete : 1; // +72
	uint8_t m_unk1 : 1; // +73
	uint8_t m_unk2 : 1; // +74
	uint8_t m_shouldNotBeDeleted : 1; // +75

	uint32_t m_globalEntityFlags; // +76
	uint8_t m_minUpdateLevel; // +80 
	uint8_t m_unk51; // +81
	uint8_t m_unk52; // +82
	uint8_t m_unk53; // +83
	uint8_t m_unk54; // +84
	char pad55[3]; // +85

	void* m_gameObject; // + 88
	netBlender* m_netBlender; // +96
	netObject* m_netObject; // + 104

	uint32_t m_creationAckedPlayers; // +112
	uint32_t m_unk74; // +116
	uint32_t m_inScopePlayers; // +120
	uint32_t m_unk7C; // + 124
	uint32_t m_clonedPlayers; // +128
	uint32_t m_lastPlayersSyncUpdated; // +132
	uint32_t m_playerSyncUpdates; // +136
	char pad4[4]; // +140

	void* m_cloneSyncCB; // +144
	char pad5[8]; // +152

	uint32_t m_unkA0; // +160
	uint16_t m_unkA4; // +164
	uint8_t m_unkA6; // +166
	uint8_t m_unkA7; // +167
	char padA8[8]; // +168

	UnkNetObject_B0 m_unkB0; // +176


	inline bool IsCreationAckedByPlayer(int index)
	{
		return (m_creationAckedPlayers & (1 << index)) != 0;
	}

	inline netBlender* GetBlender()
	{
		return *(netBlender**)((uintptr_t)this + 96);
	}

private:
	template<typename TMember>
	inline static TMember get_member(void* ptr)
	{
		union member_cast
		{
			TMember function;
			struct
			{
				void* ptr;
				uintptr_t off;
			};
		};

		member_cast cast;
		cast.ptr = ptr;
		cast.off = 0;

		return cast.function;
	}

public:
#undef FORWARD_FUNC
#define FORWARD_FUNC(name, offset, ...)     \
	using TFn = decltype(&netObject::name); \
	void** vtbl = *(void***)(this);         \
	return (this->*(get_member<TFn>(vtbl[MapNetObjectMethod(offset) / 8])))(__VA_ARGS__);

	inline void* GetSyncData()
	{
		FORWARD_FUNC(GetSyncData, 0x20);
	}

	inline netSyncTree* GetSyncTree()
	{
		FORWARD_FUNC(GetSyncTree, 0x30);
	}

	inline bool CanSyncWithNoGameObject()
	{
		FORWARD_FUNC(CanSyncWithNoGameObject, 0x98);
	}

	inline void* GetGameObject()
	{
		FORWARD_FUNC(GetGameObject, 0xB0);
	}

	inline void* CreateNetBlender()
	{
		FORWARD_FUNC(CreateNetBlender, 0x108);
	}

	inline int GetSyncFrequency()
	{
		FORWARD_FUNC(GetSyncFrequency, 0x128);
	}

	inline void MainThreadUpdate()
	{
		FORWARD_FUNC(MainThreadUpdate, 0x160);
	}

	inline void DependencyThreadUpdate()
	{
		FORWARD_FUNC(DependencyThreadUpdate, 0x168);
	}

	inline void PostDependencyThreadUpdate()
	{
		FORWARD_FUNC(PostDependencyThreadUpdate, 0x170);
	}

	inline void StartSynchronising()
	{
		FORWARD_FUNC(StartSynchronising, 0x178);
	}

	inline bool CanClone(CNetGamePlayer* player, uint32_t* reason)
	{
		FORWARD_FUNC(CanClone, 0x188, player, reason);
	}

	inline bool CanSynchronise(bool unk, uint32_t* reason)
	{
		FORWARD_FUNC(CanSynchronise, 0x1A0, unk, reason);
	}

	inline bool CanPassControl(CNetGamePlayer* player, int type, int* outReason)
	{
		FORWARD_FUNC(CanPassControl, 0x1D0, player, type, outReason);
	}

	inline bool CanBlend(int* outReason)
	{
		FORWARD_FUNC(CanBlend, 0x1E0, outReason);
	}

	inline void ChangeOwner(CNetGamePlayer* player, int migrationType)
	{
		FORWARD_FUNC(ChangeOwner, 0x1F8, player, migrationType);
	}

	inline void OnRegistered()
	{
		FORWARD_FUNC(OnRegistered, 0x200);
	}

	inline void PostMigrate(int migrationType)
	{
		FORWARD_FUNC(PostMigrate, 0x258, migrationType);
	}

	inline void PostCreate()
	{
		FORWARD_FUNC(PostCreate, 0x240);
	}

	inline void PreSync()
	{
		FORWARD_FUNC(PreSync, 0x248);
	}

	inline void PostSync()
	{
		FORWARD_FUNC(PostSync, 0x250);
	}

	inline const char* GetTypeString()
	{
		FORWARD_FUNC(GetTypeString, 0x270);
	}

#undef FORWARD_FUNC

public:
	inline void Update()
	{
		MainThreadUpdate();
		DependencyThreadUpdate();
		PostDependencyThreadUpdate();
	}

	inline uint16_t GetObjectId()
	{
		return m_objectId;
	}

	inline uint16_t GetObjectType()
	{
		return m_objectType;
	}

	inline std::string ToString()
	{
		return fmt::sprintf("[netObj:%d:%s]", GetObjectId(), GetTypeString());
	}
};

netObject* CreateCloneObject(NetObjEntityType type, uint16_t objectId, uint8_t a2, int a3, int a4);
}
