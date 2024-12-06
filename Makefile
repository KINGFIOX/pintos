.PHONY: gdb attach up start

gdb:
	gdb -x .gdbinit

# NOTE: some distros use docker compose, some use docker-compose

attach:
	docker compose exec pintos bash

up:
	docker compose up -d

down:
	docker compose down

start:
	docker compose start pintos
