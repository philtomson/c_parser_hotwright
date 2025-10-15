# Makefile for generating test files for the hotstate simulator

# Paths
C_PARSER_DIR = ../../
SIM_DIR = ../..

# Files (default - can be overridden from command line)
C_FILE ?= ../../examples/switches/switches.c
BASE_NAME ?= switches

# Extract base name from C file if not specified
ifeq ($(BASE_NAME),switches)
    BASE_NAME = $(notdir $(basename $(C_FILE)))
endif

# Default target
all: generate

# Generate memory files using the C parser
generate:
	@echo "Generating memory files from $(C_FILE)..."
	cd $(C_PARSER_DIR) && make c_parser
	$(C_PARSER_DIR)/c_parser $(C_FILE) --hotstate --output-dir $(SIM_DIR)/examples/basic_test/
	@echo "Memory files generated in $(SIM_DIR)/examples/basic_test/"
	@echo "Base name: $(BASE_NAME)"

# Clean generated files
clean:
	rm -f *_vardata.mem
	rm -f *_switchdata.mem
	rm -f *_smdata.mem
	rm -f *_params.vh
	rm -f *.v
	rm -f *.vh

# Run simulator with generated files
run: generate
	@echo "Running simulator with generated files..."
	cd $(SIM_DIR) && make
	$(SIM_DIR)/bin/hotstate_sim --base examples/basic_test/$(BASE_NAME) --stimulus examples/basic_test/stimulus.txt --max-cycles 100

# Run simulator with VCD output
run-vcd: generate
	@echo "Running simulator with VCD output..."
	cd $(SIM_DIR) && make
	$(SIM_DIR)/bin/hotstate_sim --base examples/basic_test/$(BASE_NAME) --stimulus examples/basic_test/stimulus.txt --format vcd --output examples/basic_test/trace.vcd --max-cycles 100

# Run simulator with CSV output
run-csv: generate
	@echo "Running simulator with CSV output..."
	cd $(SIM_DIR) && make
	$(SIM_DIR)/bin/hotstate_sim --base examples/basic_test/$(BASE_NAME) --stimulus examples/basic_test/stimulus.txt --format csv --output examples/basic_test/results.csv --max-cycles 100

# Debug mode
debug: generate
	@echo "Running simulator in debug mode..."
	cd $(SIM_DIR) && make
	$(SIM_DIR)/bin/hotstate_sim --base examples/basic_test/$(BASE_NAME) --stimulus examples/basic_test/stimulus.txt --debug --max-cycles 50

# Show generated files
show:
	@echo "Generated files:"
	@ls -la *_*.mem *_*.vh 2>/dev/null || echo "No generated files found"
	@echo "Using C file: $(C_FILE)"
	@echo "Base name: $(BASE_NAME)"

# Help
help:
	@echo "Available targets:"
	@echo "  generate           - Generate memory files from C source"
	@echo "  run                - Run simulator with console output"
	@echo "  run-vcd            - Run simulator with VCD output"
	@echo "  run-csv            - Run simulator with CSV output"
	@echo "  debug              - Run simulator in debug mode"
	@echo "  show               - Show generated files"
	@echo "  clean              - Clean generated files"
	@echo "  help               - Show this help message"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                           # Use default switches.c"
	@echo "  make C_FILE=../test.c          # Use different C file"
	@echo "  make C_FILE=../../test/simple.c BASE_NAME=simple"
	@echo ""
	@echo "Current configuration:"
	@echo "  C_FILE: $(C_FILE)"
	@echo "  BASE_NAME: $(BASE_NAME)"

.PHONY: all generate clean run run-vcd run-csv debug show help