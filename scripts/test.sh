#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
test_dir="$project_dir/build/host-tests"
mkdir -p "$test_dir"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/tone_synth.c" \
    "$project_dir/tests/tone_synth_test.c" \
    -o "$test_dir/tone_synth_test"

"$test_dir/tone_synth_test"

if [[ ! -f "$project_dir/romfs/sample_bank.bin" ]]; then
    echo "Missing romfs/sample_bank.bin; run scripts/import-ds-sample-bank.sh" >&2
    exit 1
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/sample_bank.c" \
    "$project_dir/source/sample_library.c" \
    "$project_dir/source/sample_player.c" \
    "$project_dir/source/waveform.c" \
    "$project_dir/tests/sample_pipeline_test.c" \
    -o "$test_dir/sample_pipeline_test"

"$test_dir/sample_pipeline_test" "$project_dir/romfs/sample_bank.bin"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/granular_engine.c" \
    "$project_dir/tests/granular_engine_test.c" \
    -lm \
    -o "$test_dir/granular_engine_test"

"$test_dir/granular_engine_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/effects_chain.c" \
    "$project_dir/tests/effects_chain_test.c" \
    -lm \
    -o "$test_dir/effects_chain_test"

"$test_dir/effects_chain_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/granular_engine.c" \
    "$project_dir/source/effects_chain.c" \
    "$project_dir/source/ram_sample.c" \
    "$project_dir/tests/live_sample_switch_test.c" \
    -lm \
    -o "$test_dir/live_sample_switch_test"

"$test_dir/live_sample_switch_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/output_meter.c" \
    "$project_dir/tests/output_meter_test.c" \
    -o "$test_dir/output_meter_test"

"$test_dir/output_meter_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/edit_repeat.c" \
    "$project_dir/tests/edit_repeat_test.c" \
    -o "$test_dir/edit_repeat_test"

"$test_dir/edit_repeat_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/recording_buffer.c" \
    "$project_dir/tests/recording_buffer_test.c" \
    -o "$test_dir/recording_buffer_test"

"$test_dir/recording_buffer_test"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"$project_dir/include" \
    "$project_dir/source/ram_sample.c" \
    "$project_dir/tests/ram_sample_test.c" \
    -o "$test_dir/ram_sample_test"

"$test_dir/ram_sample_test"
