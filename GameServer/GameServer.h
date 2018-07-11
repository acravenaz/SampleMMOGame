/* ========================================================================
$File: GameServer.h
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

#define SERVER_MAX_PLAYERS 10

#define SERVER_TICK_RATE (float)30.0

struct thread_message_queue {
	message *IncomingMessage;
	message *OutgoingMessage;
};

struct thread_handler {
	HANDLE ListenThreadHandle;
	DWORD ListenThreadID;
	HANDLE ClientThreadHandles[MAX_CLIENTS];
	DWORD ClientThreadIDs[MAX_CLIENTS];
	//uint32 ClientThreadCount;
	uint32 PlayerIDs[MAX_CLIENTS];
	thread_message_queue MessageQueues[MAX_CLIENTS];
};

struct listen_thread_data {
	SOCKET ListenSocket;
	thread_handler *ThreadHandler;
	//uint32 ThreadID;
};

struct client_thread_data {
	SOCKET ClientSocket;
	thread_handler *ThreadHandler;
	uint32 ThreadID;
};

struct game_state {
	uint32 PlayerCount;
	player Players[SERVER_MAX_PLAYERS + 1];
	room Rooms[6];
};