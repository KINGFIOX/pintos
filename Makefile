KERNEL = ./src/threads/build/kernel.o

gdb:
	gdb -x .gdbinit

attach:
	docker-compose exec pintos bash

up:
	docker-compose up -d

start:
	docker-compose start pintos
