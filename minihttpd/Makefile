CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -O0 -g
LDFLAGS :=

NAME := minihttpd

SRC := \
  src/main.c

OBJ := $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all
