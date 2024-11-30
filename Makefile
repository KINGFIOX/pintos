KERNEL = ./src/threads/build/kernel.o

.PHONY: gdb attach up start

gdb:
	gdb -x .gdbinit

attach:
	docker-compose exec pintos bash

up:
	docker-compose up -d

start:
	docker-compose start pintos
