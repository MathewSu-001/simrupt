NAME = ttt
obj-m := $(NAME).o 
ttt-objs := simrupt.o mcts.o game.o negamax.o zobrist.o xoroshiro128.o 

KDIR ?= /lib/modules/$(shell uname -r)/build
# KDIR := /usr/src/linux-headers-6.5.0-28-generic
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied
all: $(GIT_HOOKS) simrupt.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo


clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
