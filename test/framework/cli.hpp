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
#include <vector>

namespace cpueaxh_test {
namespace cli_detail {

inline void print_usage(const char* exe) {
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
        << "  --require-feature <a,b>        Fail if required host features are unavailable.\n"
        << "  --require-spec <a,b>           Fail if required generated specs are not selected.\n"
        << "  --require-family <a,b>         Fail if required generated families are not selected.\n"
        << "  --record-failure <path>        Write the first failure as JSON.\n"
        << "  --record-failures <path>       Write all collected failures as JSON.\n"
        << "  --record-bundle <dir>          Write failure/features into a diagnostics dir.\n"
        << "  --fail-fast                    Stop at the first failure instead of collecting all failures.\n"
        << "  --no-manual                    Skip manual special/regression cases.\n"
        << "  --no-regression-corpus         Skip test/regression/*.json replay files.\n"
        << "\n"
        << "Default mode runs all generated true-CPU differential tests plus manual\n"
        << "special cases and replay files from test/regression/*.json. Filtered,\n"
        << "exact-case, replay, or single-seed runs skip manual/corpus cases by design.\n";
}

inline bool parse_u64(const char* text, std::uint64_t& value) {
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

inline bool has_value(const char* text) {
    return text && *text != '\0';
}

inline std::string join_path(const std::string& directory, const char* leaf) {
    if (directory.empty()) {
        return leaf;
    }
    const char last = directory[directory.size() - 1];
    if (last == '\\' || last == '/') {
        return directory + leaf;
    }
    return directory + "\\" + leaf;
}

inline bool append_csv_values(const char* text, std::vector<std::string>& out) {
    if (!has_value(text)) {
        return false;
    }
    std::string value = text;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        const std::size_t end = comma == std::string::npos ? value.size() : comma;
        const std::string item = value.substr(start, end - start);
        if (item.empty()) {
            return false;
        }
        out.push_back(item);
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return true;
}

enum class ParseResult {
    Ok,
    Help,
    Error,
};

inline ParseResult parse_args(int argc, char** argv, cpueaxh_test::TestOptions& options) {
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
        if (arg == "--record-failures") {
            if (++index >= argc || !has_value(argv[index])) {
                std::cerr << "missing value for --record-failures\n";
                return ParseResult::Error;
            }
            options.failures_record_path = argv[index];
            continue;
        }
        if (arg == "--fail-fast") {
            options.fail_fast = true;
            continue;
        }
        if (arg == "--require-feature") {
            if (++index >= argc || !append_csv_values(argv[index], options.required_features)) {
                std::cerr << "missing or invalid value for --require-feature\n";
                return ParseResult::Error;
            }
            continue;
        }
        if (arg == "--require-spec") {
            if (++index >= argc || !append_csv_values(argv[index], options.required_specs)) {
                std::cerr << "missing or invalid value for --require-spec\n";
                return ParseResult::Error;
            }
            continue;
        }
        if (arg == "--require-family") {
            if (++index >= argc || !append_csv_values(argv[index], options.required_families)) {
                std::cerr << "missing or invalid value for --require-family\n";
                return ParseResult::Error;
            }
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
    const bool has_required_coverage = !options.required_features.empty() || !options.required_specs.empty() || !options.required_families.empty();
    if (options.dump_features_only) {
        if (options.list_only || options.list_manual_only || options.list_gates_only || options.dump_specs_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() ||
            options.has_seed_index || options.has_generated_seed_count || !options.failure_record_path.empty() || !options.failures_record_path.empty() ||
            !options.record_bundle_dir.empty() || !options.replay_path.empty() || !options.run_manual || !options.run_regression_corpus || has_required_coverage) {
            std::cerr << "--dump-features cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (options.dump_specs_only) {
        if (options.list_only || options.list_manual_only || options.list_gates_only || options.dump_features_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() ||
            options.has_seed_index || options.has_generated_seed_count || !options.failure_record_path.empty() || !options.failures_record_path.empty() ||
            !options.record_bundle_dir.empty() || !options.replay_path.empty() || !options.run_manual || !options.run_regression_corpus || has_required_coverage) {
            std::cerr << "--dump-specs cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (!options.record_bundle_dir.empty()) {
        if (!options.failure_record_path.empty() || !options.failures_record_path.empty() || !options.feature_record_path.empty() || !options.spec_manifest_path.empty()) {
            std::cerr << "--record-bundle cannot be combined with --record-failure, --record-failures, --dump-features, or --dump-specs\n";
            return ParseResult::Error;
        }
        options.failure_record_path = join_path(options.record_bundle_dir, "failure.json");
        options.failures_record_path = join_path(options.record_bundle_dir, "failures.json");
        options.feature_record_path = join_path(options.record_bundle_dir, "cpu-features.json");
        options.spec_manifest_path = join_path(options.record_bundle_dir, "generated-specs.json");
    }
    if (!options.manual_case.empty()) {
        if (options.list_only || options.list_manual_only || options.list_gates_only || !options.exact_case.empty() || !options.filter.empty() ||
            options.has_seed_index || options.has_generated_seed_count || !options.replay_path.empty() || !options.run_manual || !options.run_regression_corpus || has_required_coverage) {
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
            options.has_generated_seed_count || !options.failure_record_path.empty() || !options.failures_record_path.empty() || !options.spec_manifest_path.empty() || !options.record_bundle_dir.empty() || !options.replay_path.empty() ||
            !options.run_manual || !options.run_regression_corpus || has_required_coverage) {
            std::cerr << "--list-manual cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (options.list_gates_only) {
        if (options.list_only || options.list_manual_only || !options.manual_case.empty() || !options.exact_case.empty() || !options.filter.empty() || options.has_seed_index ||
            options.has_generated_seed_count || !options.failure_record_path.empty() || !options.failures_record_path.empty() || !options.spec_manifest_path.empty() || !options.record_bundle_dir.empty() || !options.replay_path.empty() ||
            !options.run_manual || !options.run_regression_corpus || has_required_coverage) {
            std::cerr << "--list-gates cannot be combined with other options\n";
            return ParseResult::Error;
        }
    }
    if (options.list_only) {
        if (options.has_seed_index || options.has_generated_seed_count || !options.failure_record_path.empty() || !options.failures_record_path.empty() ||
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

} // namespace cli_detail

inline int run_cli(int argc, char** argv) {
    TestOptions options;
    const cli_detail::ParseResult parse_result = cli_detail::parse_args(argc, argv, options);
    if (parse_result == cli_detail::ParseResult::Help) {
        return 0;
    }
    if (parse_result == cli_detail::ParseResult::Error) {
        cli_detail::print_usage(argv[0]);
        return 2;
    }

    if (!options.replay_path.empty()) {
        std::string replay_error;
        if (!apply_replay_file(options.replay_path, options, replay_error)) {
            std::cerr << replay_error << "\n";
            return 2;
        }
    }

    if (options.dump_features_only) {
        return dump_host_feature_record(options.feature_record_path) ? 0 : 1;
    }

    if (options.dump_specs_only) {
        return dump_generated_spec_manifest(options.spec_manifest_path) ? 0 : 1;
    }

    return run_all_tests(options) ? 0 : 1;
}

} // namespace cpueaxh_test

