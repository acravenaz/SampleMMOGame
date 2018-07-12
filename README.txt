========================================================================
*** README ***
(C) Copyright 2018 by Andrew Craven

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
========================================================================

***TO RUN:
This repository comes with compiled binaries for both client and server in the /Release/ folder.

Therefore, it is not necessary to compile from source code to simply run the sample programs.
Simply copy the files from the release directory to a directory of your choosing and run the 
corresponding executable. First, start GameServer.exe and ensure it binds a socket and begins
listening for connections successfully. There should be no configuration needed for this.

Then, run GameClient.exe on whichever computer you wish to connect to your server from (can be
run on the same computer as your server). When prompted, enter the IP address of your server.
This can be an internal network IP such as 192.168.0.10, an external public IP on the internet
(make sure to port forward! by default, this program uses port 37015), or localhost if you're
running the server on the same computer. When prompted, enter the name of your character
(if a character with that name doesn't exist in the server's database, it will be created)
and you will be logged in by the server unless a client is already active on a player with that name.

From there, the game begins and you can enter commands to control your character. Enter 'help'
for a list of commands.

*NOTE: This program relies in Win32 headers and netcode is implemented using the Windows Sockets API.
As such, this program only runs on Windows systems and it is recommended to use Windows 7 or later.

***TO COMPILE:
This solution was built and compiled in Visual Studio Community 2015, and should (in theory) be
compatible with newer versions of VC++ Community as well. This Project is designed to be completely 
stand-alone, with all dependencies (namely, SQLite) included in the formats needed to compile 
without any additional options or configuration. The only dependencies which are not included are 
basic Win32 headers, libs, and DLLs which are expected to be installed with a standard installation
of Visual Studio.

In other words, simply copy the entire repository to the directory of your choosing, open
Game.sln in a compatible version of Visual Studio Community, and right click both projects
(GameServer and GameClient), and click build. Check your corresponding debug or release folder
and copy sqlite3.dll from the SQLite folder to the same working directory as GameServer.exe (if needed)
and run. GameClient.exe does NOT need sqlite3.dll present in order to run.

