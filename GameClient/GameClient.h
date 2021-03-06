/* ========================================================================
$File: GameClient.h
$Date: 7/11/2018
$Revision: $
$Creator: Andrew Craven
(C) Copyright 2018 by Andrew Craven

This file is part of SampleMMOGame.

SampleMMOGame is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

SampleMMOGame is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with SampleMMOGame. If not, see <https://www.gnu.org/licenses/>.

======================================================================== */

#pragma once

#include "GameShared.h"

struct client_state {
	player Player;
	enemy CurrentEnemy;
	room CurrentRoom;
	combat_update CombatData;
};