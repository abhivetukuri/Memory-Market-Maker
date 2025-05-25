#!/bin/bash

# Memory Market Maker - Test Runner Script
# This script runs different types of tests and benchmarks

set -e  # Exit on any error

echo "Memory Market Maker - Test Runner"
echo "================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if file exists
check_file() {
    if [ ! -f "$1" ]; then
        print_error "File not found: $1"
        return 1
    fi
    return 0
}

# Function to check if directory exists
check_directory() {
    if [ ! -d "$1" ]; then
        print_error "Directory not found: $1"
        return 1
    fi
    return 0
}

# Build the project
build_project() {
    print_status "Building project..."
    make clean
    make all
    if [ $? -eq 0 ]; then
        print_success "Build completed successfully"
    else
        print_error "Build failed"
        exit 1
    fi
}

# Run unit tests
run_unit_tests() {
    print_status "Running unit tests..."
    make test
    if [ $? -eq 0 ]; then
        print_success "Unit tests built successfully"
        ./memory_market_maker_tests
        if [ $? -eq 0 ]; then
            print_success "Unit tests passed"
        else
            print_error "Unit tests failed"
            exit 1
        fi
    else
        print_error "Unit test build failed"
        exit 1
    fi
}

# Run main program tests
run_main_tests() {
    print_status "Running main program tests..."
    ./memory_market_maker
    if [ $? -eq 0 ]; then
        print_success "Main program tests completed"
    else
        print_error "Main program tests failed"
        exit 1
    fi
}

# Run ITCH data processing test
run_itch_test() {
    print_status "Checking ITCH data file..."
    if check_file "data/sample.itch"; then
        print_status "Running ITCH data processing test..."
        ./memory_market_maker 2>&1 | grep -A 20 "ITCH Data Processing Test"
        print_success "ITCH data processing test completed"
    else
        print_warning "ITCH data file not found, skipping ITCH test"
    fi
}

# Run scenario tests
run_scenario_tests() {
    print_status "Checking scenario files..."
    if check_directory "data/matching"; then
        print_status "Running scenario tests..."
        ./memory_market_maker 2>&1 | grep -A 30 "Scenario Runner Test"
        print_success "Scenario tests completed"
    else
        print_warning "Scenario directory not found, skipping scenario tests"
    fi
}

# Run performance benchmarks
run_performance_tests() {
    print_status "Running performance benchmarks..."
    ./memory_market_maker 2>&1 | grep -A 15 "Performance Benchmark"
    print_success "Performance benchmarks completed"
}

# Run all tests
run_all_tests() {
    print_status "Running all tests..."
    
    # Build project
    build_project
    
    # Run unit tests
    run_unit_tests
    
    # Run main program tests
    run_main_tests
    
    print_success "All tests completed successfully!"
}

# Run specific test type
run_specific_test() {
    case $1 in
        "build")
            build_project
            ;;
        "unit")
            build_project
            run_unit_tests
            ;;
        "main")
            build_project
            run_main_tests
            ;;
        "itch")
            build_project
            run_itch_test
            ;;
        "scenarios")
            build_project
            run_scenario_tests
            ;;
        "performance")
            build_project
            run_performance_tests
            ;;
        "all")
            run_all_tests
            ;;
        *)
            print_error "Unknown test type: $1"
            print_status "Available test types: build, unit, main, itch, scenarios, performance, all"
            exit 1
            ;;
    esac
}

# Main script logic
if [ $# -eq 0 ]; then
    # No arguments provided, run all tests
    run_all_tests
else
    # Run specific test type
    run_specific_test "$1"
fi

print_success "Test runner completed!" 