#!/bin/bash

TESTS_FLDR="${PWD}/tests"
TEST_DATA_FLDR="${PWD}/tests/tests_data"
TEST_REPORT_FLDR="${PWD}/tests/tests_report"

TMP_FLDR="$TEST_DATA_FLDR/tmp"
BEFORE_FLDR="$TMP_FLDR/asan/before"
AFTER_FLDR="$TMP_FLDR/asan/after"

TOOL="./build/refactor_tool"
CLANG="clang++-20 -fsanitize=address -fno-omit-frame-pointer -g"

rm -rf "$TMP_FLDR/*"

mkdir -p "$BEFORE_FLDR"
mkdir -p "$AFTER_FLDR"

cp "$TEST_DATA_FLDR/leak_example.cpp" "$BEFORE_FLDR/leak_example.cpp"
cp "$TEST_DATA_FLDR/leak_example.cpp" "$AFTER_FLDR/leak_example.cpp"

$TOOL "$AFTER_FLDR/leak_example.cpp"

$CLANG "$BEFORE_FLDR/leak_example.cpp" -o "$BEFORE_FLDR/leak_example"
$CLANG "$AFTER_FLDR/leak_example.cpp" -o "$AFTER_FLDR/leak_example"

$BEFORE_FLDR/leak_example 2> "$BEFORE_FLDR/report.log"
$AFTER_FLDR/leak_example 2> "$AFTER_FLDR/report.log"

cp "$BEFORE_FLDR/report.log" "$TEST_REPORT_FLDR/asan_report_before.log"
cp "$AFTER_FLDR/report.log" "$TEST_REPORT_FLDR/asan_report_after.log"