# mumble_bridge


This mod is divided into two parts.
The first part, in lua, runs on the server, creating an UDP connection, which when it receives a username, returns with positional coordinates.
It can be tested using the command on linux "nc -u address 44000".
So just type a username that is logged into the server and the "nc" will list positional data as the user moves.
In this part of the mod was used the formatting pattern of the mod "minetest-mumble-wrapper", created by Elkien3.
This plugin needs to be added to minetest.conf in the "secure.trusted_mods" list, as it uses a compiled socket library

The second part of the mod is a binary, which the source code is in the src directory for those who want to compile it for themselves.
This binary connects to the UDP port created on the server by the mod in lua. Then it processes the formatted data and sends it to mumble's "Link" plugin.
This binary called "wrapper" needs 2 parameters: the first is the address of the minetest server, and the second the username of the user on the server.
It should be used like this: "wrapper minetest.server.address Sam".
Before running it, you must have mumble loaded and logged in. Ideally, all users are logged into the same mumble server, so they can find each other.
When running the wrapper, we will have positional audio without using a CSM or saturating the log with information.
But for obvious reasons the Elkien3 mod is much faster, as it does not depend on the server to retrieve this positional data.

So, to this wrapper works, the server have to support it, running the lua mod.
Then you have just open and connect the mumble, enter the world/server on minetest and finally execute the wrapper with the required parameters.

have fun
