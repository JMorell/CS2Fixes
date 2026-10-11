/**
 * =============================================================================
 * ZombieReborn (ZR) Plugin API
 * Copyright (C) 2023-2026 Source2ZE
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

// Required for ISmmAPI in ZR_Connect
#include <ISmmPlugin.h>

#define ZR_INTERFACE "ZR001"
#define ZOMBIEREBORN_INTERFACE ZR_INTERFACE

// Damage type bits, mirrored so callers do not need the SDK.
#define ZR_DMG_FALL (1u << 5)
#define ZR_DMG_BURN 8u

// Team constants matching CS_TEAM_*
#define ZR_TEAM_NONE 0
#define ZR_TEAM_SPECTATOR 1
#define ZR_TEAM_T 2  // Zombie
#define ZR_TEAM_CT 3 // Human
#define ZR_TEAM_ALL -1

// =============================================================================
// Class description.
// Safe value type to copy across plugin boundaries without engine dependencies.
// =============================================================================
struct ZRClassInfo
{
	char szName[64];
	char szModel[256];
	int iTeam;		 // ZR_TEAM_T (zombie) or ZR_TEAM_CT (human)
	int iHealth;
	float flSpeed;
	float flGravity;
	float flScale;
	float flKnockback; // Zombie classes only; 0.0 for humans
	int iAdminFlag;	   // 0 means available to everyone
};

// =============================================================================
// IZRListener - notifications pushed by ZombieReborn (forward callbacks).
// =============================================================================
class IZRListener
{
public:
	virtual ~IZRListener() = default;

	// -- Infection -------------------------------------------------------------
	virtual void OnClientInfected(int iSlot, int iAttacker, bool bMotherInfect) {}
	virtual void OnClientHumanPost(int iSlot, bool bRespawned) {}

	// -- Respawn ---------------------------------------------------------------
	virtual void OnClientRespawned(int iSlot) {}
	virtual void OnClientRespawnScheduled(int iSlot, bool bWillRespawn) {}

	// -- Ztele -----------------------------------------------------------------
	virtual void OnClientZteled(int iSlot, bool bForce) {}

	// -- Napalm ----------------------------------------------------------------
	virtual void OnClientIgnited(int iSlot, float flDuration) {}

	// -- Classes ---------------------------------------------------------------
	virtual void OnClassesLoaded() {}

	// -- Round -----------------------------------------------------------------
	virtual void OnRoundEndPlayer(int iSlot, bool bAlive, bool bZombie) {}

	// -- Leader ----------------------------------------------------------------
	virtual void OnLeaderChanged(int iSlot, bool bIsLeader) {}

	// -- EntWatch items --------------------------------------------------------
	virtual void OnItemPickup(int iSlot, const char* pszItem) {}
	virtual void OnItemDrop(int iSlot, const char* pszItem) {}
	virtual void OnItemTransfer(int iFrom, int iTo, const char* pszItem) {}
};

using IZombieRebornListener = IZRListener;

// =============================================================================
// IZombieReborn - queries and actions available to other plugins (natives).
// =============================================================================
class IZombieReborn
{
public:
	virtual ~IZombieReborn() = default;

	// -- Listener registration -------------------------------------------------
	virtual void AddListener(IZRListener* pListener) = 0;
	virtual void RemoveListener(IZRListener* pListener) = 0;

	// -- Infection state -------------------------------------------------------
	virtual bool IsClientZombie(int iSlot) = 0;
	virtual bool IsClientHuman(int iSlot) = 0;
	virtual bool IsClientMotherZombie(int iSlot) = 0;

	// -- Infection actions -----------------------------------------------------
	virtual bool InfectClient(int iSlot, int iAttacker, bool bMotherInfect) = 0;
	virtual bool HumanizeClient(int iSlot) = 0;

	// -- Classes ---------------------------------------------------------------
	virtual const char* GetActiveClassName(int iSlot) = 0;
	virtual const char* GetPreferredClassName(int iSlot, int iTeam) = 0;
	virtual bool GetClassInfo(const char* pszName, bool bZombie, ZRClassInfo* pOut) = 0;
	virtual int GetClassCount(bool bZombie) = 0;
	virtual bool GetClassByIndex(bool bZombie, int iIndex, ZRClassInfo* pOut) = 0;
	virtual bool SelectClientClass(int iSlot, const char* pszName) = 0;

	// -- Respawn ---------------------------------------------------------------
	virtual bool IsRespawnEnabled() = 0;

	// -- Napalm / Ignite -------------------------------------------------------
	virtual void IgniteClient(int iSlot, float flDuration, unsigned int iDamageType = ZR_DMG_BURN) = 0;
};

using IZR = IZombieReborn;

// =============================================================================
// Connection and Lifecycle Management
// =============================================================================

inline IZombieReborn* g_pZR = nullptr;

inline bool ZR_Loaded()
{
	return g_pZR != nullptr;
}

inline IZombieReborn* ZR()
{
	return g_pZR;
}

class CZRListener;

namespace ZRPluginDetail
{
	inline std::vector<CZRListener*>& Listeners()
	{
		static std::vector<CZRListener*> s_vecListeners;
		return s_vecListeners;
	}

	inline void Add(CZRListener* pListener)
	{
		auto& vec = Listeners();
		if (std::find(vec.begin(), vec.end(), pListener) == vec.end())
			vec.push_back(pListener);
	}

	inline void Remove(CZRListener* pListener)
	{
		auto& vec = Listeners();
		vec.erase(std::remove(vec.begin(), vec.end(), pListener), vec.end());
	}
} // namespace ZRPluginDetail

/**
 * Inherit from CZRListener to receive ZombieReborn forwards.
 * Auto-registers with ZombieReborn upon connection.
 */
class CZRListener : public IZRListener
{
public:
	CZRListener()
	{
		ZRPluginDetail::Add(this);
		if (g_pZR)
			g_pZR->AddListener(this);
	}

	~CZRListener() override
	{
		if (g_pZR)
			g_pZR->RemoveListener(this);
		ZRPluginDetail::Remove(this);
	}

	CZRListener(const CZRListener&) = delete;
	CZRListener& operator=(const CZRListener&) = delete;
};

using CZombieRebornListener = CZRListener;

/**
 * Resolves the ZombieReborn interface from Metamod:Source.
 * Returns true if resolved or if not required.
 */
inline bool ZR_Connect(ISmmAPI* ismm, bool bRequired = false)
{
	g_pZR = nullptr;

	if (ismm)
	{
		int ret = 0;
		PluginId id = 0;
		void* pInterface = ismm->MetaFactory(ZR_INTERFACE, &ret, &id);

		if (pInterface && ret == META_IFACE_OK)
			g_pZR = static_cast<IZombieReborn*>(pInterface);
	}

	if (g_pZR)
	{
		for (CZRListener* pListener : ZRPluginDetail::Listeners())
			g_pZR->AddListener(pListener);
	}

	return !bRequired || g_pZR != nullptr;
}

/**
 * Detaches all listeners and disconnects from ZombieReborn.
 */
inline void ZR_Disconnect()
{
	if (g_pZR)
	{
		for (CZRListener* pListener : ZRPluginDetail::Listeners())
			g_pZR->RemoveListener(pListener);
	}

	g_pZR = nullptr;
}

// =============================================================================
// Inline Natives (Safe wrappers with null-checking fallback)
// =============================================================================

inline bool ZR_IsClientZombie(int iSlot)
{
	return g_pZR && g_pZR->IsClientZombie(iSlot);
}

inline bool ZR_IsClientHuman(int iSlot)
{
	return g_pZR && g_pZR->IsClientHuman(iSlot);
}

inline bool ZR_IsClientMotherZombie(int iSlot)
{
	return g_pZR && g_pZR->IsClientMotherZombie(iSlot);
}

inline bool ZR_InfectClient(int iSlot, int iAttacker = -1, bool bMotherInfect = false)
{
	return g_pZR && g_pZR->InfectClient(iSlot, iAttacker, bMotherInfect);
}

inline bool ZR_HumanizeClient(int iSlot)
{
	return g_pZR && g_pZR->HumanizeClient(iSlot);
}

inline const char* ZR_GetActiveClassName(int iSlot)
{
	return g_pZR ? g_pZR->GetActiveClassName(iSlot) : "";
}

inline const char* ZR_GetPreferredClassName(int iSlot, int iTeam)
{
	return g_pZR ? g_pZR->GetPreferredClassName(iSlot, iTeam) : "";
}

inline bool ZR_GetClassInfo(const char* pszName, bool bZombie, ZRClassInfo* pOut)
{
	return g_pZR && g_pZR->GetClassInfo(pszName, bZombie, pOut);
}

inline int ZR_GetClassCount(bool bZombie)
{
	return g_pZR ? g_pZR->GetClassCount(bZombie) : 0;
}

inline bool ZR_GetClassByIndex(bool bZombie, int iIndex, ZRClassInfo* pOut)
{
	return g_pZR && g_pZR->GetClassByIndex(bZombie, iIndex, pOut);
}

inline bool ZR_SelectClientClass(int iSlot, const char* pszName)
{
	return g_pZR && g_pZR->SelectClientClass(iSlot, pszName);
}

inline bool ZR_IsRespawnEnabled()
{
	return g_pZR && g_pZR->IsRespawnEnabled();
}

inline void ZR_IgniteClient(int iSlot, float flDuration, unsigned int iDamageType = ZR_DMG_BURN)
{
	if (g_pZR)
		g_pZR->IgniteClient(iSlot, flDuration, iDamageType);
}
