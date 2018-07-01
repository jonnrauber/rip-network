gcc t2.c -Wall -lpthread -o t2 &&
gnome-terminal -- sh -c "make id=1" &&
gnome-terminal -- sh -c "make id=2" &&
gnome-terminal -- sh -c "make id=3" &&
gnome-terminal -- sh -c "make id=4"
