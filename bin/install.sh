#!/bin/bash
mkdir ~/.local
mkdir ~/.local/share
mkdir ~/.local/share/applications
touch ~/.local/share/applications/Zira.desktop
sed "s|%dir%|$PWD|g" desktop.txt > ~/.local/share/applications/Zira.desktop
temp=$(dirname $(realpath "$0"))
chmod 777 $temp/ZiraEditor
sudo apt-get install qt5-default
exec $temp/ZiraEditor
