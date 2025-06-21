CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
LDFLAGS = -lm

# Source files
SRCS = lexer.c parser.c ast.c cfg.c cfg_builder.c cfg_utils.c hw_analyzer.c cfg_to_microcode.c ast_to_microcode.c microcode_output.c verilog_generator.c preprocessor.c
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
hw_analyzer.o: hw_analyzer.c hw_analyzer.h ast.h lexer.h
cfg_to_microcode.o: cfg_to_microcode.c cfg_to_microcode.h cfg.h hw_analyzer.h
microcode_output.o: microcode_output.c cfg_to_microcode.h cfg.h hw_analyzer.h
verilog_generator.o: verilog_generator.c verilog_generator.h cfg_to_microcode.h
preprocessor.o: preprocessor.c preprocessor.h lexer.h
main.o: main.c parser.h lexer.h ast.h cfg.h cfg_builder.h cfg_utils.h hw_analyzer.h cfg_to_microcode.h verilog_generator.h preprocessor.h
test_cfg.o: test_cfg.c parser.h lexer.h ast.h cfg.h cfg_builder.h cfg_utils.h

# Clean
clean:
	rm -f $(OBJS) $(MAIN_OBJ) $(TEST_OBJS) c_parser test_cfg *.dot *.png
	rm -f test/*.vcd test/*_template.v test/*_tb.v test/Makefile.sim test/sim_main.cpp test/verilator_sim.h test/user.v
	rm -f test/*_smdata.mem test/*_vardata.mem

# Run CFG tests
test: test_cfg
	./test_cfg

# Run comprehensive parser tests
run_tests: c_parser
	@echo "Running comprehensive c_parser test suite..."
	@cd test && ./run_tests.sh

# Run tests with detailed output
test_verbose: c_parser
	@echo "Running tests with detailed output..."
	@cd test && ./run_tests.sh --verbose

# Test specific functionality
test_multi_vars: c_parser
	@echo "Testing multiple variable declarations..."
	./c_parser test/test_multi_vars.c --all-hdl

test_includes: c_parser
	@echo "Testing #include functionality..."
	./c_parser test/test_include_multi.c --all-hdl

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

.PHONY: all clean test run_tests test_verbose test_multi_vars test_includes graphs