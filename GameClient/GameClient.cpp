/* ========================================================================
$File: GameClient.cpp
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

#include "GameClient.h"

#pragma comment(lib, "Ws2_32.lib")

//NOTE: called at termination points to hold the console window open
//TODO: make a proper "press any key to exit" function
void PressEnterToContinue() {
	printf("Press Enter to continue\n");
	string temp;
	getline(cin, temp);
}

SOCKET Win32_WinSockInit() {
	printf("Initializing Client WinSock...\n");
	WSADATA WinSockData;
	int WinSockLoadResult = WSAStartup(MAKEWORD(2, 2), &WinSockData);
	if (WinSockLoadResult != 0) {
		printf("WinSock load failed!: error code %d\n", WinSockLoadResult);
		return INVALID_SOCKET;
	}
	else {
		printf("WinSock loaded successfully!\n");
	}

	addrinfo *ClientAddrInfo = 0;
	//addrinfo *ptr = 0; 
	addrinfo hints = {};

	// TODO: Investigate if call to ZeroMemory is really required.
	// API Docs include it, but {} clears the struct to zero implicitly.
	ZeroMemory(&hints, sizeof(hints));

	hints.ai_family = AF_INET; // TODO: forcing IPv4 for now. This should be changed to AF_UNSPEC later to allow v6!
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// TODO: This will block/fail if input is malformed. Replace with more robust input method.
	string ServerIPAddr;
	printf("Please enter server IP address to connect to:\n");
	getline(cin, ServerIPAddr);

	PCSTR ServerIPAddrC = ServerIPAddr.c_str();
	int GetAddrInfoResult = getaddrinfo(ServerIPAddrC, DEFAULT_PORT, &hints, &ClientAddrInfo);

	if (GetAddrInfoResult != 0) {
		printf("Failed to retrieve address for socket!: %d\n", GetAddrInfoResult);
		return INVALID_SOCKET;
	}
	else {
		printf("WinSock retrieved a socket address successfully!\n");
	}

	SOCKET ConnectSocket = INVALID_SOCKET;
	ConnectSocket = socket(ClientAddrInfo->ai_family, ClientAddrInfo->ai_socktype, ClientAddrInfo->ai_protocol);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Error getting socket: %ld\n", WSAGetLastError());
		return INVALID_SOCKET;
	}
	else {
		printf("Retrieved Socket successfully.\n");
	}

	// TODO: Should try the next address from ClientAddrInfo if initial connect fails
	int ConnectResult = connect(ConnectSocket, ClientAddrInfo->ai_addr, (int)ClientAddrInfo->ai_addrlen);
	if (ConnectResult == SOCKET_ERROR) {
		printf("Unable to connect to server!\n");
		return INVALID_SOCKET;
	}
	else {
		printf("Connected to server successfully!\n");
	}
	// NOTE: This is not needed anymore and can be discarded
	freeaddrinfo(ClientAddrInfo);
	return ConnectSocket;
}

void ListPlayerStats(player *Player) {
	item *Weapon = &Player->Inventory[Player->Weapon];
	string WeaponName = "None";
	item *Shield = &Player->Inventory[Player->Shield];
	string ShieldName = "None";
	int ATK = Player->BaseATK;
	int DEF = Player->BaseDEF;
	if (Weapon->ID) {
		WeaponName = Weapon->Name;
		ATK += Weapon->Param[0];
		DEF += Weapon->Param[1];
	}
	if (Shield->ID) {
		ShieldName = Shield->Name;
		ATK += Shield->Param[0];
		DEF += Shield->Param[1];
	}
	printf("%s's Stats: | HP: %d/%d | Weapon: %s | Shield: %s | ATK: %d | DEF: %d\n\n", Player->Name.c_str(), Player->HP, Player->HPMax, WeaponName.c_str(), ShieldName.c_str(), ATK, DEF);
}

void ListPlayerItems(player *Player) {
	printf("Inventory: | ");
	for (int i = 0; i < CountElements(Player->Inventory); i++) {
		if (Player->Inventory[i].Type != 0) {
			printf("Slot %d: %s | ", i, Player->Inventory[i].Name.c_str());
		}
	}
	printf("\n\n");
}

void ListItems(room *Room) {
	for (int i = 0; i < CountElements(Room->Items); i++) {
		if (Room->Items[i].Type != 0) {
			printf("There is a %s in the room! Say \"pickup %d\" to pick up.\n", Room->Items[i].Name.c_str(), i);
		}
	}
}

void ListEnemies(room *Room) {
	for (int i = 0; i < CountElements(Room->Enemies); i++) {
		if (Room->Enemies[i].HP != 0) {
			printf("There is a %s in the room! Say \"fight %d\" to fight.\n", Room->Enemies[i].Name.c_str(), i);
		}
	}
}

void EnterRoom(client_state *ClientState) {
	room *Room = &ClientState->CurrentRoom;
	ClientState->Player.CurrentRoom = Room->RoomID;
	uint32 RoomNumber = Room->RoomID;
	system("cls");
	//ListPlayerItems(&ClientState->Player);
	switch (RoomNumber) {
		case 0:
		printf("Invalid room!\n");
		break;
		case 1:
		printf("You stand in a small, dimly lit room, resembling a prison cell.\nThere is a door to the east (type \"move east\" to enter).\n");
		break;
		case 2:
		printf("You enter a chamber with three doors, one to the north, one to the east, and one to the west.\nTo the south, a decorative sword hangs on the wall.\n");
		break;
		case 3:
		printf("You enter a large, empty room, reminiscent of a ballroom, with ornate floors and tapestries lining the walls.\nAcross the room to the north is another door, in addition to the one behind you to the south.\n");
		break;
		case 4:
		printf("You enter a small, ornate chamber with a chest in the middle.\nThe only door in the room is behind you, to the south.\n");
		break;
		case 5:
		printf("You have unlocked the door to the exit! A big skeleton stands in your way...\n");
		break;
		default:
		printf("Invalid room!\n");
		break;
	}
	ListItems(Room);
	ListEnemies(Room);
	printf("\n");
}

void StartBattle(client_state *ClientState) {
	enemy *Enemy = &ClientState->CurrentEnemy;
	player *Player = &ClientState->Player;
	Player->Target = Enemy;
	Player->State = FIGHTING;
	system("cls");
	printf("Engaged in combat with %s!\n %s's HP: %d\nSay \"attack\" to attack, or \"escape\" to flee!\nCan also use and equip items.\n\n", Enemy->Name.c_str(), Enemy->Name.c_str(), Enemy->HP);
	ListPlayerStats(&ClientState->Player);
}

void DisplayCombatUpdate (client_state *ClientState) {
	combat_update CombatData = ClientState->CombatData;
	enemy *Enemy = &ClientState->CurrentEnemy;
	if (CombatData.Outcome == COMBATANTS_ALIVE) {
		printf("You attacked %s, dealing %d damage!\n%s attacks you, dealing %d damage!\n", Enemy->Name.c_str(), CombatData.PlayerDamageDealt, Enemy->Name.c_str(), CombatData.PlayerDamageTaken);
		printf("%s has %d/%d HP remaining\n", Enemy->Name.c_str(), Enemy->HP, Enemy->HPMax);
		ListPlayerStats(&ClientState->Player);
	}
	if (CombatData.Outcome == PLAYER_DEAD) {
		printf("You attacked %s, dealing %d damage!\n%s attacks you, dealing %d damage!\n", Enemy->Name.c_str(), CombatData.PlayerDamageDealt, Enemy->Name.c_str(), CombatData.PlayerDamageTaken);
		printf("Oh dear, you are dead!\nPress enter to respawn.\n");
	}
	if (CombatData.Outcome == ENEMY_DEAD) {
		printf("You attacked %s, dealing %d damage!\n%s attacks you, dealing %d damage!\n", Enemy->Name.c_str(), CombatData.PlayerDamageDealt, Enemy->Name.c_str(), CombatData.PlayerDamageTaken);
		printf("%s has been defeated!\n", Enemy->Name.c_str());
		ListPlayerStats(&ClientState->Player);
		printf("Press enter to return to room.\n");
	}
	if (CombatData.Outcome == PLAYER_FLED) {
		printf("You escape from combat!\nAs you're fleeing, %s attacks you, dealing %d damage!\n", Enemy->Name.c_str(), CombatData.PlayerDamageTaken);
		ListPlayerStats(&ClientState->Player);
		printf("Press enter to continue.\n");
	}
	if (CombatData.Outcome == PLAYER_FLEE_DEAD) {
		printf("You attempt to escape from combat!\nAs you're fleeing, %s attacks you, dealing %d damage and killing you!\n", Enemy->Name.c_str(), CombatData.PlayerDamageTaken);
		printf("Oh dear, you are dead!\nPress enter to respawn.\n");
	}
}

bool CheckChoice(string Choice) {
	//printf("Checking Choice... ");
	bool Valid = false;
	size_t SpacePos = 0;
	SpacePos = Choice.find_first_of(" ", 0);
	if (SpacePos != string::npos) {
		string Command = Choice.substr(0, SpacePos);
		string Parameter = Choice.substr(SpacePos + 1, string::npos);
		if (Command.compare("move") == 0) {
			if (Parameter.compare("north") == 0 || Parameter.compare("east") == 0 || Parameter.compare("south") == 0 || Parameter.compare("west") == 0) {
				Valid = true;
			}
		}
		else if (Command.compare("pickup") == 0) {
			int ParamInt = stoi(Parameter, nullptr, 10);
			if (ParamInt < ROOM_MAX_ITEMS && ParamInt > 0) {
				Valid = true;
			}
		}
		else if (Command.compare("fight") == 0) {
			int ParamInt = stoi(Parameter, nullptr, 10);
			if (ParamInt < ROOM_MAX_ENEMIES && ParamInt > 0) {
				Valid = true;
			}
		}
		else if (Command.compare("use") == 0) {
			int ParamInt = stoi(Parameter, nullptr, 10);
			if (ParamInt < PLAYER_MAX_INVENTORY && ParamInt > 0) {
				Valid = true;
			}
		}
		else if (Command.compare("equip") == 0) {
			int ParamInt = stoi(Parameter, nullptr, 10);
			if (ParamInt < PLAYER_MAX_INVENTORY && ParamInt > 0) {
				Valid = true;
			}
		}
	}
	else {
		if (Choice.compare("attack") == 0) {
			Valid = true;
		}
		if (Choice.compare("escape") == 0) {
			Valid = true;
		}
		if (Choice.compare("inventory") == 0) {
			Valid = true;
		}
		if (Choice.compare("stats") == 0) {
			Valid = true;
		}
		if (Choice.compare("help") == 0) {
			Valid = true;
		}
		if (Choice.compare("exit") == 0) {
			Valid = true;
		}
	}
	return Valid;
}

void ChooseAction(client_state *ClientState, SOCKET ConnectSocket) {
	bool ChoiceValid = false;
	while (ChoiceValid == false) {
		printf("Choose your action (\"help\" for a list of commands)... ");
		string Choice;
		getline(cin, Choice);
		if (CheckChoice(Choice)) {
			//printf("Choice Valid!\n");
			ChoiceValid = true;
			char MsgCmd = MC_INVALID;
			char Param = 0;
			size_t SpacePos = Choice.find_first_of(" ", 0);
			if (SpacePos != string::npos) {
				string Command = Choice.substr(0, SpacePos);
				string Parameter = Choice.substr(SpacePos + 1, string::npos);
				if (Command.compare("move") == 0) {
					//printf("command is MOVE\n");
					MsgCmd = MC_MOVETO;
					if (Parameter.compare("north") == 0) {
						//printf("parameter is NORTH\n");
						Param = 1;
					}
					else if (Parameter.compare("east") == 0) {
						//printf("parameter is EAST\n");
						Param = 2;
					}
					else if (Parameter.compare("south") == 0) {
						//printf("parameter is SOUTH\n");
						Param = 3;
					}
					else if (Parameter.compare("west") == 0) {
						//printf("parameter is WEST\n");
						Param = 4;
					}
				}
				else if (Command.compare("pickup") == 0) {
					//printf("command is PICKUP\n");
					MsgCmd = MC_PICKUP;
					Param = stoi(Parameter, nullptr, 10);
					//printf("parameter is %d\n", Param);
				}
				else if (Command.compare("fight") == 0) {
					//printf("command is FIGHT\n");
					MsgCmd = MC_FIGHT;
					Param = stoi(Parameter, nullptr, 10);
					//printf("parameter is %d\n", Param);
				}
				else if (Command.compare("use") == 0) {
					MsgCmd = MC_USEITEM;
					Param = stoi(Parameter, nullptr, 10);
				}
				else if (Command.compare("equip") == 0) {
					MsgCmd = MC_EQUIP;
					Param = stoi(Parameter, nullptr, 10);
				}
			}
			else if (Choice.compare("attack") == 0) {
				MsgCmd = MC_ATTACK;
			}
			else if (Choice.compare("escape") == 0) {
				MsgCmd = MC_FLEE;
			}
			else if (Choice.compare("exit") == 0) {
				MsgCmd = MC_LOGOFF;
			}
			else if (Choice.compare("inventory") == 0) {
				ListPlayerItems(&ClientState->Player);
				ChooseAction(ClientState, ConnectSocket);
				return;
			}
			else if (Choice.compare("stats") == 0) {
				ListPlayerStats(&ClientState->Player);
				ChooseAction(ClientState, ConnectSocket);
				return;
			}
			else if (Choice.compare("help") == 0) {
				printf("Valid commands are:\n\n");
				printf("move [north, east, south, west]: Move to a different room in the specified direction.\n");
				printf("pickup [item number]:            Pick up the specified item in the room. Cannot be used in combat.\n");
				printf("fight [enemy number]:            Attempt to initiate combat with the specified enemy.\n");
				printf("use [inventory slot number]:     Use the item in the specified slot. Must be a consumable.\n");
				printf("equip [inventory slot number]:   Equip the item in the specified slot. Must be a weapon.\n");
				printf("attack:                          Can only be used in combat. Attack the enemy you're engaged with.\n");
				printf("escape:                          Can only be used in combat. Attempt to flee combat.\n");
				printf("inventory:                       Display your player's inventory. Can be used any time.\n");
				printf("stats:                           Display your player's stats, such as HP and equipment.\n");
				printf("exit:                            Logs off and exits the client. Your progress will be saved.\n\n");
				ChooseAction(ClientState, ConnectSocket);
				return;
			}
			message OutMsg = {MsgCmd, Param};
			//memcpy(OutMsg.Data, &Param, sizeof(int));
			char *OutBuffer = (char *)&OutMsg;
			int BytesSent = send(ConnectSocket, OutBuffer, DEFAULT_BUFFER_LENGTH, 0);
		}
		else {
			printf("Invalid Selection! Try again.\n");
		}
	}
}

int main() {
	SOCKET ConnectSocket = INVALID_SOCKET;
	ConnectSocket = Win32_WinSockInit();
	client_state ClientState = {};
	bool Running = true;
	if (ConnectSocket != INVALID_SOCKET) {
		//cin.ignore();
		int BytesReceived = 0;
		// NOTE: Send the server a hello upon connecting
		message OutgoingMessage = {MC_HELLO, 0};
		char *SendBuffer = (char *)&OutgoingMessage;
		int BytesSent = send(ConnectSocket, SendBuffer, DEFAULT_BUFFER_LENGTH, 0);
		if (BytesSent == SOCKET_ERROR) {
			printf("Send failed: %d\n", WSAGetLastError());
			closesocket(ConnectSocket);
			WSACleanup();
			return 1;
		}
		else {
			printf("Hello Sent\n");
		}
		while (Running) {
			char ReceiveBuffer[DEFAULT_BUFFER_LENGTH] = {};
			// NOTE: Receiving server's response
			BytesReceived = recv(ConnectSocket, ReceiveBuffer, DEFAULT_BUFFER_LENGTH, 0);
			if (BytesReceived > 0) {
				//printf("Bytes received: %d\n", BytesReceived);
				message *ReceivedMessage = (message *)ReceiveBuffer;
				// NOTE: MC_REQLOGON should be the first response the client receives upon sending MC_HELLO above.
				if (ReceivedMessage->Command == MC_REQLOGON) {
					printf("Server is requesting logon. Enter your logon name: ");
					string LogonName;
					getline(cin, LogonName);
					message LogonMessage = {MC_LOGON, 0};
					const char *LogonNameC = LogonName.c_str();
					memcpy(LogonMessage.Data, LogonNameC, strlen(LogonNameC));
					char *LogonBuffer = (char *)&LogonMessage;
					BytesSent = send(ConnectSocket, LogonBuffer, DEFAULT_BUFFER_LENGTH, 0);
				}
				// NOTE: Server responds with this after client sends MC_LOGON with logon name
				else if (ReceivedMessage->Command == MC_PLAYERINFO) {
					printf("Receiving player info.\n");
					player *PlayerInfo = (player *)ReceivedMessage->Data;
					ClientState.Player = *PlayerInfo;
					printf("Player Info Received. Player's name is %s, standing in room %d.\n", ClientState.Player.Name.c_str(), ClientState.Player.CurrentRoom);
					message OutMsg = {MC_STARTPLAYER, 0};
					char *OutBuffer = (char *)&OutMsg;
					BytesSent = send(ConnectSocket, OutBuffer, DEFAULT_BUFFER_LENGTH, 0);
				}
				else if (ReceivedMessage->Command == MC_LOGONFAILED) {
					printf("Logon failed! Player already logged in.\n");
					Running = false;
				}
				else if (ReceivedMessage->Command == MC_ENTERROOM) {
					printf("Entering room...\n\n");
					//ClientState.CurrentRoom = (room *)ReceivedMessage->Data;
					memcpy(&ClientState.CurrentRoom, ReceivedMessage->Data, sizeof(room));
					EnterRoom(&ClientState);
					ChooseAction(&ClientState, ConnectSocket);
				}
				else if (ReceivedMessage->Command == MC_UPDATEINV) {
					printf("Picked up item!\n");
					memcpy(ClientState.Player.Inventory, ReceivedMessage->Data, sizeof(ClientState.Player.Inventory));
					ListPlayerItems(&ClientState.Player);
					ChooseAction(&ClientState, ConnectSocket);
				}
				else if (ReceivedMessage->Command == MC_PLAYERUPDATE) {
					printf("Received updated player info!\n");
					memcpy(&ClientState.Player, ReceivedMessage->Data, sizeof(player));
					ListPlayerStats(&ClientState.Player);
					ChooseAction(&ClientState, ConnectSocket);
				}
				else if (ReceivedMessage->Command == MC_ENGAGE) {
					printf("Engaging in combat...\n");
					memcpy(&ClientState.CurrentEnemy, ReceivedMessage->Data, sizeof(enemy));
					StartBattle(&ClientState);
					ChooseAction(&ClientState, ConnectSocket);
				}
				else if (ReceivedMessage->Command == MC_CMBUPDATE) {
					printf("Received combat update!\n");
					memcpy(&ClientState.CombatData, ReceivedMessage->Data, sizeof(combat_update));
					memcpy(&ClientState.Player, &ClientState.CombatData.PlayerInfo, sizeof(player));
					memcpy(&ClientState.CurrentEnemy, &ClientState.CombatData.EnemyInfo, sizeof(enemy));
					DisplayCombatUpdate(&ClientState);
					ChooseAction(&ClientState, ConnectSocket);
				}
				else if (ReceivedMessage->Command == MC_DISENGAGE) {
					printf("Received combat update!\n");
					memcpy(&ClientState.CombatData, ReceivedMessage->Data, sizeof(combat_update));
					memcpy(&ClientState.Player, &ClientState.CombatData.PlayerInfo, sizeof(player));
					memcpy(&ClientState.CurrentEnemy, &ClientState.CombatData.EnemyInfo, sizeof(enemy));
					DisplayCombatUpdate(&ClientState);
					if (ClientState.CombatData.Outcome == ENEMY_DEAD && ClientState.CombatData.EnemyInfo.Name.compare("Big Skeleton") == 0) {
						printf("You have defeated the big skeleton and made your escape!\n");
						string temp;
						getline(cin, temp);
						message OutMsg = {MC_LOGOFF, 0};
						char *OutBuffer = (char *)&OutMsg;
						BytesSent = send(ConnectSocket, OutBuffer, DEFAULT_BUFFER_LENGTH, 0);
						Running = false;
					}
					else {
						string temp;
						getline(cin, temp);
						message OutMsg = {MC_STARTPLAYER, 0};
						char *OutBuffer = (char *)&OutMsg;
						BytesSent = send(ConnectSocket, OutBuffer, DEFAULT_BUFFER_LENGTH, 0);
					}
					//ChooseAction(&ClientState, ConnectSocket);
				}
				else if (ReceivedMessage->Command == MC_INVALID) {
					if (ReceivedMessage->Data[0] == MCERR_INVALIDROOM) {
						printf("Cannot move in that direction! No door exists.\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_MOVE_REQUIRES_KEY) {
						printf("Cannot open door! Requires key.\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_NOT_NEUTRAL) {
						printf("You must not be in combat (or dead) to perform this action!\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_ITEM_EXISTS) {
						printf("Item already exists in inventory! Can't pick up duplicates.\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_PICKUP_REQUIRES_KEY) {
						printf("Selected item is locked in a chest! Requires key.\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_INVENTORY_FULL) {
						printf("Inventory full! Cannot pick up any more items.\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_ITEM_INVALID) {
						printf("Item does not exist! try again...\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_OUT_OF_BOUNDS) {
						printf("Attempted an array access that was out of bounds!\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_PLAYER_DEAD) {
						printf("You are dead! Cannot perform actions while dead.\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_NOT_CONSUMABLE) {
						printf("Selected item is not a consumable!\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_NOT_WEAPON) {
						printf("Selected item is not a weapon!\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_NOT_FIGHTING) {
						printf("Selected action can only be performed in combat!\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_TARGET_INVALID) {
						printf("Selected enemy does not exist!\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_TARGET_FIGHTING) {
						printf("Your target is engaged in combat with someone else!\n");
					}
					else if (ReceivedMessage->Data[0] == MCERR_TARGET_DEAD) {
						printf("Your target is already dead!\n");
					}
					else {
						printf("Selection was invalid! No specific error code provided. Try again...\n");
					}
					ChooseAction(&ClientState, ConnectSocket);
				}
				else {
					printf("Received Message, but was not recognized!\n");
				}
			}
			else if (BytesReceived == 0) {
				printf("No bytes received. Closing connection...\n");
				int ShutdownResult = shutdown(ConnectSocket, SD_SEND);
				if (ShutdownResult == SOCKET_ERROR) {
					printf("shutdown failed: %d\n", WSAGetLastError());
					closesocket(ConnectSocket);
					WSACleanup();
					return 1;
					Running = false;
				}
				else {
					printf("Send Connection closed successfully.\n");
					Running = false;
				}
			}
			else {
				printf("Receive failed: %d\n", WSAGetLastError());
			}
		}
	}
	else {
		printf("WinSock initialization failed! Check error code.\n");
		closesocket(ConnectSocket);
		WSACleanup();
		PressEnterToContinue();
		return 1;
	}
	WSACleanup();
	//PressEnterToContinue();
	return 0;
}