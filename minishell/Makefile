CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -O0 -g
LDFLAGS :=

NAME := minishell

SRC := \
  src/main.c \
  src/exec.c \
  src/pipe.c \
  src/redir.c \
  src/observe.c

OBJ := $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all
