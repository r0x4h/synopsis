# synopsis

Build:
`gcc -o synopsis.o synopsis.c nopslib.c util.c $(pkg-config --cflags gtk+-3.0) $(pkg-config --libs gtk+-3.0) -lsqlite3 -lcurl -lpthread -rdynamic -Wall -Werror`


#Features
- View complete list of games (PS VITA only for now)
- Refresh to sync games list with nopaystation.com
- Search (by Title Id & Name)
- Ability to download .pkg files
- Decrypt & unpack .pkg files

#TODO
- Indication of download progress
- Allow multiple simultaneous downloads
- View cover art & other details about the selected game
