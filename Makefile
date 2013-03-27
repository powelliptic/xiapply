CMD = xiapply

LDLIBS += -lX11 -lXext -lXi
CFLAGS += -Wall -Werror

$(CMD): xiapply.c

clean:
	$(RM) $(CMD)
