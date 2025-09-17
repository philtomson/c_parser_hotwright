#!/bin/bash

# Test runner script for c_parser
# Tests multiple variable declarations and #include functionality

echo "=== C Parser Test Suite ==="
echo "Testing multiple variable declarations and #include functionality"
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Function to run a test
run_test() {
    local test_name="$1"
    local test_file="$2"
    local expected_result="$3"  # "pass" or "fail"
    
    echo -n "Testing $test_name... "
    TESTS_RUN=$((TESTS_RUN + 1))
    
    # Run the parser
    if ../bin/c_parser "$test_file" --all-hdl > /dev/null 2>&1; then
        if [ "$expected_result" = "pass" ]; then
            echo -e "${GREEN}PASS${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}FAIL${NC} (expected failure but got success)"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        if [ "$expected_result" = "fail" ]; then
            echo -e "${GREEN}PASS${NC} (expected failure)"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}FAIL${NC} (unexpected failure)"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    fi
}

# Function to run a detailed test with output
run_detailed_test() {
    local test_name="$1"
    local test_file="$2"
    
    echo "=== $test_name ==="
    echo "File: $test_file"
    echo "Content:"
    cat "$test_file"
    echo
    echo "Parser output:"
    ../bin/c_parser "$test_file" --all-hdl
    echo
}

# Change to test directory
cd "$(dirname "$0")"

# Check if c_parser exists
if [ ! -f "../bin/c_parser" ]; then
    echo -e "${RED}Error: c_parser not found. Please run 'make' first.${NC}"
    exit 1
fi

echo "Running basic functionality tests..."
echo

# Test 1: Multiple variable declarations
run_test "Multiple variable declarations" "test_multi_vars.c" "pass"

# Test 2: Multiple variables with initialization
run_test "Multiple variables with initialization" "test_multi_init.c" "pass"

# Test 3: Include with multiple variables
run_test "Include with multiple variable declarations" "test_include_multi.c" "pass"

# Test 4: Variable scoping
run_test "Variable scoping and shadowing" "test_scoping.c" "pass"

# Test 5: Existing test files
run_test "Simple logical operations" "test_logical_ops.c" "pass"
run_test "While loop" "test_while.c" "pass"
run_test "If-while combination" "test_if_while.c" "pass"
run_test "Function features" "test_functions.c" "pass"
run_test "For loop" "test_for.c" "pass"
run_test "All loops" "test_all_loops.c" "pass"

# Test for switch with varnum
run_test "Switch with varnum" "test_switch_varnum.c" "pass"

echo
echo "=== Test Summary ==="
echo "Tests run: $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed.${NC}"
    exit 1
fi