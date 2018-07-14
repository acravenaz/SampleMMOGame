/* ========================================================================
$File: GameShared.h
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

#include <vector>
#include <string>
using namespace std;

#define CountElements(Array) (sizeof(Array) / sizeof((Array)[0]))

#define MAX_CLIENTS 10
#define DEFAULT_PORT "37015"
#define DEFAULT_BUFFER_LENGTH 4096

#define DEFAULT_NAME_LENGTH (size_t)32
#define PLAYER_MAX_INVENTORY 11
#define ROOM_MAX_ITEMS 11
#define ROOM_MAX_ENEMIES 11
#define ROOM_MAX_CONNECTIONS 5
#define ITEM_MAX_PARAMS 10

typedef uint32_t uint32;

/* Item Types:
	0 = Nothing/Invalid
	1 = Weapon
		Param 0 = ATK Bonus
		Param 1 = DEF Bonus
	2 = Consumable
		Param 0 = HP Restored
	3 = Key
		Param 0 = Door/chest to unlock
*/

#define ITEM_WEAPON 1
#define ITEM_CONSUMABLE 2
#define ITEM_KEY 3
#define ITEM_SHIELD 4

struct item {
	char Name[DEFAULT_NAME_LENGTH];
	int ID;
	int Type;
	int Param[ITEM_MAX_PARAMS];
	uint32 RequiredKey;
	void UpdateItem(item *CopyItem) {
		strncpy_s(Name, CopyItem->Name, DEFAULT_NAME_LENGTH);
		ID = CopyItem->ID;
		Type = CopyItem->Type;
		RequiredKey = CopyItem->RequiredKey;
		for (int i = 0; i < ITEM_MAX_PARAMS; i++) {
			Param[i] = CopyItem->Param[i];
		}
	}
	//NOTE: Set an item to the default values of a particular item ID.
	//		1 = Potion, 2 = Sword, 3 = Door Key, 4 = Chest Key, 5 = Shield
	void SetItem(int ItemID) {
		if (ItemID == 1) {
			ID = 1;
			strncpy_s(Name, "Health Potion", DEFAULT_NAME_LENGTH);
			Type = ITEM_CONSUMABLE;
			Param[0] = 6; // +HP
			RequiredKey = 0;
		}
		else if (ItemID == 2) {
			ID = 2;
			strncpy_s(Name, "Sword", DEFAULT_NAME_LENGTH);
			Type = ITEM_WEAPON;
			Param[0] = 3; // +ATK
			Param[1] = 1; // +DEF
			RequiredKey = 0;
		}
		else if (ItemID == 3) {
			ID = 3;
			strncpy_s(Name, "Door Key", DEFAULT_NAME_LENGTH);
			Type = ITEM_KEY;
			Param[0] = 1; // Opens doors/items which require this param
			RequiredKey = 0;
		}
		else if (ItemID == 4) {
			ID = 4;
			strncpy_s(Name, "Chest Key", DEFAULT_NAME_LENGTH);
			Type = ITEM_KEY;
			Param[0] = 2;
			RequiredKey = 0;
		}
		else if (ItemID == 5) {
			ID = 5;
			strncpy_s(Name, "Shield", DEFAULT_NAME_LENGTH);
			Type = ITEM_SHIELD;
			Param[0] = 0;
			Param[1] = 3;
			RequiredKey = 0;
		}
		else {
			printf("ItemID specified was invalid!\n");
		}
	}
	void ClearItem() {
		strncpy_s(Name, "", DEFAULT_NAME_LENGTH);
		ID = 0;
		Type = 0;
		RequiredKey = 0;
		for (int i = 0; i < ITEM_MAX_PARAMS; i++) {
			Param[i] = 0;
		}
	}
};

//NOTE: The following are state values for players and enemies. INACTIVE means the entity doesn't exist. Players in GameSate->Players[]
//		should ALWAYS be set back to this state when logging off or disconnecting for any reason. This is what is checked to see if a player
//		slot is available.
#define INACTIVE 0
//NOTE: Alive and not presently engaged. Enemies in this state can be engaged in combat, 
//		and players in this state can engage others in combat, pick up, equip, and consume items, and move to other rooms
#define NEUTRAL 1
//NOTE: Alive and engaged in combat. Players in this state cannot pick up items, but can equip and consume them.
//		Must flee or complete combat before they can move to other rooms. Enemies in this state cannot be engaged by another player.
#define FIGHTING 2
//NOTE: Entity exists in the world but is dead. Cannot be engaged in combat, but can be looted for items.
#define DEAD 3
struct enemy;

struct player {
	char Name[DEFAULT_NAME_LENGTH];
	int CurrentRoom;
	int HPMax;
	int HP;
	int BaseATK;
	int BaseDEF;
	item Inventory[PLAYER_MAX_INVENTORY];
	//NOTE: Index in the player's Inventory[] of the equipped weapon
	uint32 Weapon;
	uint32 Shield;
	uint32 State;
	enemy *Target;
	void UpdatePlayer(player *CopyPlayer) {
		strncpy_s(Name, CopyPlayer->Name, DEFAULT_NAME_LENGTH);
		CurrentRoom = CopyPlayer->CurrentRoom;
		HPMax = CopyPlayer->HPMax;
		HP = CopyPlayer->HP;
		BaseATK = CopyPlayer->BaseATK;
		BaseDEF = CopyPlayer->BaseDEF;
		Weapon = CopyPlayer->Weapon;
		Shield = CopyPlayer->Shield;
		State = CopyPlayer->State;
		Target = CopyPlayer->Target;
		for (int i = 1; i < PLAYER_MAX_INVENTORY; i++) {
			Inventory[i].UpdateItem(&CopyPlayer->Inventory[i]);
		}
	}
	void SetupPlayer(const char *PlayerName) {
		strncpy_s(Name, PlayerName, DEFAULT_NAME_LENGTH);
		CurrentRoom = 1;
		HPMax = 10;
		HP = 10;
		BaseATK = 2;
		BaseDEF = 1;
		State = NEUTRAL;
		Weapon = 0;
		Shield = 0;
		for (int i = 0; i < PLAYER_MAX_INVENTORY; i++) {
			//NOTE: Probably not necessary, since any call to 
			//		SetupPlayer should be on a player object
			//		that's already been cleared... but just in case.
			Inventory[i].ClearItem();
		}
	}
	//NOTE: Same as SetupPlayer(), without changing the character's name.
	//		This is called on player death.
	void ResetPlayer() {
		CurrentRoom = 1;
		HPMax = 10;
		HP = 10;
		BaseATK = 2;
		BaseDEF = 1;
		State = NEUTRAL;
		Weapon = 0;
		Shield = 0;
		for (int i = 0; i < PLAYER_MAX_INVENTORY; i++) {
			//NOTE: Probably not necessary, since any call to 
			//		SetupPlayer should be on a player object
			//		that's already been cleared... but just in case.
			Inventory[i].ClearItem();
		}
	}
	void ClearPlayer() {
		strncpy_s(Name, "", DEFAULT_NAME_LENGTH);
		CurrentRoom = 0;
		HPMax = 0;
		HP = 0;
		BaseATK = 0;
		BaseDEF = 0;
		Weapon = 0;
		Shield = 0;
		State = INACTIVE;
		Target = 0;
		for (int i = 0; i < PLAYER_MAX_INVENTORY; i++) {
			Inventory[i].ClearItem();
		}
	}
};

struct enemy {
	char Name[DEFAULT_NAME_LENGTH];
	uint32 HPMax;
	int HP;
	int ATK;
	int DEF;
	item DropItem;
	uint32 State;
	void UpdateEnemy(enemy *CopyEnemy) {
		strncpy_s(Name, CopyEnemy->Name, DEFAULT_NAME_LENGTH);
		HPMax = CopyEnemy->HPMax;
		HP = CopyEnemy->HP;
		ATK = CopyEnemy->ATK;
		DEF = CopyEnemy->DEF;
		State = CopyEnemy->State;
		DropItem.UpdateItem(&CopyEnemy->DropItem);
	}
	void SetupEnemy(uint32 Type) {
		//Goblin
		if (Type == 1) {
			strncpy_s(Name, "Goblin", DEFAULT_NAME_LENGTH);
			HPMax = 10;
			HP = 10;
			ATK = 4;
			DEF = 1;
			DropItem.SetItem(4);
			State = NEUTRAL;
		}
		if (Type == 2) {
			strncpy_s(Name, "Really Big Skeleton", DEFAULT_NAME_LENGTH);
			HPMax = 18;
			HP = 18;
			ATK = 8;
			DEF = 2;
			State = NEUTRAL;
		}
	}
	void ClearEnemy() {
		strncpy_s(Name, "", DEFAULT_NAME_LENGTH);
		HPMax = 0;
		HP = 0;
		ATK = 0;
		DEF = 0;
		DropItem.ClearItem();
		State = INACTIVE;
	}
};

struct room {
	uint32 RoomID;
	uint32 RequiredKey;
	item Items[ROOM_MAX_ITEMS];
	enemy Enemies[ROOM_MAX_ENEMIES];
	// ?player *Players[10];
	uint32 ConnectingRooms[ROOM_MAX_CONNECTIONS];
	/*NOTE: Connecting rooms follows NESW convention.
			0 = INVALID
			1 = NORTH
			2 = EAST
			3 = SOUTH
			4 = WEST
	The value at each member of the array is the room 
	in GameState->Rooms that it connects to in that direction.
	0 means it doesn't connect to a room.
	*/
};

//NOTE: Combat Update Outcome noting that both combatants are alive. Expected with an MC_CMBUPDATE message.
#define COMBATANTS_ALIVE 1
//NOTE: Combat Update Outcome noting that enemy has been defeated. Expected with an MC_DISENGAGE message.
#define ENEMY_DEAD 2
//NOTE: Combat Update Outcome noting that player is dead. Expected with an MC_DISENGAGE message.
#define PLAYER_DEAD 3
//NOTE: Combat Update Outcome noting that player fled. Expected with MC_DISENGAGE. 
#define PLAYER_FLED 4
//NOTE: Combat Update Outcome noting that player attempted to flee, but died trying to escape.
#define PLAYER_FLEE_DEAD 5

struct combat_update {
	uint32 Outcome;
	uint32 PlayerDamageDealt;
	uint32 PlayerDamageTaken;
	player PlayerInfo;
	enemy EnemyInfo;
};

//NOTE: Client/Server message command definitions:

//NOTE: Message was blank/empty. This is considered invalid and should be followed immediately by another recv 
//		attempt until the client or server received valid data
#define MC_NONE (char)0x00
//NOTE: Initial message sent by client upon connecting to server
#define MC_HELLO (char)0x01
//NOTE: Response from server informing the client that it made an invalid request. Data should be an MCERR_ code
#define MC_ERROR (char)0x02
//NOTE: Request from server to client for logon information
#define MC_REQLOGON (char)0x03
//NOTE: Response from client to server with logon information. Data buffer should be filled with player's character name
#define MC_LOGON (char)0x04
//NOTE: Response from server to client with player information. Data buffer should contain a player object
#define MC_PLAYERINFO (char)0x05
//NOTE: Message from client to server asking the server to start gameplay for the active player. Server will respond with an MC_ENTERROOM
#define MC_STARTPLAYER (char)0x06
//NOTE: Server sends this message to client to tell it to enter another room. Data should be a room struct containing the room the player is entering.
#define MC_ENTERROOM (char)0x07
//NOTE: Client message asking server to move player in the specified direction. Data should range from 1-4, in NESW order
#define MC_MOVETO (char)0x08
//NOTE: Client message asking to pick up an item in the room. Data should range from 0-9
#define MC_PICKUP (char)0x09
//NOTE: Client message asking to fight an enemy in the room. Data should range from 0-9
#define MC_FIGHT (char)0x0A
//NOTE: Client message asking to flee current fight. Data should be 0
#define MC_FLEE (char)0x0B
//NOTE: Message from server updating client inventory. Data should contain the Inventory array of the player to be updated.
#define MC_UPDATEINV (char)0x0C
//NOTE: Client message requesting attack action with current weapon. Must be in status FIGHTING.
#define MC_ATTACK (char)0x0D
// UNUSED
//#define MC_BLOCK (char)0x0E
// UNUSED
//#define MC_PARRY (char)0x0F
//NOTE: Client message requesting to consume an item. Data should be the inventory slot of the item to be consumed
#define MC_USEITEM (char)0x10
//NOTE: Client message requesting to equip an item as a weapon. Data should be the inventory slot
#define MC_EQUIP (char)0x11
//NOTE: Server message informing client that the player has been engaged in combat. Data should contain the enemy object that engaged them
#define MC_ENGAGE (char)0x12
//NOTE: Server message disengaging player from combat. Client will return to the menu of the room they currently are in
#define MC_DISENGAGE (char)0x13
//NOTE: Server message updating player and enemy information after a turn of combat. Data should contain a combat_update struct
#define MC_CMBUPDATE (char)0x14
//NOTE: Server message updating player info. Typically done after consuming or equipping an item. Data should be the player struct
#define MC_PLAYERUPDATE (char)0x15
//NOTE: Client message requesting to log off.
#define MC_LOGOFF (char)0x16
//NOTE: Internal server message left directly by client handler thread to inform main thread of connect termination.
#define MC_DISCONNECT (char)0x17
//NOTE: Server message to client denying logon. Currently only happens if player is already logged on.
#define MC_LOGONFAILED (char)0x18

// **** MESSAGE ERROR CODES ****

#define MCERR_INVALIDROOM (char)0x01

#define MCERR_NOT_NEUTRAL (char)0x02

#define MCERR_ITEM_EXISTS (char)0x03

#define MCERR_INVENTORY_FULL (char)0x04

#define MCERR_ITEM_INVALID (char)0x05

#define MCERR_OUT_OF_BOUNDS (char)0x06

#define MCERR_PLAYER_DEAD (char)0x07

#define MCERR_NOT_CONSUMABLE (char)0x08

#define MCERR_NOT_WEAPON (char)0x09

#define MCERR_NOT_FIGHTING (char)0x0A

#define MCERR_TARGET_INVALID (char)0x0B

#define MCERR_MOVE_REQUIRES_KEY (char)0x0C

#define MCERR_PICKUP_REQUIRES_KEY (char)0x0D

#define MCERR_TARGET_FIGHTING (char)0x0E

#define MCERR_TARGET_DEAD (char)0x0F

#pragma pack(push, 1)
struct message {
	char Command;
	char Data[DEFAULT_BUFFER_LENGTH - 1];
};
#pragma pack(pop)