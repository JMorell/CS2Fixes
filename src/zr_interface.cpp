/**
 * =============================================================================
 * ZombieReborn
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

#include "zr_interface.h"

#include "customio.h"
#include "playermanager.h"
#include "user_preferences.h"
#include "zombiereborn.h"

#include "entity/ccsplayercontroller.h"
#include "entity/ccsplayerpawn.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "tier0/memdbgon.h"

namespace
{
	std::vector<IZRListener*> s_vecListeners;
} // namespace

void ClearZREventListeners()
{
	s_vecListeners.clear();
}

class CZRInterface : public IZombieReborn
{
public:
	void AddListener(IZRListener* pListener) override;
	void RemoveListener(IZRListener* pListener) override;

	bool IsClientZombie(int iSlot) override;
	bool IsClientHuman(int iSlot) override;
	bool IsClientMotherZombie(int iSlot) override;

	bool InfectClient(int iSlot, int iAttacker, bool bMotherInfect) override;
	bool HumanizeClient(int iSlot) override;

	const char* GetActiveClassName(int iSlot) override;
	const char* GetPreferredClassName(int iSlot, int iTeam) override;
	bool GetClassInfo(const char* pszName, bool bZombie, ZRClassInfo* pOut) override;
	int GetClassCount(bool bZombie) override;
	bool GetClassByIndex(bool bZombie, int iIndex, ZRClassInfo* pOut) override;
	bool SelectClientClass(int iSlot, const char* pszName) override;

	bool IsRespawnEnabled() override;

	void IgniteClient(int iSlot, float flDuration, unsigned int iDamageType) override;
};

void CZRInterface::AddListener(IZRListener* pListener)
{
	if (!pListener)
		return;

	if (std::find(s_vecListeners.begin(), s_vecListeners.end(), pListener) != s_vecListeners.end())
		return;

	s_vecListeners.push_back(pListener);
}

void CZRInterface::RemoveListener(IZRListener* pListener)
{
	s_vecListeners.erase(std::remove(s_vecListeners.begin(), s_vecListeners.end(), pListener), s_vecListeners.end());
}

bool CZRInterface::IsClientZombie(int iSlot)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController)
		return false;

	if (g_ZRRoundState == EZRRoundState::ROUND_START)
		return false;

	return pController->m_iTeamNum() == CS_TEAM_T;
}

bool CZRInterface::IsClientHuman(int iSlot)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController)
		return false;

	const int iTeam = pController->m_iTeamNum();
	if (iTeam != CS_TEAM_T && iTeam != CS_TEAM_CT)
		return false;

	return !IsClientZombie(iSlot);
}

bool CZRInterface::IsClientMotherZombie(int iSlot)
{
	return ZR_IsMotherZombie(iSlot);
}

bool CZRInterface::InfectClient(int iSlot, int iAttacker, bool bMotherInfect)
{
	CCSPlayerController* pVictim = CCSPlayerController::FromSlot(iSlot);
	if (!pVictim)
		return false;

	if (bMotherInfect)
	{
		std::vector<SpawnPoint*> spawns = ZR_GetSpawns();
		if (spawns.empty())
			return false;

		ZR_InfectMotherZombie(pVictim, spawns);
		return true;
	}

	CCSPlayerController* pAttacker = iAttacker >= 0 ? CCSPlayerController::FromSlot(iAttacker) : nullptr;
	ZR_Infect(pAttacker, pVictim, false);
	return true;
}

bool CZRInterface::HumanizeClient(int iSlot)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController)
		return false;

	ZR_Cure(pController);
	return true;
}

const char* CZRInterface::GetActiveClassName(int iSlot)
{
	static char s_szActiveClassBuffers[4][64];
	static size_t s_nActiveBufIndex = 0;
	char* pszBuf = s_szActiveClassBuffers[s_nActiveBufIndex++ % 4];
	pszBuf[0] = '\0';

	if (!g_playerManager)
		return pszBuf;

	ZEPlayer* pPlayer = g_playerManager->GetPlayer(iSlot);
	if (!pPlayer)
		return pszBuf;

	std::shared_ptr<ZRClass> pClass = pPlayer->GetActiveZRClass();
	if (!pClass)
		return pszBuf;

	V_strncpy(pszBuf, pClass->szClassName.c_str(), 64);
	return pszBuf;
}

const char* CZRInterface::GetPreferredClassName(int iSlot, int iTeam)
{
	static char s_szPreferredClassBuffers[4][64];
	static size_t s_nPreferredBufIndex = 0;
	char* pszBuf = s_szPreferredClassBuffers[s_nPreferredBufIndex++ % 4];
	pszBuf[0] = '\0';

	if (!g_pUserPreferencesSystem)
		return pszBuf;

	const bool bZombie = (iTeam == ZR_TEAM_T);
	const char* pszPref = g_pUserPreferencesSystem->GetPreference(iSlot, bZombie ? ZOMBIE_CLASS_KEY_NAME : HUMAN_CLASS_KEY_NAME);
	if (pszPref)
		V_strncpy(pszBuf, pszPref, 64);

	return pszBuf;
}

namespace
{
	void CollectClasses(bool bZombie, std::vector<std::shared_ptr<ZRClass>>& vecOut)
	{
		if (!g_pZRPlayerClassManager)
			return;

		g_pZRPlayerClassManager->GetZRClassList(bZombie ? CS_TEAM_T : CS_TEAM_CT, vecOut, nullptr);
	}

	bool FillClassInfo(const std::shared_ptr<ZRClass>& pClass, ZRClassInfo* pOut)
	{
		if (!pClass || !pOut)
			return false;

		V_strncpy(pOut->szName, pClass->szClassName.c_str(), sizeof(pOut->szName));

		pOut->szModel[0] = '\0';
		if (!pClass->vecModels.empty())
			V_strncpy(pOut->szModel, pClass->vecModels[0]->szModelPath.c_str(), sizeof(pOut->szModel));

		pOut->iTeam = pClass->iTeam;
		pOut->iHealth = pClass->iHealth;
		pOut->flSpeed = pClass->flSpeed;
		pOut->flGravity = pClass->flGravity;
		pOut->flScale = pClass->flScale;
		pOut->iAdminFlag = static_cast<int>(pClass->iAdminFlag);
		pOut->flKnockback = 0.0f;

		if (pClass->iTeam == CS_TEAM_T)
		{
			auto pZombieClass = std::static_pointer_cast<ZRZombieClass>(pClass);
			if (pZombieClass)
				pOut->flKnockback = pZombieClass->flKnockback;
		}

		return true;
	}
} // namespace

bool CZRInterface::GetClassInfo(const char* pszName, bool bZombie, ZRClassInfo* pOut)
{
	if (!pszName || !pOut)
		return false;

	std::vector<std::shared_ptr<ZRClass>> vecClasses;
	CollectClasses(bZombie, vecClasses);

	for (const auto& pClass : vecClasses)
	{
		if (pClass && !V_stricmp(pClass->szClassName.c_str(), pszName))
			return FillClassInfo(pClass, pOut);
	}

	return false;
}

int CZRInterface::GetClassCount(bool bZombie)
{
	std::vector<std::shared_ptr<ZRClass>> vecClasses;
	CollectClasses(bZombie, vecClasses);
	return static_cast<int>(vecClasses.size());
}

bool CZRInterface::GetClassByIndex(bool bZombie, int iIndex, ZRClassInfo* pOut)
{
	std::vector<std::shared_ptr<ZRClass>> vecClasses;
	CollectClasses(bZombie, vecClasses);

	if (iIndex < 0 || iIndex >= static_cast<int>(vecClasses.size()))
		return false;

	return FillClassInfo(vecClasses[iIndex], pOut);
}

bool CZRInterface::SelectClientClass(int iSlot, const char* pszName)
{
	if (!pszName || !g_pUserPreferencesSystem)
		return false;

	std::vector<std::shared_ptr<ZRClass>> vecZombie, vecHuman;
	CollectClasses(true, vecZombie);
	CollectClasses(false, vecHuman);

	for (const auto& vecClasses : {vecZombie, vecHuman})
	{
		for (const auto& pClass : vecClasses)
		{
			if (!pClass || V_stricmp(pClass->szClassName.c_str(), pszName))
				continue;

			g_pUserPreferencesSystem->SetPreference(iSlot, pClass->iTeam == CS_TEAM_T ? ZOMBIE_CLASS_KEY_NAME : HUMAN_CLASS_KEY_NAME, pClass->szClassName.c_str());
			return true;
		}
	}

	return false;
}

bool CZRInterface::IsRespawnEnabled()
{
	return ZR_GetRespawnEnabled();
}

void CZRInterface::IgniteClient(int iSlot, float flDuration, unsigned int iDamageType)
{
	CCSPlayerController* pController = CCSPlayerController::FromSlot(iSlot);
	if (!pController)
		return;

	CCSPlayerPawn* pPawn = pController->GetPlayerPawn();
	if (!pPawn)
		return;

	IgnitePawn(pPawn, flDuration, nullptr, nullptr, nullptr, DamageTypes_t(iDamageType));
}

namespace
{
	CZRInterface s_ZRInterface;
} // namespace

IZombieReborn* GetPublicZRInterface()
{
	return &s_ZRInterface;
}

// =============================================================================
// Event Notification Dispatchers
// Safe snapshots used to allow reentrant modifications during callbacks.
// =============================================================================

void FireClientInfected(int iSlot, int iAttacker, bool bMotherInfect)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnClientInfected(iSlot, iAttacker, bMotherInfect);
	}
}

void FireClientHumanPost(int iSlot, bool bRespawned)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnClientHumanPost(iSlot, bRespawned);
	}
}

void FireClientRespawned(int iSlot)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnClientRespawned(iSlot);
	}
}

void FireClientRespawnScheduled(int iSlot, bool bWillRespawn)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnClientRespawnScheduled(iSlot, bWillRespawn);
	}
}

void FireClientZteled(int iSlot, bool bForce)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnClientZteled(iSlot, bForce);
	}
}

void FireClientIgnited(int iSlot, float flDuration)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnClientIgnited(iSlot, flDuration);
	}
}

void FireClassesLoaded()
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnClassesLoaded();
	}
}

void FireRoundEndPlayer(int iSlot, bool bAlive, bool bZombie)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnRoundEndPlayer(iSlot, bAlive, bZombie);
	}
}

void FireLeaderChanged(int iSlot, bool bIsLeader)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnLeaderChanged(iSlot, bIsLeader);
	}
}

void FireItemPickup(int iSlot, const char* pszItem)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnItemPickup(iSlot, pszItem);
	}
}

void FireItemDrop(int iSlot, const char* pszItem)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnItemDrop(iSlot, pszItem);
	}
}

void FireItemTransfer(int iFrom, int iTo, const char* pszItem)
{
	auto vecListeners = s_vecListeners;
	for (IZRListener* pListener : vecListeners)
	{
		if (pListener)
			pListener->OnItemTransfer(iFrom, iTo, pszItem);
	}
}
