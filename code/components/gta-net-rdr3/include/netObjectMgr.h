#pragma once

#include <netObject.h>
#include <CrossBuildRuntime.h>
#include <atArray.h>

class CNetGamePlayer;

namespace rage
{
class netObjectMgr;

class netObjectMgrReassignProvider
{
  netObjectMgr *m_netObjectMgr;
  void *netObjectMgrMessageHandler;
  void *m_netBandwidthMgr;
public:
  uint32_t unk_20;
  uint32_t m_unkFlags1[1];
  uint32_t m_ipcEventRefs[1];
  uint32_t m_unkFlags3[1];
  void *m_sysIpcCreateEvent;
};

struct netObjectReassignMgr
{
  netObjectMgrReassignProvider* m_reassignProvider;
  netObjectMgrReassignProvider* m_reassign;
};

class netObjectMgr
{
public:
	virtual ~netObjectMgr() = 0;

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

#define FORWARD_FUNC(name, offset, ...)    \
	using TFn = decltype(&netObjectMgr::name); \
	void** vtbl = *(void***)(this);        \
	return (this->*(get_member<TFn>(vtbl[(offset / 8) + ((offset > 0x68) ? (xbr::IsGameBuildOrGreater<1436>() ? 1 : 0) : 0)])))(__VA_ARGS__)

public:
	inline void UnregisterNetworkObject(rage::netObject* object, int reason, bool force1, bool force2)
	{
		FORWARD_FUNC(UnregisterNetworkObject, 0x38, object, reason, force1, force2);
	}

	inline void ChangeOwner(rage::netObject* object, CNetGamePlayer* player, int migrationType)
	{
		FORWARD_FUNC(ChangeOwner, 0x40, object, player, migrationType);
	}

	inline void RegisterNetworkObject(rage::netObject* entity)
	{
		// in 1436 R* added 1 more method right before RegisterNetworkObject
		FORWARD_FUNC(RegisterNetworkObject, 0x70, entity);
	}

	inline void PreMultithreadedUpdate()
	{
		FORWARD_FUNC(PreMultithreadedUpdate, 0xA0);
	}
    
	inline void PostMultithreadedUpdate()
	{
		FORWARD_FUNC(PostMultithreadedUpdate, 0xA8);
	}	

private:
	struct atDNetObjectNode
	{
		virtual ~atDNetObjectNode();

		netObject* object;
		atDNetObjectNode* next;
	};

	struct ObjectHolder
	{
		atDNetObjectNode* objects;
		netObject** unk; // might not just be a netObject**
	};

	struct UnkNetObjectMgr_208
	{
	  char unk[148];
	};

	struct UnkNetObjectMgr_1488
	{
	  char unk[148];
	};

	struct SyncMessageInfoQueues
	{
	  char unk[24];
	};

private:
	ObjectHolder m_objects[32]; // +8
	UnkNetObjectMgr_208 m_unk208[32]; // +520
	UnkNetObjectMgr_1488 m_unk1488[32]; // +5256

	bool m_isInitialized; // +9992
	bool unk_2709; // +9993
	char pad_270A[6]; // +9994
	void* m_netObjectIdMgr[2]; // +10,000
	void* m_firstObjectIdMgrPtr; // 43,200
	void* m_secondObjectIdMgrPtr; // 43,208
	netObjectReassignMgr m_netObjectReassignMgr;

public:
	netObjectMgrReassignProvider m_reassignProvider;
	_RTL_CRITICAL_SECTION m_autoLock;

private:
	void* m_unkLockBE28;
	char pad_BE30[24];
	SyncMessageInfoQueues m_syncMessageInfoQueues[32];
	char pad_C148[8];
	int unk_C150;
	char pad_C154[4];
	BYTE m_isSortedUnkC160;
	char unk_C159[7];
	atArray<void*> m_unkC160;
	uint16_t m_deferredQueueObjectIds[10];
	char pad_C184[4];
	uint32_t m_currentProcessingDeferredIndex;
	uint32_t m_numDeferredQueueObjectIds;
	rage::netObject** m_registeredClones;
	uint16_t m_registeredClonesCount;
	uint16_t m_registeredClonesSize;
	char pad_C19C[4];
	void* unk_C1A0;

public:
	template<typename T>
	inline void ForAllNetObjects(int playerId, const T& callback)
	{
		for (auto node = m_objects[playerId].objects; node; node = node->next)
		{
			if (node->object)
			{
				callback(node->object);
			}
		}
	}

	netObject* GetNetworkObject(uint16_t id, bool a3);

	static netObjectMgr* GetInstance();
};
}
