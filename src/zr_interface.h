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

#pragma once

#include "public/izombiereborn.h"

IZombieReborn* GetPublicZRInterface();
void ClearZREventListeners();

// Notification dispatchers
void FireClientInfected(int iSlot, int iAttacker, bool bMotherInfect);
void FireClientHumanPost(int iSlot, bool bRespawned);
void FireClientRespawned(int iSlot);
void FireClientRespawnScheduled(int iSlot, bool bWillRespawn);
void FireClientZteled(int iSlot, bool bForce);
void FireClientIgnited(int iSlot, float flDuration);
void FireClassesLoaded();
void FireRoundEndPlayer(int iSlot, bool bAlive, bool bZombie);
void FireLeaderChanged(int iSlot, bool bIsLeader);
void FireItemPickup(int iSlot, const char* pszItem);
void FireItemDrop(int iSlot, const char* pszItem);
void FireItemTransfer(int iFrom, int iTo, const char* pszItem);
