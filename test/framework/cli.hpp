#pragma once

// CLI parsing and top-level process entry helpers for the regression runner.
// Split from test/main.cpp so main.cpp remains a narrow executable entry point.

#define NOMINMAX

#include "framework.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

namespace {

void print_usage(const char* exe) {
    std::cout
        << "usage: " << exe << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --help                         Show this help.\n"
        << "  --list                         List generated differential test specs.\n"
        << "  --list-manual                  List manual/unsafe-native coverage index.\n"
        << "  --list-gates                   List stage3 regression gate index.\n"
        << "  --manual-case <name>           Replay an indexed manual/unsafe-native coverage group.\n"
        << "  --case <exact-name>            Run/list one exact generated spec name.\n"
        << "  --filter-exact <exact-name>    Alias for --case.\n"
        << "  --filter <substring>           Run/list specs whose names contain substring.\n"
        << "  --seed-index <index>           Run one deterministic seed index.\n"
        << "  --generated-seeds <count>      Run count generated seeds per selected spec.\n"
        << "  --replay <path>                Replay a generated failure/regression JSON.\n"
        << "  --dump-features <path>         Write host feature JSON and exit.\n"
        << "  --dump-specs <path>            Write generated spec manifest JSON and exit.\n"
        << "  --record-failure <path>        Write the first failure as JSON.\n"
        << "  --record-bundle <dir>          Write failure/features into a diagnostics dir.\n"
        << "  --no-manual                    Skip manual special/regression cases.\n"
        << "  --no-regression-corpus         Skip test/regression/*.json replay files.\n"
        << "\n"
        << "Default mode runs all generated true-CPU differential tests plus manual\n"
        << "special cases and replay files from test/regression/*.json. Filtered,\n"
        << "exact-case, replay, or single-seed runs skip manual/corpus cases by design.\n";
}

bool parse_u64(const char* text, std::uint64_t& value) {
    if (!text || *text == '\0') {
        return false;
    }
    std::uint64_t result = 0;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(*cursor - '0');
        if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) {
            return false;
        }
        result = result * 10u + digit;
    }
    value = result;
    return true;
}

bool has_value(const char* text) {
    return text && *text != '\0';
}

std::string join_path(const std::string& directory, const char* leaf) {
    if (directory.empty()) {
        return leaf;
    }
    const char last = directory[directory.size() - 1];
    if (last == '\\' || last == '/') {
        return directory + leaf;
    }
    return directory + "\\" + leaf;
}

enum class ParseResult {
    Ok,
    Help,
    Error,
};

ParseResult parse_args(int argc, char** argv, cpueaxh_test::TestOptions& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return ParseResult::Help;
        }
        if (arg == "--list") {
            options.list_only = true;
            continue;
        }
        if (arg == "--list-manual") {
            options.list_manual_only = true;
            continue;
        }
        if (arg == "--list-gates") {
            options.list_gates_only = true;
            continue;
        }
        if (arg == "--manual-case") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --manual-case\n";
                return ParseResult::Error;
            }
            options.manual_case = argv[index];
            continue;
        }
        if (arg == "--no-manual") {
            options.run_manual = false;
            continue;
        }
        if (arg == "--no-regression-corpus") {
            options.run_regression_corpus = false;
            continue;
        }
        if (arg == "--case" || arg == "--filter-exact") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for " << arg << "\n";
                return ParseResult::Error;
            }
            options.exact_case = argv[index];
            continue;
        }
        if (arg == "--filter") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --filter\n";
                return ParseResult::Error;
            }
            options.filter = argv[index];
            continue;
        }
        if (arg == "--seed-index") {
            if (++index >= argc || !parse_u64(argv[index], options.seed_index)) {
                std::cerr << "invalid value for --seed-index\n";
                return ParseResult::Error;
            }
            options.has_seed_index = true;
            continue;
        }
        if (arg == "--generated-seeds") {
            if (++index >= argc || !parse_u64(argv[index], options.generated_seed_count) || options.generated_seed_count == 0) {
                std::cerr << "invalid value for --generated-seeds\n";
                return ParseResult::Error;
            }
            options.has_generated_seed_count = true;
            continue;
        }
        if (arg == "--replay") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --replay\n";
                return ParseResult::Error;
            }
            options.replay_path = argv[index];
            continue;
        }
        if (arg == "--dump-features") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --dump-features\n";
                return ParseResult::Error;
            }
            options.feature_record_path = argv[index];
            options.dump_features_only = true;
            continue;
        }
        if (arg == "--dump-specs") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --dump-specs\n";
                return ParseResult::Error;
            }
            options.spec_manifest_path = argv[index];
            options.dump_specs_only = true;
            continue;
        }
        if (arg == "--record-failure") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --record-failure\n";
                return ParseResult::Error;
            }
            options.failure_record_path = argv[index];
            continue;
        }
        if (arg == "--record-bundle") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --record-bundle\n";
                return ParseResult::Error;
            }
            options.record_bundle_dir = argv[index];
            continue;
        }

        std::cerr << "unknown option: " << arg << "\n";
        return ParseResult::Error;
    }
    if (!options.exact_case.empty() && !options.filter.empty()) {
        std::cerr << "--case/--filter-exact cannot be combined with --filter\n";
        return ParseResult::Error;
    }
    if (options.dump_features_only) {
        if (options.list_only || options.list_manual_only || options.list_gates_only || options.dump_specs_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() ||
            options.has_seed_index || options.has_generated_seed_count || !options.failure_record_path.empty() ||
            !options.record_bundle_dir.empty() || !options.replay_path.empty() || !options.run_manual || !options.run_regression_corpus) {
            std::cerr << "--dump-features cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (options.dump_specs_only) {
        if (options.list_only || options.list_manual_only || options.list_gates_only || options.dump_features_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() ||
            options.has_seed_index || options.has_generated_seed_count || !options.failure_record_path.empty() ||
            !options.record_bundle_dir.empty() || !options.replay_path.empty() || !options.run_manual || !options.run_regression_corpus) {
            std::cerr << "--dump-specs cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (!options.record_bundle_dir.empty()) {
        if (!options.failure_record_path.empty() || !options.feature_record_path.empty() || !options.spec_manifest_path.empty()) {
            std::cerr << "--record-bundle cannot be combined with --record-failure, --dump-features, or --dump-specs\n";
            return ParseResult::Error;
        }
        options.failure_record_path = join_path(options.record_bundle_dir, "failure.json");
        options.feature_record_path = join_path(options.record_bundle_dir, "cpu-features.json");
        options.spec_manifest_path = join_path(options.record_bundle_dir, "generated-specs.json");
    }
    if (!options.manual_case.empty()) {
        if (options.list_only || options.list_manual_only || options.list_gates_only || !options.exact_case.empty() || !options.filter.empty() ||
            options.has_seed_index || options.has_generated_seed_count || !options.replay_path.empty() || !options.run_manual || !options.run_regression_corpus) {
            std::cerr << "--manual-case cannot be combined with list, generated selectors, seed, replay, or skip options\n";
            return ParseResult::Error;
        }
    }
    if ((options.list_only && options.list_manual_only) || (options.list_only && options.list_gates_only) ||
        (options.list_manual_only && options.list_gates_only)) {
        std::cerr << "--list, --list-manual, and --list-gates are mutually exclusive\n";
        return ParseResult::Error;
    }
    if (options.list_manual_only) {
        if (options.list_only || options.list_gates_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() || options.has_seed_index ||
            options.has_generated_seed_count || !options.failure_record_path.empty() || !options.spec_manifest_path.empty() || !options.record_bundle_dir.empty() || !options.replay_path.empty() ||
            !options.run_manual || !options.run_regression_corpus) {
            std::cerr << "--list-manual cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (options.list_gates_only) {
        if (options.list_only || options.list_manual_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() || options.has_seed_index ||
            options.has_generated_seed_count || !options.failure_record_path.empty() || !options.spec_manifest_path.empty() || !options.record_bundle_dir.empty() || !options.replay_path.empty() ||
            !options.run_manual || !options.run_regression_corpus) {
            std::cerr << "--list-gates cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (options.list_only) {
        if (options.has_seed_index || options.has_generated_seed_count || !options.failure_record_path.empty() ||
            !options.spec_manifest_path.empty() || !options.record_bundle_dir.empty() || !options.replay_path.empty() || !options.run_manual || !options.run_regression_corpus) {
            std::cerr << "--list can only be combined with --case/--filter-exact or --filter\n";
            return ParseResult::Error;
        }
    }
    if (!options.replay_path.empty()) {
        if (options.list_only || options.list_manual_only || options.list_gates_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() ||
            options.has_seed_index || options.has_generated_seed_count || options.dump_features_only || options.dump_specs_only ||
            !options.run_manual || !options.run_regression_corpus) {
            std::cerr << "--replay cannot be combined with list, selector, seed, generated-seeds, or skip options\n";
            return ParseResult::Error;
        }
    }
    return ParseResult::Ok;
}

}


inline int run_cli(int argc, char** argv) {
    cpueaxh_test::TestOptions options;
    const ParseResult parse_result = parse_args(argc, argv, options);
    if (parse_result == ParseResult::Help) {
        return 0;
    }
    if (parse_result == ParseResult::Error) {
        print_usage(argv[0]);
        return 2;
    }

    if (!options.replay_path.empty()) {
        std::string replay_error;
        if (!cpueaxh_test::apply_replay_file(options.replay_path, options, replay_error)) {
            std::cerr << replay_error << "\n";
            return 2;
        }
    }

    if (options.dump_features_only) {
        return cpueaxh_test::dump_host_feature_record(options.feature_record_path) ? 0 : 1;
    }

    if (options.dump_specs_only) {
        return cpueaxh_test::dump_generated_spec_manifest(options.spec_manifest_path) ? 0 : 1;
    }

    return cpueaxh_test::run_all_tests(options) ? 0 : 1;
}
