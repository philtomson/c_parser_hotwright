CC = gcc
CFLAGS = -Wall -Wextra -g -std=c99
LDFLAGS = -lm

BIN_DIR = bin
SRC_DIR = src/

# Source files
SRCS = $(addprefix $(SRC_DIR), lexer.c parser.c ast.c cfg.c cfg_builder.c cfg_utils.c hw_analyzer.c cfg_to_microcode.c ast_to_microcode.c ssa_optimizer.c microcode_output.c verilog_generator.c preprocessor.c expression_evaluator.c)
OBJS = $(addprefix $(BIN_DIR)/, $(notdir $(SRCS:.c=.o)))

# Test programs
TEST_SRCS = $(addprefix $(SRC_DIR), test_cfg.c)
TEST_OBJS = $(addprefix $(BIN_DIR)/, $(notdir $(TEST_SRCS:.c=.o)))

# Main program
MAIN_SRC = $(addprefix $(SRC_DIR), main.c)
MAIN_OBJ = $(addprefix $(BIN_DIR)/, $(notdir $(MAIN_SRC:.c=.o)))

# Targets
all: $(BIN_DIR) $(BIN_DIR)/c_parser $(BIN_DIR)/test_cfg

$(BIN_DIR)/c_parser: $(OBJS) $(MAIN_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/test_cfg: $(OBJS) $(TEST_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Pattern rule for object files
$(BIN_DIR)/%.o: $(SRC_DIR)%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
$(BIN_DIR)/lexer.o: $(SRC_DIR)lexer.c $(SRC_DIR)lexer.h
$(BIN_DIR)/parser.o: $(SRC_DIR)parser.c $(SRC_DIR)parser.h $(SRC_DIR)lexer.h $(SRC_DIR)ast.h
$(BIN_DIR)/ast.o: $(SRC_DIR)ast.c $(SRC_DIR)ast.h $(SRC_DIR)lexer.h
$(BIN_DIR)/cfg.o: $(SRC_DIR)cfg.c $(SRC_DIR)cfg.h $(SRC_DIR)ast.h $(SRC_DIR)lexer.h
$(BIN_DIR)/cfg_builder.o: $(SRC_DIR)cfg_builder.c $(SRC_DIR)cfg_builder.h $(SRC_DIR)cfg.h $(SRC_DIR)ast.h $(SRC_DIR)lexer.h
$(BIN_DIR)/cfg_utils.o: $(SRC_DIR)cfg_utils.c $(SRC_DIR)cfg_utils.h $(SRC_DIR)cfg.h $(SRC_DIR)ast.h $(SRC_DIR)lexer.h
$(BIN_DIR)/hw_analyzer.o: $(SRC_DIR)hw_analyzer.c $(SRC_DIR)hw_analyzer.h $(SRC_DIR)ast.h $(SRC_DIR)lexer.h
$(BIN_DIR)/cfg_to_microcode.o: $(SRC_DIR)cfg_to_microcode.c $(SRC_DIR)cfg_to_microcode.h $(SRC_DIR)cfg.h $(SRC_DIR)hw_analyzer.h
$(BIN_DIR)/ssa_optimizer.o: $(SRC_DIR)ssa_optimizer.c $(SRC_DIR)ssa_optimizer.h $(SRC_DIR)cfg.h $(SRC_DIR)hw_analyzer.h $(SRC_DIR)lexer.h
$(BIN_DIR)/microcode_output.o: $(SRC_DIR)microcode_output.c $(SRC_DIR)cfg_to_microcode.h $(SRC_DIR)cfg.h $(SRC_DIR)hw_analyzer.h $(SRC_DIR)microcode_defs.h
$(BIN_DIR)/verilog_generator.o: $(SRC_DIR)verilog_generator.c $(SRC_DIR)verilog_generator.h $(SRC_DIR)cfg_to_microcode.h
$(BIN_DIR)/preprocessor.o: $(SRC_DIR)preprocessor.c $(SRC_DIR)preprocessor.h $(SRC_DIR)lexer.h
$(BIN_DIR)/main.o: $(SRC_DIR)main.c $(SRC_DIR)parser.h $(SRC_DIR)lexer.h $(SRC_DIR)ast.h $(SRC_DIR)cfg.h $(SRC_DIR)cfg_builder.h $(SRC_DIR)cfg_utils.h $(SRC_DIR)hw_analyzer.h $(SRC_DIR)cfg_to_microcode.h $(SRC_DIR)ssa_optimizer.h $(SRC_DIR)verilog_generator.h $(SRC_DIR)preprocessor.h
$(BIN_DIR)/expression_evaluator.o: $(SRC_DIR)expression_evaluator.c $(SRC_DIR)expression_evaluator.h $(SRC_DIR)ast.h $(SRC_DIR)lexer.h $(SRC_DIR)hw_analyzer.h
$(BIN_DIR)/test_cfg.o: $(SRC_DIR)test_cfg.c $(SRC_DIR)parser.h $(SRC_DIR)lexer.h $(SRC_DIR)ast.h $(SRC_DIR)cfg.h $(SRC_DIR)cfg_builder.h $(SRC_DIR)cfg_utils.h

# Clean
clean:
	rm -f $(OBJS) $(MAIN_OBJ) $(TEST_OBJS) $(BIN_DIR)/c_parser $(BIN_DIR)/test_cfg *.dot *.png
	rm -f test/*.vcd test/*_template.v test/*_tb.v test/Makefile.sim test/sim_main.cpp test/verilator_sim.h test/user.v
	rm -f test/*_smdata.mem test/*_vardata.mem
	rm -rf $(BIN_DIR)

# Run CFG tests
test: $(BIN_DIR)/test_cfg
	./$(BIN_DIR)/test_cfg

# Run comprehensive parser tests
run_tests: $(BIN_DIR)/c_parser
	@echo "Running comprehensive c_parser test suite..."
	@cd test && ./run_tests.sh

# Run tests with detailed output
test_verbose: $(BIN_DIR)/c_parser
	@echo "Running tests with detailed output..."
	@cd test && ./run_tests.sh --verbose

# Test specific functionality
test_multi_vars: $(BIN_DIR)/c_parser
	@echo "Testing multiple variable declarations..."
	./$(BIN_DIR)/c_parser test/test_multi_vars.c --all-hdl

test_includes: $(BIN_DIR)/c_parser
	@echo "Testing #include functionality..."
	./$(BIN_DIR)/c_parser test/test_include_multi.c --all-hdl

# Generate and view graphs
graphs: $(BIN_DIR)/test_cfg
	./$(BIN_DIR)/test_cfg
	@echo "Generating PNG files from DOT files..."
	@for dot in *.dot; do \
		if [ -f "$$dot" ]; then \
			png=$${dot%.dot}.png; \
			dot -Tpng "$$dot" -o "$$png" && echo "Generated $$png"; \
		fi \
	done

.PHONY: all clean test run_tests test_verbose test_multi_vars test_includes graphs