CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
LDFLAGS = 

# Source files
SRCS = lexer.c parser.c ast.c cfg.c cfg_builder.c cfg_utils.c
OBJS = $(SRCS:.c=.o)

# Test programs
TEST_SRCS = test_cfg.c
TEST_OBJS = $(TEST_SRCS:.c=.o)

# Main program
MAIN_SRC = main.c
MAIN_OBJ = $(MAIN_SRC:.c=.o)

# Targets
all: c_parser test_cfg

c_parser: $(OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_cfg: $(OBJS) $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
lexer.o: lexer.c lexer.h
parser.o: parser.c parser.h lexer.h ast.h
ast.o: ast.c ast.h lexer.h
cfg.o: cfg.c cfg.h ast.h lexer.h
cfg_builder.o: cfg_builder.c cfg_builder.h cfg.h ast.h lexer.h
cfg_utils.o: cfg_utils.c cfg_utils.h cfg.h ast.h lexer.h
main.o: main.c parser.h lexer.h ast.h
test_cfg.o: test_cfg.c parser.h lexer.h ast.h cfg.h cfg_builder.h cfg_utils.h

# Clean
clean:
	rm -f $(OBJS) $(MAIN_OBJ) $(TEST_OBJS) c_parser test_cfg *.dot *.png

# Run tests
test: test_cfg
	./test_cfg

# Generate and view graphs
graphs: test_cfg
	./test_cfg
	@echo "Generating PNG files from DOT files..."
	@for dot in *.dot; do \
		if [ -f "$$dot" ]; then \
			png=$${dot%.dot}.png; \
			dot -Tpng "$$dot" -o "$$png" && echo "Generated $$png"; \
		fi \
	done

.PHONY: all clean test graphs