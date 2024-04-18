#! /bin/sh
gcc -Wall -Wextra -Werror -o scsd `pkg-config --cflags libnotify` main.c `pkg-config --libs libnotify`
