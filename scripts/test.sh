#!/bin/bash
# Test runner for Chart Preview
#
# Usage:
#   ./scripts/test.sh              # Run all tests
#   ./scripts/test.sh -v           # Run all tests with verbose output
#   ./scripts/test.sh unit         # Run only unit tests
#   ./scripts/test.sh integration  # Run only integration tests
#   ./scripts/test.sh compliance   # Run only compliance tests

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$SCRIPT_DIR/_common.sh"

# Parse arguments
VERBOSE=""
TEST_TYPE="all"

for arg in "$@"; do
    case $arg in
        -v|--verbose)
            VERBOSE="-v"
            ;;
        unit|Unit|UNIT)
            TEST_TYPE="unit"
            ;;
        integration|Integration|INTEGRATION)
            TEST_TYPE="integration"
            ;;
        compliance|Compliance|COMPLIANCE)
            TEST_TYPE="compliance"
            ;;
        all|All|ALL)
            TEST_TYPE="all"
            ;;
        --help|-h)
            echo "Usage: ./scripts/test.sh [options] [test_type]"
            echo ""
            echo "Test types:"
            echo "  all          Run all tests (default)"
            echo "  unit         Run C++ unit tests only"
            echo "  integration  Run Python integration tests only"
            echo "  compliance   Run pluginval compliance tests only"
            echo ""
            echo "Options:"
            echo "  -v, --verbose  Verbose output"
            exit 0
            ;;
    esac
done

echo -e "${YELLOW}=== Chart Preview Test Runner ===${NC}"
echo "Test type: $TEST_TYPE"

ensure_venv

run_unit_tests() {
    echo -e "\n${BLUE}=== Running C++ Unit Tests ===${NC}"
    local unit_binary="$TESTS_DIR/unit/build/unit_tests_artefacts/Release/unit_tests"

    if [ ! -f "$unit_binary" ]; then
        echo -e "${YELLOW}Building unit tests...${NC}"
        cmake -B "$TESTS_DIR/unit/build" -S "$TESTS_DIR/unit" -DCMAKE_BUILD_TYPE=Release
        cmake --build "$TESTS_DIR/unit/build" --config Release -- -j$(cpu_count)
    fi

    pytest "$TESTS_DIR/unit" $VERBOSE
}

run_integration_tests() {
    echo -e "\n${BLUE}=== Running Integration Tests ===${NC}"
    pytest "$TESTS_DIR/integration" $VERBOSE
}

run_compliance_tests() {
    echo -e "\n${BLUE}=== Running Compliance Tests ===${NC}"
    pytest "$TESTS_DIR/compliance" $VERBOSE
}

case "$TEST_TYPE" in
    unit)
        run_unit_tests
        ;;
    integration)
        run_integration_tests
        ;;
    compliance)
        run_compliance_tests
        ;;
    all)
        run_unit_tests
        run_integration_tests
        run_compliance_tests
        ;;
esac

echo ""
echo -e "${GREEN}All tests passed!${NC}"
