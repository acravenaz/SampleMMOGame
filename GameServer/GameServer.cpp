/* ========================================================================
$File: GameServer.cpp
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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <timeapi.h>
#include <sqlite3.h>

#include "GameServer.h"

using namespace std;

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "sqlite3.lib")

//NOTE: This is a global for now.
static int64_t GlobalPerfCountFrequency;

//NOTE: called at termination points to hold the console window open
//TODO: make a proper "press any key to exit" function
void PressEnterToContinue() {
	printf("Press Enter to continue\n");
	string temp;
	getline(cin, temp);
}

//NOTE: Initialize WinSock!
SOCKET Win32_WinSockInit() {
	printf("Initializing Server WinSock...\n");
	WSADATA WinSockData;
	int WinSockLoadResult = WSAStartup(MAKEWORD(2, 2), &WinSockData);
	if (WinSockLoadResult != 0) {
		printf("WinSock load failed!: error code %d\n", WinSockLoadResult);
		return INVALID_SOCKET;
	}
	else {
		printf("WinSock loaded successfully!\n");
	}

	addrinfo *ServerAddrInfo;
	addrinfo hints;

	ZeroMemory(&hints, sizeof(hints));

	hints.ai_family = AF_INET; // TODO: forcing IPv4 for now. This should be changed to AF_UNSPEC later to allow v6!
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	int GetAddrInfoResult = getaddrinfo(0, DEFAULT_PORT, &hints, &ServerAddrInfo);
	if (GetAddrInfoResult != 0) {
		printf("Failed to retrieve address for socket!: %d\n", GetAddrInfoResult);
		return INVALID_SOCKET;
	}
	else {
		printf("WinSock retrieved a socket address successfully!\n");
	}

	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = socket(ServerAddrInfo->ai_family, ServerAddrInfo->ai_socktype, ServerAddrInfo->ai_protocol);

	if (ListenSocket == INVALID_SOCKET) {
		printf("Error getting socket: %ld\n", WSAGetLastError());
		freeaddrinfo(ServerAddrInfo);
		return INVALID_SOCKET;
	}
	else {
		printf("Retrieved Socket successfully.\n");
	}

	//Set up TCP socket to listen for data
	int BindResult = bind(ListenSocket, ServerAddrInfo->ai_addr, (int)ServerAddrInfo->ai_addrlen);
	if (BindResult == SOCKET_ERROR) {
		printf("Failed to bind socket: %d\n", WSAGetLastError());
		freeaddrinfo(ServerAddrInfo);
		closesocket(ListenSocket);
		return INVALID_SOCKET;
	}
	else {
		printf("Bound socket successfully.\n");
	}
	// At this point, the addrinfo is not needed anymore and can be discarded
	freeaddrinfo(ServerAddrInfo);

	int ListenResult = listen(ListenSocket, SOMAXCONN);
	if (ListenResult == SOCKET_ERROR) {
		printf("Failed to listen on socket: %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		return INVALID_SOCKET;
	}
	else {
		printf("Listening on socket successfully.\n");
	}
	return ListenSocket;
}

// NOTE: Entry point for each thread created when a new client connects on the listening socket
DWORD WINAPI Thread_ClientConnection(void *Data) {
	//SOCKET ClientSocket = (SOCKET)Data;
	client_thread_data *ThreadData = (client_thread_data *)Data;
	thread_message_queue *MessageQueue = &ThreadData->ThreadHandler->MessageQueues[ThreadData->ThreadID];

	bool ClientConnected = true;
	while (ClientConnected) {

		char ReceiveBuffer[DEFAULT_BUFFER_LENGTH];
		int BytesReceived = recv(ThreadData->ClientSocket, ReceiveBuffer, DEFAULT_BUFFER_LENGTH, 0);

		if (BytesReceived > 0) {
			//NOTE: Intercept MC_LOGOFF and set ClientConnected to false, so the thread can properly
			//		terminate before informing main() that the client disconnected with MC_DISCONNECT
			message *IncMessage = (message *)ReceiveBuffer;
			if (IncMessage->Command != MC_LOGOFF) {
				MessageQueue->IncomingMessage = IncMessage;
				bool MessageSent = false;
				while (MessageSent == false) {
					message *Message = MessageQueue->OutgoingMessage;
					if (Message) {
						char *OutgoingBuffer = (char *)Message;
						int BytesSent = send(ThreadData->ClientSocket, OutgoingBuffer, DEFAULT_BUFFER_LENGTH, 0);
						if (BytesSent > 0) {
							//printf("Sent %d bytes.\n", BytesSent);
						}
						else {
							printf("Send Failed: %d\n", WSAGetLastError());
							ClientConnected = false;
						}
						MessageSent = true;
						MessageQueue->OutgoingMessage = 0;
					}
				}
			}
			else {
				ClientConnected = false;
			}
		}
		else if (BytesReceived == 0) {
			printf("Client closed connection.\n");
			ClientConnected = false;
		}
		else {
			printf("Receive Failed: %d\n", WSAGetLastError());
			ClientConnected = false;
		}
	}
	printf("Closing connection.\n");
	// NOTE: Shut down connection when done
	int ShutdownResult = shutdown(ThreadData->ClientSocket, SD_SEND);
	if (ShutdownResult == SOCKET_ERROR) {
		printf("Failed to close connection: %d\n", WSAGetLastError());
		closesocket(ThreadData->ClientSocket);
		//NOTE: Since thread is terminating, this needs to be on the heap so it
		//		doesn't get popped off the stack before the main thread can handle it
		message *DisconnectMessage = (message *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(message));
		DisconnectMessage->Command = MC_DISCONNECT;
		MessageQueue->IncomingMessage = DisconnectMessage;
		return 1;
	}
	else {
		printf("Closed connection successfully.\n");
		//NOTE: Since thread is terminating, this needs to be on the heap so it
		//		doesn't get popped off the stack before the main thread can handle it
		message *DisconnectMessage = (message *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(message));
		DisconnectMessage->Command = MC_DISCONNECT;
		MessageQueue->IncomingMessage = DisconnectMessage;
	}
	return 0;
}

// NOTE: Entry point for the listener thread kicked off in main()
DWORD WINAPI Thread_ListenForConnections(void *Data) {
	//SOCKET ListenSocket = (SOCKET)Data;
	listen_thread_data *ThreadData = (listen_thread_data *)Data;
	printf("Waiting for Client Connection...\n");
	SOCKET ClientSocket = INVALID_SOCKET;
	while (ClientSocket = accept(ThreadData->ListenSocket, NULL, NULL)) {
		if (ClientSocket != INVALID_SOCKET) {
			// TODO: Search through thread handles for ones that have been released
			printf("Client Connection accepted.\n");
			client_thread_data *ClientThreadData = (client_thread_data *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(client_thread_data));
			ClientThreadData->ClientSocket = ClientSocket;
			ClientThreadData->ThreadHandler = ThreadData->ThreadHandler;
			//ClientThreadData->ThreadID = ThreadData->ThreadHandler->ClientThreadCount++;
			bool FoundUnusedThread = false;
			for (int i = 0; i < MAX_CLIENTS && FoundUnusedThread == false; i++) {
				if (ThreadData->ThreadHandler->ClientThreadIDs[i] == 0) {
					ClientThreadData->ThreadID = i;
					printf("Success! Starting thread %d.\n", ClientThreadData->ThreadID);
					ThreadData->ThreadHandler->ClientThreadHandles[ClientThreadData->ThreadID] =
						CreateThread(NULL, 0, Thread_ClientConnection, (void *)ClientThreadData, 0,
							&ThreadData->ThreadHandler->ClientThreadIDs[ClientThreadData->ThreadID]);
					FoundUnusedThread = true;
				}
			}
			if (!FoundUnusedThread) {
				printf("Max clients reached! No Available threads.\n");
				closesocket(ClientSocket);
			}
			ClientSocket = INVALID_SOCKET;
		}
		else {
			printf("Failed to accept client connection: %d\n", WSAGetLastError());
			closesocket(ClientSocket);
			ClientSocket = INVALID_SOCKET;
		}
	}
	return 0;
}


inline LARGE_INTEGER Win32_GetWallClock() {
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline float Win32_GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
	float Result = ((float)(End.QuadPart - Start.QuadPart) /
		(float)GlobalPerfCountFrequency);
	return(Result);
}

bool Game_PlayerHasKey(player *Player, uint32 KeyID) {
	bool HasKey = false;
	for (int i = 0; i < CountElements(Player->Inventory); i++) {
		if (Player->Inventory[i].Type == 3) {
			if (Player->Inventory[i].Param[0] == KeyID) {
				HasKey = true;
				break;
			}
		}
	}
	return HasKey;
}

bool Game_CheckPlayerActive(game_state *GameState, string LogonName) {
	bool PlayerActive = false;
	for (uint32 i = 1; i <= SERVER_MAX_PLAYERS; i++) {
		if (LogonName.compare(GameState->Players[i].Name) == 0) {
			if (GameState->Players[i].State != INACTIVE) {
				PlayerActive = true;
				break;
			}
		}
	}
	return PlayerActive;
}

//NOTE: Search the player list and find the lowest available player ID to use.
uint32 Game_GetNextPlayerID(game_state *GameState) {
	uint32 PlayerID = 0;
	for (uint32 i = 1; i <= SERVER_MAX_PLAYERS; i++) {
		if (GameState->Players[i].State == INACTIVE) {
			PlayerID = i;
			break;
		}
	}
	return PlayerID;
}

//TODO: Does this need to return a pointer to the player object? 
//		This function already requires the caller to know the PlayerID,
//		So the parent could just retrieve the player object from GameState
player *Game_IntitializePlayer(game_state *GameState, uint32 PlayerID, string PlayerName) {
	player *Player;
	Player = &GameState->Players[PlayerID];
	Player->Name = PlayerName;
	Player->CurrentRoom = 1;
	Player->HPMax = 10;
	Player->HP = 10;
	Player->BaseATK = 2;
	Player->BaseDEF = 1;
	return Player;
}

uint32 Game_LogonPlayer(game_state *GameState, string LogonName) {
	uint32 PlayerID = 0;
	PlayerID = Game_GetNextPlayerID(GameState);

	if (PlayerID) {
		bool PlayerLoggedOn = false;
		PlayerLoggedOn = Game_CheckPlayerActive(GameState, LogonName);
		if (!PlayerLoggedOn) {
			//NOTE: Check the SQL database to find out if a player with the given name already exists
			player *Player = &GameState->Players[PlayerID];
			sqlite3 *DB;
			int Result = sqlite3_open("GameData.db", &DB);
			if (Result == SQLITE_OK) {
				char PlayerInfoBuffer[DEFAULT_BUFFER_LENGTH];
				_snprintf_s(PlayerInfoBuffer, DEFAULT_BUFFER_LENGTH, "SELECT Name, Room, HP, Weapon, Shield FROM Players WHERE Name='%s';", LogonName.c_str());

				sqlite3_stmt *PlayerInfoStmt;
				Result = sqlite3_prepare_v2(DB, PlayerInfoBuffer, -1, &PlayerInfoStmt, 0);
				if (Result == SQLITE_OK) {
					//NOTE: This query returns SQLITE_OK even if the player is not found
					//		and the query statement contains nothing. However, sqlite3_step() will not return
					//		SQLITE_ROW when a blank query is returned (even on the first call),
					//		so we don't risk writing invalid data to our new player object. In this case, 
					//		the player object is simply left in its initial state and started as a brand
					//		new player (which is intended functionality).
					printf("Attempting to load player info from Player database.\n");
					player *Player = Game_IntitializePlayer(GameState, PlayerID, LogonName);
					Player->State = NEUTRAL;
					//TODO: We're not catching any errors here!
					Result = sqlite3_step(PlayerInfoStmt);
					if (Result == SQLITE_ROW) {
						printf("Player found. Loading info from DB.\n");
						Player->Name = (char *)sqlite3_column_text(PlayerInfoStmt, 0);
						Player->CurrentRoom = sqlite3_column_int(PlayerInfoStmt, 1);
						Player->HP = sqlite3_column_int(PlayerInfoStmt, 2);
						Player->Weapon = sqlite3_column_int(PlayerInfoStmt, 3);
						Player->Shield = sqlite3_column_int(PlayerInfoStmt, 4);

						sqlite3_finalize(PlayerInfoStmt);

						//NOTE: Begin loading inventory info
						char InventoryInfoBuffer[DEFAULT_BUFFER_LENGTH];
						_snprintf_s(InventoryInfoBuffer, DEFAULT_BUFFER_LENGTH, "SELECT Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8, Slot9, Slot10 FROM Inventories WHERE Name='%s';", LogonName.c_str());
						sqlite3_stmt *InventoryStmt;
						Result = sqlite3_prepare_v2(DB, InventoryInfoBuffer, -1, &InventoryStmt, 0);
						if (Result == SQLITE_OK) {
							printf("Player inventory found. Loading player inventory from DB.\n");
							Result = sqlite3_step(InventoryStmt);
							if (Result == SQLITE_ROW) {
								for (int i = 1; i <= PLAYER_MAX_INVENTORY; i++) {
									int ItemID = 0;
									ItemID = sqlite3_column_int(InventoryStmt, i - 1);
									Player->Inventory[i].ID = ItemID;
									//TODO: THESE SHOULD NOT BE MAGIC NUMBERS!!!!
									//		Item information should get pulled from a separate table?
									if (ItemID == 1) {
										Player->Inventory[i].Name = "Health Potion";
										Player->Inventory[i].Type = ITEM_CONSUMABLE;
										Player->Inventory[i].Param[0] = 6;
									}
									else if (ItemID == 2) {
										Player->Inventory[i].Name = "Sword";
										Player->Inventory[i].Type = ITEM_WEAPON;
										Player->Inventory[i].Param[0] = 3;
										Player->Inventory[i].Param[1] = 1;
									}
									else if (ItemID == 3) {
										Player->Inventory[i].Name = "Door Key";
										Player->Inventory[i].Type = ITEM_KEY;
										Player->Inventory[i].Param[0] = 1;
									}
									else if (ItemID == 4) {
										Player->Inventory[i].Name = "Chest Key";
										Player->Inventory[i].Type = ITEM_KEY;
										Player->Inventory[i].Param[0] = 2;
									}
									else if (ItemID == 5) {
										Player->Inventory[i].Name = "Shield";
										Player->Inventory[i].Type = ITEM_SHIELD;
										Player->Inventory[i].Param[0] = 0;
										Player->Inventory[i].Param[1] = 3;
									}
								}
							}
							sqlite3_finalize(InventoryStmt);
						}
						else {
							fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(DB));
							sqlite3_close(DB);
						}
					}
					else {
						printf("Player not found, creating new player...\n");
					}
				}
				else {
					fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(DB));
					sqlite3_close(DB);
				}
			}
			else {
				fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(DB));
				sqlite3_close(DB);
			}
			sqlite3_close(DB);
		}
		else {
			printf("Client attempted logon, but a player with the same name is already logged on!\n");
			return 0;
		}
	}
	else {
		printf("Client attempted logon, but maximum players reached!\n");
		return 0;
	}

	return PlayerID;
}

bool Game_SavePlayerState(game_state *GameState, uint32 PlayerID) {
	player *Player = &GameState->Players[PlayerID];
	sqlite3 *DB;
	int Result = sqlite3_open("GameData.db", &DB);
	if (Result == SQLITE_OK) {
		char PlayerInfoBuffer[DEFAULT_BUFFER_LENGTH];
		_snprintf_s(PlayerInfoBuffer, DEFAULT_BUFFER_LENGTH, "REPLACE INTO Players (Name, Room, HP, Weapon, Shield) VALUES ('%s', %d, %d, %d, %d);", Player->Name.c_str(), Player->CurrentRoom, Player->HP, Player->Weapon, Player->Shield);

		char *err_msg = 0;
		Result = sqlite3_exec(DB, PlayerInfoBuffer, 0, 0, &err_msg);
		if (Result == SQLITE_OK) {
			printf("Player Info saved successfully\n");

			char InventoryInfoBuffer[DEFAULT_BUFFER_LENGTH];
			_snprintf_s(InventoryInfoBuffer, DEFAULT_BUFFER_LENGTH, "REPLACE INTO Inventories (Name, Slot1, Slot2, Slot3, Slot4, Slot5, Slot6, Slot7, Slot8, Slot9, Slot10) VALUES ('%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, %d);", Player->Name.c_str(),
				Player->Inventory[1].ID, Player->Inventory[2].ID, Player->Inventory[3].ID, Player->Inventory[4].ID, Player->Inventory[5].ID,
				Player->Inventory[6].ID, Player->Inventory[7].ID, Player->Inventory[8].ID, Player->Inventory[9].ID, Player->Inventory[10].ID);

			char *err_msg = 0;
			Result = sqlite3_exec(DB, InventoryInfoBuffer, 0, 0, &err_msg);
			if (Result == SQLITE_OK) {
				printf("Inventory saved successfully\n");
			}
			else {
				fprintf(stderr, "SQL error: %s\n", err_msg);
				sqlite3_free(err_msg);
				sqlite3_close(DB);
				return false;
			}
		}
		else {
			fprintf(stderr, "SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);
			sqlite3_close(DB);
			return false;
		}
	}
	else {
		//TODO: Logging!
	}
	return true;
}

void Game_FreePlayer(game_state *GameState, uint32 PlayerID) {
	player *Player = &GameState->Players[PlayerID];
	Player->BaseATK = 0;
	Player->BaseDEF = 0;
	Player->CurrentRoom = 0;
	Player->HP = 0;
	Player->HPMax = 0;
	for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
		Player->Inventory[i].ID = 0;
		Player->Inventory[i].Name = "";
		Player->Inventory[i].Type = 0;
		for (int j = 0; j < ITEM_MAX_PARAMS; j++) {
			Player->Inventory[i].Param[j] = 0;
		}
	}
	Player->Name = "";
	Player->Shield = 0;
	Player->State = INACTIVE;
	Player->Target = 0;
	Player->Weapon = 0;
}

bool Game_LogoffPlayer(game_state *GameState, uint32 PlayerID) {
	bool SaveSuccess = false;
	SaveSuccess = Game_SavePlayerState(GameState, PlayerID);
	Game_FreePlayer(GameState, PlayerID);
	return SaveSuccess;
}

void Game_InitializeRooms(game_state *GameState) {
	// NOTE: Starting at Rooms[1]. Rooms[0] is reserved as an invalid room
	//		 All arrays assume first member is reserved as invalid

	//TODO: Now that we have a SQL database, could these values be stored in a table and read from there?
	//		This would significantly increase scalability
	GameState->Rooms[1].RoomID = 1;
	GameState->Rooms[1].Items[1].Name = "Health Potion";
	GameState->Rooms[1].Items[1].ID = 1;
	GameState->Rooms[1].Items[1].Type = ITEM_CONSUMABLE;
	GameState->Rooms[1].Items[1].Param[0] = 6;
	GameState->Rooms[1].ConnectingRooms[2] = 2;

	GameState->Rooms[2].RoomID = 2;
	GameState->Rooms[2].ConnectingRooms[4] = 1;
	GameState->Rooms[2].ConnectingRooms[1] = 3;
	GameState->Rooms[2].ConnectingRooms[2] = 5;
	GameState->Rooms[2].Items[1].Name = "Sword";
	GameState->Rooms[2].Items[1].ID = 2;
	GameState->Rooms[2].Items[1].Type = ITEM_WEAPON;
	GameState->Rooms[2].Items[1].Param[0] = 3;
	GameState->Rooms[2].Items[1].Param[1] = 1;

	GameState->Rooms[3].RoomID = 3;
	GameState->Rooms[3].ConnectingRooms[1] = 4;
	GameState->Rooms[3].ConnectingRooms[3] = 2;
	GameState->Rooms[3].Enemies[1].Name = "Goblin";
	GameState->Rooms[3].Enemies[1].ATK = 4;
	GameState->Rooms[3].Enemies[1].DEF = 1;
	GameState->Rooms[3].Enemies[1].HP = 10;
	GameState->Rooms[3].Enemies[1].HPMax = 10;
	GameState->Rooms[3].Enemies[1].State = NEUTRAL;
	GameState->Rooms[3].Enemies[1].DropItem.ID = 4;
	GameState->Rooms[3].Enemies[1].DropItem.Name = "Chest Key";
	GameState->Rooms[3].Enemies[1].DropItem.Type = ITEM_KEY;
	GameState->Rooms[3].Enemies[1].DropItem.Param[0] = 2;
	GameState->Rooms[3].Enemies[2].Name = "Goblin";
	GameState->Rooms[3].Enemies[2].ATK = 4;
	GameState->Rooms[3].Enemies[2].DEF = 1;
	GameState->Rooms[3].Enemies[2].HP = 10;
	GameState->Rooms[3].Enemies[2].HPMax = 10;
	GameState->Rooms[3].Enemies[2].State = NEUTRAL;
	GameState->Rooms[3].Enemies[2].DropItem.ID = 4;
	GameState->Rooms[3].Enemies[2].DropItem.Name = "Chest Key";
	GameState->Rooms[3].Enemies[2].DropItem.Type = ITEM_KEY;
	GameState->Rooms[3].Enemies[2].DropItem.Param[0] = 2;
	GameState->Rooms[3].Enemies[3].Name = "Goblin";
	GameState->Rooms[3].Enemies[3].ATK = 4;
	GameState->Rooms[3].Enemies[3].DEF = 1;
	GameState->Rooms[3].Enemies[3].HP = 10;
	GameState->Rooms[3].Enemies[3].HPMax = 10;
	GameState->Rooms[3].Enemies[3].State = NEUTRAL;
	GameState->Rooms[3].Enemies[3].DropItem.ID = 4;
	GameState->Rooms[3].Enemies[3].DropItem.Name = "Chest Key";
	GameState->Rooms[3].Enemies[3].DropItem.Type = ITEM_KEY;
	GameState->Rooms[3].Enemies[3].DropItem.Param[0] = 2;

	GameState->Rooms[4].RoomID = 4;
	GameState->Rooms[4].ConnectingRooms[3] = 3;
	GameState->Rooms[4].Items[1].Name = "Door Key";
	GameState->Rooms[4].Items[1].ID = 3;
	GameState->Rooms[4].Items[1].Type = ITEM_KEY;
	GameState->Rooms[4].Items[1].Param[0] = 1;
	GameState->Rooms[4].Items[1].RequiredKey = 2;
	GameState->Rooms[4].Items[2].Name = "Shield";
	GameState->Rooms[4].Items[2].ID = 5;
	GameState->Rooms[4].Items[2].Type = ITEM_SHIELD;
	GameState->Rooms[4].Items[2].Param[0] = 0;
	GameState->Rooms[4].Items[2].Param[1] = 3;
	GameState->Rooms[4].Items[2].RequiredKey = 2;

	GameState->Rooms[5].RoomID = 5;
	GameState->Rooms[5].ConnectingRooms[4] = 2;
	GameState->Rooms[5].RequiredKey = 1;
	GameState->Rooms[5].Enemies[1].Name = "Big Skeleton";
	GameState->Rooms[5].Enemies[1].ATK = 8;
	GameState->Rooms[5].Enemies[1].DEF = 2;
	GameState->Rooms[5].Enemies[1].HP = 18;
	GameState->Rooms[5].Enemies[1].HPMax = 18;
	GameState->Rooms[5].Enemies[1].State = NEUTRAL;
}

/* NOTE: main() will focus on handling the game tick logic and messages from clients (or more accurately, other threads).
		 To this end, after we get a listening socket from Win32_WinSockInit, a separate thread is
		 kicked off to listen for and handle connections. From there, each client gets its own, new thread,
		 tracked by the ThreadHandler object. main() continues on and starts executing the game loop. */
int main() {
	//NOTE: Initialize sqlite3 and set up the database if it has not already been setup.
	sqlite3 *DB;
	int Result = sqlite3_open("GameData.db", &DB);
	if (Result == SQLITE_OK) {
		char *Command = "CREATE TABLE IF NOT EXISTS Players(ID INT PRIMARY KEY, Name TEXT NOT NULL, Room INT, HP INT, Weapon INT, Shield INT);"
			"CREATE UNIQUE INDEX IF NOT EXISTS idx_Players_Name ON Players (Name);"
			"CREATE TABLE IF NOT EXISTS Inventories(ID INT PRIMARY KEY, Name TEXT NOT NULL, Slot1 INT, Slot2 INT, Slot3 INT, Slot4 INT, Slot5 INT, Slot6 INT, Slot7 INT, Slot8 INT, Slot9 INT, Slot10 INT);"
			"CREATE UNIQUE INDEX IF NOT EXISTS idx_Inventories_Name ON Inventories (Name);";
		char *err_msg = 0;
		Result = sqlite3_exec(DB, Command, 0, 0, &err_msg);
		if (Result == SQLITE_OK) {
			printf("Tables created successfully\n");
		}
		else {
			fprintf(stderr, "SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);
			sqlite3_close(DB);
			string tmp;
			getline(cin, tmp);
			return 1;
		}
	}
	else {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(DB));
		sqlite3_close(DB);
		string tmp;
		getline(cin, tmp);
		return 1;
	}
	sqlite3_close(DB);

	//NOTE: Initialize basic game structures
	game_state GameState = {};
	Game_InitializeRooms(&GameState);
	thread_handler ThreadHandler = {};

	//NOTE: Set up timing information for calling Sleep() later, so we don't waste server CPU cycles
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;
	UINT DesiredSchedulerMS = 1;
	bool SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
	float TargetSecondsPerFrame = 1.0f / SERVER_TICK_RATE;

	//NOTE: Begin WinSock init and start the listener thread
	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = Win32_WinSockInit();
	if (ListenSocket != INVALID_SOCKET) {
		listen_thread_data Data = {};
		Data.ListenSocket = ListenSocket;
		Data.ThreadHandler = &ThreadHandler;
		ThreadHandler.ListenThreadHandle = 
			CreateThread(NULL, 0, Thread_ListenForConnections, (void *)&Data, 0, &ThreadHandler.ListenThreadID);
		// TODO: Add terminating condition
		LARGE_INTEGER LastCounter = Win32_GetWallClock();
		while (true) {
			for (uint32 ThreadID = 0; ThreadID < MAX_CLIENTS; ThreadID++) {
				if (ThreadHandler.ClientThreadHandles[ThreadID]) {
					message *Message = ThreadHandler.MessageQueues[ThreadID].IncomingMessage;
					if (Message) {
						if (Message->Command == MC_HELLO) {
							//printf("Client says hello.\n");
							printf("Sending logon request.\n");
							message OutgoingMessage = {MC_REQLOGON, 0};
							ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
						}
						else if (Message->Command == MC_LOGON) {
							//printf("Received logon name from client.\n");
							string LogonName(Message->Data);
							uint32 PlayerID = 0;
							PlayerID = Game_LogonPlayer(&GameState, LogonName);
							if (PlayerID) {
								ThreadHandler.PlayerIDs[ThreadID] = PlayerID;
								//printf("Sending player info to client.\n");
								message OutgoingMessage = {MC_PLAYERINFO, 0};
								memcpy(OutgoingMessage.Data, &GameState.Players[PlayerID], sizeof(player));
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
							else {
								printf("Player logon failed!\n");
								message OutgoingMessage = {MC_LOGONFAILED, 0};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_STARTPLAYER) {
							//printf("Client requesting to start playing.\n");
							player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
							room CurrentRoom = GameState.Rooms[ActivePlayer->CurrentRoom];
							//printf("Sending current room info to client.\n");
							message OutgoingMessage = {MC_ENTERROOM, 0};
							memcpy(OutgoingMessage.Data, &CurrentRoom, sizeof(room));
							ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
						}
						else if (Message->Command == MC_MOVETO) {
							printf("Client requesting to move to another room.\n");
							int MoveDirection = 0;
							MoveDirection = (int)Message->Data[0];
							if (MoveDirection > 0 && MoveDirection < ROOM_MAX_CONNECTIONS) {
								player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
								if (ActivePlayer->State == NEUTRAL) {
									room *ActiveRoom = &GameState.Rooms[ActivePlayer->CurrentRoom];
									uint32 ConnectedRoom = ActiveRoom->ConnectingRooms[MoveDirection];
									printf("Moving to room %d\n", ConnectedRoom);
									if (ConnectedRoom != 0) {
										room *NewRoom = &GameState.Rooms[ConnectedRoom];
										if (NewRoom->RequiredKey != 0) {
											if (Game_PlayerHasKey(ActivePlayer, NewRoom->RequiredKey)) {
												ActivePlayer->CurrentRoom = NewRoom->RoomID;
												message OutgoingMessage = {MC_ENTERROOM, 0};
												memcpy(OutgoingMessage.Data, NewRoom, sizeof(room));
												ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
											}
											else {
												printf("Player lacks required key!\n");
												message OutgoingMessage = {MC_INVALID, MCERR_MOVE_REQUIRES_KEY};
												ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
											}
										}
										else {
											ActivePlayer->CurrentRoom = NewRoom->RoomID;
											message OutgoingMessage = {MC_ENTERROOM, 0};
											memcpy(OutgoingMessage.Data, NewRoom, sizeof(room));
											ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
										}
									}
									else {
										printf("ConnectedRoom is invalid!\n");
										message OutgoingMessage = {MC_INVALID, MCERR_INVALIDROOM};
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
								}
								else {
									printf("Player is not in NEUTRAL state! Cannot move!");
									message OutgoingMessage = {MC_INVALID, MCERR_NOT_NEUTRAL};
									ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
								}
							}
							else {
								printf("Move direction is invalid!\n");
								message OutgoingMessage = {MC_INVALID, MCERR_INVALIDROOM};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_PICKUP) {
							printf("Client requesting to pick up item...\n");
							int PickupItemID = 0;
							PickupItemID = (int)Message->Data[0];
							//NOTE: Make sure the requested item is within array bounds
							if (PickupItemID > 0 && PickupItemID < ROOM_MAX_ITEMS) {
								player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
								if (ActivePlayer->State == NEUTRAL) {
									room *CurrentRoom = &GameState.Rooms[ActivePlayer->CurrentRoom];
									item *PickupItem = &CurrentRoom->Items[PickupItemID];
									if (PickupItem->ID != 0) {
										bool CanPickUp = false;
										if (PickupItem->RequiredKey != 0) {
											if (Game_PlayerHasKey(ActivePlayer, PickupItem->RequiredKey)) {
												CanPickUp = true;
											}
										}
										else {
											CanPickUp = true;
										}
										if (CanPickUp) {
											bool ItemAlreadyExists = false;
											//NOTE: Go through elements of array and make sure item doesn't already exist
											//		Not allowing duplicate pickups sice we don't have item spawners yet
											for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
												if (ActivePlayer->Inventory[i].ID == PickupItem->ID) {
													printf("Item already exists in player's inventory!\n");
													message OutgoingMessage = {MC_INVALID, MCERR_ITEM_EXISTS};
													ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
													ItemAlreadyExists = true;
													break;
												}
											}
											if (!ItemAlreadyExists) {
												//NOTE: Find the nearest empty slot in player inventory
												bool ItemAdded = false;
												for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
													if (ActivePlayer->Inventory[i].Type == 0) {
														printf("Empty slot found. Adding item...\n");
														//NOTE: Copy the item from the room's Items[] to the player inventory
														//		We don't have spawners yet, so the item stays in the room
														memcpy(&ActivePlayer->Inventory[i], PickupItem, sizeof(item));
														message OutgoingMessage = {MC_UPDATEINV, 0};
														memcpy(OutgoingMessage.Data, ActivePlayer->Inventory, sizeof(ActivePlayer->Inventory));
														ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
														ItemAdded = true;
														break;
													}
												}
												if (!ItemAdded) {
													printf("Player inventory is full!\n");
													message OutgoingMessage = {MC_INVALID, MCERR_INVENTORY_FULL};
													ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
												}
											} //NOTE: ItemAlreadyExists error is handled above
										}
										else {
											printf("Player lacks required key!\n");
											message OutgoingMessage = {MC_INVALID, MCERR_PICKUP_REQUIRES_KEY};
											ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
										}
									}
									else {
										printf("Item requested doesn't exist in room/has ID of 0!\n");
										message OutgoingMessage = {MC_INVALID, MCERR_ITEM_INVALID};
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
								}
								else {
									printf("Player is not in NEUTRAL state! Cannot pickup!");
									message OutgoingMessage = {MC_INVALID, MCERR_NOT_NEUTRAL};
									ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
								}
							}
							else {
								printf("Item requested was out of bounds!\n");
								message OutgoingMessage = {MC_INVALID, MCERR_OUT_OF_BOUNDS};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_USEITEM) {
							printf("Client requesting to use item...\n");
							int UseItemID = 0;
							UseItemID = (int)Message->Data[0];
							if (UseItemID > 0 && UseItemID < PLAYER_MAX_INVENTORY) {
								player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
								if (ActivePlayer->State == NEUTRAL || ActivePlayer->State == FIGHTING) {
									item *UseItem = &ActivePlayer->Inventory[UseItemID];
									if (UseItem->Type == ITEM_CONSUMABLE) {
										//NOTE: Currently the only consumable is a health potion,
										//		So just increase player HP for now.
										ActivePlayer->HP += UseItem->Param[0];
										if (ActivePlayer->HP > ActivePlayer->HPMax) 
											ActivePlayer->HP = ActivePlayer->HPMax;
										UseItem->ID = 0;
										UseItem->Name = "";
										UseItem->Param[0] = 0;
										UseItem->Type = 0;
										printf("Item Consumed. Sending updated player stats to client...\n");
										message OutgoingMessage = {MC_PLAYERUPDATE, 0};
										memcpy(OutgoingMessage.Data, ActivePlayer, sizeof(player));
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
									else {
										printf("Item is not a consumable! Cannot use!\n");
										message OutgoingMessage = {MC_INVALID, MCERR_NOT_CONSUMABLE};
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
								}
								else {
									printf("Player is not in NEUTRAL or FIGHTING state! Cannot use!\n");
									message OutgoingMessage = {MC_INVALID, MCERR_PLAYER_DEAD};
									ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
								}
							}
							else {
								printf("Item requested was out of bounds!\n");
								message OutgoingMessage = {MC_INVALID, MCERR_OUT_OF_BOUNDS};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_EQUIP) {
							printf("Client requesting to equip item...\n");
							int EquipItemID = 0;
							EquipItemID = (int)Message->Data[0];
							if (EquipItemID > 0 && EquipItemID < PLAYER_MAX_INVENTORY) {
								player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
								if (ActivePlayer->State == NEUTRAL || ActivePlayer->State == FIGHTING) {
									item *EquipItem = &ActivePlayer->Inventory[EquipItemID];
									if (EquipItem->Type == ITEM_WEAPON) {
										ActivePlayer->Weapon = EquipItemID;
										printf("Item Equipped. Sending updated player stats to client...\n");
										message OutgoingMessage = {MC_PLAYERUPDATE, 0};
										memcpy(OutgoingMessage.Data, ActivePlayer, sizeof(player));
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
									else if (EquipItem->Type == ITEM_SHIELD) {
										ActivePlayer->Shield = EquipItemID;
										printf("Item Equipped. Sending updated player stats to client...\n");
										message OutgoingMessage = {MC_PLAYERUPDATE, 0};
										memcpy(OutgoingMessage.Data, ActivePlayer, sizeof(player));
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
									else {
										printf("Item is not a weapon! Cannot equip!\n");
										message OutgoingMessage = {MC_INVALID, MCERR_NOT_WEAPON};
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
								}
								else {
									printf("Player is not in NEUTRAL or FIGHTING state! Cannot equip!\n");
									message OutgoingMessage = {MC_INVALID, MCERR_PLAYER_DEAD};
									ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
								}
							}
							else {
								printf("Requested item was out of bounds!\n");
								message OutgoingMessage = {MC_INVALID, MCERR_OUT_OF_BOUNDS};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_FIGHT) {
							printf("Client requesting to fight an enemy...\n");
							int EnemyID = 0;
							EnemyID = Message->Data[0];
							if (EnemyID > 0 && EnemyID < ROOM_MAX_ENEMIES) {
								player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
								if (ActivePlayer->State == NEUTRAL) {
									room *CurrentRoom = &GameState.Rooms[ActivePlayer->CurrentRoom];
									enemy *Enemy = &CurrentRoom->Enemies[EnemyID];
									if (Enemy->State == NEUTRAL) {
										printf("Player and enemy are in valid state. Initiating combat...");
										ActivePlayer->State = FIGHTING;
										Enemy->State = FIGHTING;
										ActivePlayer->Target = Enemy;
										Enemy->Target = ActivePlayer;
										message OutgoingMessage = {MC_ENGAGE, 0};
										memcpy(OutgoingMessage.Data, Enemy, sizeof(enemy));
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
									else {
										char Error = MCERR_TARGET_INVALID;
										if (Enemy->State == FIGHTING) {
											printf("Target is already fighting!\n");
											Error = MCERR_TARGET_FIGHTING;
										}
										if (Enemy->State == DEAD) {
											printf("Target is already dead!\n");
											Error = MCERR_TARGET_DEAD;
										}
										printf("Target is invalid!\n");
										message OutgoingMessage = {MC_INVALID, Error};
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
								}
								else {
									printf("Player is not in NEUTRAL state! Cannot fight!");
									message OutgoingMessage = {MC_INVALID, MCERR_NOT_NEUTRAL};
									ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
								}
							}
							else {
								printf("Requested enemy was out of bounds!\n");
								message OutgoingMessage = {MC_INVALID, MCERR_OUT_OF_BOUNDS};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_ATTACK) {
							printf("Client requesting to attack..\n");
							player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
							if (ActivePlayer->State == FIGHTING) {
								enemy *Enemy = 0;
								Enemy = ActivePlayer->Target;
								if (Enemy) {
									if (Enemy->State == FIGHTING) {
										//NOTE: Figure out player's ATK and DEF values
										int PlayerATK = ActivePlayer->BaseATK;

										if (ActivePlayer->Weapon) {
											PlayerATK += ActivePlayer->Inventory[ActivePlayer->Weapon].Param[0];
										}
										if (ActivePlayer->Shield) {
											PlayerATK += ActivePlayer->Inventory[ActivePlayer->Shield].Param[0];
										}

										int PlayerDEF = ActivePlayer->BaseDEF;

										if (ActivePlayer->Weapon) {
											PlayerDEF += ActivePlayer->Inventory[ActivePlayer->Weapon].Param[1];
										}
										if (ActivePlayer->Shield) {
											PlayerDEF += ActivePlayer->Inventory[ActivePlayer->Shield].Param[1];
										}

										int Damage = PlayerATK - Enemy->DEF;
										//NOTE: If DEF value leaves damage at 0 or less, we don't want to heal.
										//		Instead deal at least 1 damage
										if (Damage < 1) Damage = 1;
										Enemy->HP -= Damage;
										if (Enemy->HP <= 0) {
											//TODO: Kill the enemy when we have a proper spawning mecahnic.
											//		For now, enemy will just resurrect with full HP.
											Enemy->State = NEUTRAL;
											Enemy->HP = Enemy->HPMax;
											Enemy->Target = nullptr;
											ActivePlayer->State = NEUTRAL;
											ActivePlayer->Target = nullptr;

											//NOTE: If the enemy drops an item, add it here to the player's inventory.
											if (Enemy->DropItem.ID != 0) {
												item *DropItem = &Enemy->DropItem;
												bool ItemAlreadyExists = false;
												//NOTE: Go through elements of array and make sure item doesn't already exist
												//		Not allowing duplicate pickups sice we don't have item spawners yet
												for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
													if (ActivePlayer->Inventory[i].ID == DropItem->ID) {
														printf("Item already exists in player's inventory!\n");
														ItemAlreadyExists = true;
														break;
													}
												}
												if (!ItemAlreadyExists) {
													//NOTE: Find the nearest empty slot in player inventory
													bool ItemAdded = false;
													for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
														if (ActivePlayer->Inventory[i].Type == 0) {
															printf("Empty slot found. Adding item...\n");
															//NOTE: Copy the item from the room's Items[] to the player inventory
															//		We don't have spawners yet, so the item stays in the room
															memcpy(&ActivePlayer->Inventory[i], DropItem, sizeof(item));
															ItemAdded = true;
															break;
														}
													}
													if (!ItemAdded) {
														printf("Player inventory is full!\n");
													}
												}
											}
											combat_update CmbUpdate = {};
											CmbUpdate.Outcome = ENEMY_DEAD;
											CmbUpdate.PlayerDamageDealt = Damage;
											CmbUpdate.PlayerDamageTaken = 0;
											CmbUpdate.PlayerInfo = *ActivePlayer;
											CmbUpdate.EnemyInfo = *Enemy;

											message OutgoingMessage = {MC_DISENGAGE, 0};
											memcpy(OutgoingMessage.Data, &CmbUpdate, sizeof(combat_update));
											ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
										}
										//NOTE: Enemy is still alive. Process enemy turn.
										else {
											//TODO: For now, enemy will just attack every turn.
											int EnemyATK = Enemy->ATK;
											int EnemyDamage = EnemyATK - PlayerDEF;
											if (EnemyDamage < 1) EnemyDamage = 1;
											ActivePlayer->HP -= EnemyDamage;
											if (ActivePlayer->HP <= 0) {
												Enemy->State = NEUTRAL;
												Enemy->Target = nullptr;
												//NOTE: Player is dead!
												ActivePlayer->State = NEUTRAL;
												ActivePlayer->HP = ActivePlayer->HPMax;
												ActivePlayer->CurrentRoom = 1;
												ActivePlayer->Target = nullptr;
												ActivePlayer->Weapon = 0;
												//NOTE: Player drops all items on death!
												for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
													ActivePlayer->Inventory[i].ID = 0;
													ActivePlayer->Inventory[i].Name = "";
													ActivePlayer->Inventory[i].Type = 0;
													for (int j = 0; j < ITEM_MAX_PARAMS; j++) {
														ActivePlayer->Inventory[i].Param[j] = 0;
													}
												}
												combat_update CmbUpdate = {};
												CmbUpdate.Outcome = PLAYER_DEAD;
												CmbUpdate.PlayerDamageDealt = Damage;
												CmbUpdate.PlayerDamageTaken = EnemyDamage;
												CmbUpdate.PlayerInfo = *ActivePlayer;
												CmbUpdate.EnemyInfo = *Enemy;

												message OutgoingMessage = {MC_DISENGAGE, 0};
												memcpy(OutgoingMessage.Data, &CmbUpdate, sizeof(combat_update));
												ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
											}
											else {
												//NOTE: Both combatants still alive. Send normal combat update.
												combat_update CmbUpdate = {};
												CmbUpdate.Outcome = COMBATANTS_ALIVE;
												CmbUpdate.PlayerDamageDealt = Damage;
												CmbUpdate.PlayerDamageTaken = EnemyDamage;
												CmbUpdate.PlayerInfo = *ActivePlayer;
												CmbUpdate.EnemyInfo = *Enemy;

												message OutgoingMessage = {MC_CMBUPDATE, 0};
												memcpy(OutgoingMessage.Data, &CmbUpdate, sizeof(combat_update));
												ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
											}
										}
									}
									else {
										//NOTE: Error should not happen during normal gameplay
										printf("Player's target is not in fighting state!\n");
										message OutgoingMessage = {MC_INVALID, MCERR_TARGET_INVALID};
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
								}
								else {
									//NOTE: Error should not happen during normal gameplay
									printf("Player's target is invalid!\n");
									message OutgoingMessage = {MC_INVALID, MCERR_TARGET_INVALID};
									ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
								}
							}
							else {
								printf("Player is not in FIGHTING state! Cannot attack!\n");
								message OutgoingMessage = {MC_INVALID, MCERR_NOT_FIGHTING};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_FLEE) {
							printf("Client requesting to escape combat..\n");
							player *ActivePlayer = &GameState.Players[ThreadHandler.PlayerIDs[ThreadID]];
							if (ActivePlayer->State == FIGHTING) {
								enemy *Enemy = 0;
								Enemy = ActivePlayer->Target;
								if (Enemy) {
									if (Enemy->State == FIGHTING) {
										//TODO: For now, enemy will just attack every turn.
										int PlayerDEF = ActivePlayer->BaseDEF;
										if (ActivePlayer->Weapon) {
											PlayerDEF += ActivePlayer->Inventory[ActivePlayer->Weapon].Param[1];
										}
										if (ActivePlayer->Shield) {
											PlayerDEF += ActivePlayer->Inventory[ActivePlayer->Shield].Param[1];
										}
										int EnemyATK = Enemy->ATK;
										int EnemyDamage = EnemyATK - PlayerDEF;
										if (EnemyDamage < 1) EnemyDamage = 1;
										ActivePlayer->HP -= EnemyDamage;
										if (ActivePlayer->HP <= 0) {
											Enemy->State = NEUTRAL;
											Enemy->Target = nullptr;
											//NOTE: Player is dead!
											ActivePlayer->State = NEUTRAL;
											ActivePlayer->HP = ActivePlayer->HPMax;
											ActivePlayer->CurrentRoom = 1;
											ActivePlayer->Target = nullptr;
											ActivePlayer->Weapon = 0;
											//NOTE: Player drops all items on death!
											for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
												ActivePlayer->Inventory[i].ID = 0;
												ActivePlayer->Inventory[i].Name = "";
												ActivePlayer->Inventory[i].Type = 0;
												for (int j = 0; j < ITEM_MAX_PARAMS; j++) {
													ActivePlayer->Inventory[i].Param[j] = 0;
												}
											}
											combat_update CmbUpdate = {};
											CmbUpdate.Outcome = PLAYER_FLEE_DEAD;
											CmbUpdate.PlayerDamageDealt = 0;
											CmbUpdate.PlayerDamageTaken = EnemyDamage;
											CmbUpdate.PlayerInfo = *ActivePlayer;
											CmbUpdate.EnemyInfo = *Enemy;

											message OutgoingMessage = {MC_DISENGAGE, 0};
											memcpy(OutgoingMessage.Data, &CmbUpdate, sizeof(combat_update));
											ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
										}
										else {
											Enemy->State = NEUTRAL;
											Enemy->Target = nullptr;
											ActivePlayer->State = NEUTRAL;
											ActivePlayer->Target = nullptr;
											//NOTE: Both combatants still alive. Send normal combat update.
											combat_update CmbUpdate = {};
											CmbUpdate.Outcome = PLAYER_FLED;
											CmbUpdate.PlayerDamageDealt = 0;
											CmbUpdate.PlayerDamageTaken = EnemyDamage;
											CmbUpdate.PlayerInfo = *ActivePlayer;
											CmbUpdate.EnemyInfo = *Enemy;
											message OutgoingMessage = {MC_DISENGAGE, 0};
											memcpy(OutgoingMessage.Data, &CmbUpdate, sizeof(combat_update));
											ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
										}
									}
									else {
										printf("Target is not in FIGHTING state!");
										message OutgoingMessage = {MC_INVALID, MCERR_TARGET_INVALID};
										ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
									}
								}
								else {
									printf("Player's target is not valid!\n");
									message OutgoingMessage = {MC_INVALID, MCERR_TARGET_INVALID};
									ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
								}
							}
							else {
								printf("Player is not in FIGHTING state! Cannot flee!\n");
								message OutgoingMessage = {MC_INVALID, MCERR_NOT_FIGHTING};
								ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
							}
						}
						else if (Message->Command == MC_DISCONNECT) {
							printf("Client disconnected. Resetting state and disengaging any enemies...\n");
							uint32 PlayerID = ThreadHandler.PlayerIDs[ThreadID];
							player *ActivePlayer = &GameState.Players[PlayerID];
							if (ActivePlayer->State != INACTIVE) {
								ActivePlayer->State = INACTIVE;
								if (ActivePlayer->Target) {
									ActivePlayer->Target->State = NEUTRAL;
									ActivePlayer->Target->Target = nullptr;
								}
								ActivePlayer->Target = nullptr;
								Game_LogoffPlayer(&GameState, PlayerID);
							}
							ThreadHandler.ClientThreadIDs[ThreadID] = 0;
							ThreadHandler.ClientThreadHandles[ThreadID] = 0;
							ThreadHandler.MessageQueues[ThreadID].IncomingMessage = 0;
							ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = 0;
							ThreadHandler.PlayerIDs[ThreadID] = 0;
						}
						else {
							printf("Message not recognized!\n");
							message OutgoingMessage = {MC_INVALID, 0};
							ThreadHandler.MessageQueues[ThreadID].OutgoingMessage = &OutgoingMessage;
						}
						ThreadHandler.MessageQueues[ThreadID].IncomingMessage = 0;
					}
				}
			}

			LARGE_INTEGER WorkCounter = Win32_GetWallClock();
			float WorkSecondsElapsed = Win32_GetSecondsElapsed(LastCounter, WorkCounter);

			//TODO: Not thoroughly tested yet! Possible bugs.
			float SecondsElapsedForFrame = WorkSecondsElapsed;
			if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
				if (SleepIsGranular) {
					DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame -
						SecondsElapsedForFrame));
					if (SleepMS > 0) {
						Sleep(SleepMS);
					}
				}
				float TestSecondsElapsedForFrame = Win32_GetSecondsElapsed(LastCounter, Win32_GetWallClock());
				if (TestSecondsElapsedForFrame < TargetSecondsPerFrame) {
					//TODO: Log missed sleep here
				}
				//NOTE: This is the "spin the CPU" option and is NOT desired! 
				//		Should only happen if we miss our sleep or can't get 
				//		the desired timing resolution from timeBeginPeriod()
				while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
					SecondsElapsedForFrame = Win32_GetSecondsElapsed(LastCounter,
						Win32_GetWallClock());
				}
			}
			else {
				//NOTE: Missed framerate!
				//TODO: Logging
			}

			LARGE_INTEGER EndCounter = Win32_GetWallClock();
			float MSPerFrame = 1000.0f*Win32_GetSecondsElapsed(LastCounter, EndCounter);
			LastCounter = EndCounter;
		}
	}
	else {
		printf("WinSock Init Failed!");
		timeEndPeriod(DesiredSchedulerMS);
		WSACleanup();
		PressEnterToContinue();
		return 1;
	}
	timeEndPeriod(DesiredSchedulerMS);
	closesocket(ListenSocket);
	WSACleanup();
	PressEnterToContinue();
	return 0;
}