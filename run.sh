gcc t2.c -Wall -lpthread -o t2 &&
gnome-terminal --command="make id=1" &&
gnome-terminal --command="make id=2" &&
gnome-terminal --command="make id=3" &&
gnome-terminal --command="make id=4"
