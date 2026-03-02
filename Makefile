NAME := push_swap_tester
CC := cc
CFLAGS := -Wall -Wextra -Werror
INCLUDES := -Iincludes

SRC := \
	src/main.c \
	src/engine/executor.c \
	src/engine/judge.c \
	src/engine/ui.c \
	src/utils/generator.c \
	src/utils/str_utils.c \
	tests/cases.c

OBJ := $(SRC:.c=.o)

CLR_RESET := \033[0m
CLR_INFO := \033[1;36m
CLR_OK := \033[1;32m
CLR_WARN := \033[1;33m

all: $(NAME)

$(NAME): $(OBJ)
	@printf "$(CLR_INFO)[LINK]$(CLR_RESET) %s\n" "$(NAME)"
	@$(CC) $(CFLAGS) $(INCLUDES) $(OBJ) -o $(NAME)
	@printf "$(CLR_OK)[DONE]$(CLR_RESET) %s ready\n" "$(NAME)"

%.o: %.c
	@printf "$(CLR_INFO)[CC]$(CLR_RESET) %s\n" "$<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@printf "$(CLR_WARN)[CLEAN]$(CLR_RESET) deleting object files\n"
	@rm -f $(OBJ)

fclean: clean
	@printf "$(CLR_WARN)[FCLEAN]$(CLR_RESET) deleting binary\n"
	@rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
