#define NOMINMAX

#include "demo/framework.hpp"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

#pragma comment(lib, "cpueaxh.lib")

namespace {

void print_usage(const char* exe) {
    std::cout
        << "usage: " << exe << " [options]\n"
        << "\n"
        << "Options:\n"
        << "  --help                         Show this help.\n"
        << "  --list                         List generated differential test specs.\n"
        << "  --list-manual                  List manual/unsafe-native coverage index.\n"
        << "  --case <exact-name>            Run/list one exact generated spec name.\n"
        << "  --filter-exact <exact-name>    Alias for --case.\n"
        << "  --filter <substring>           Run/list specs whose names contain substring.\n"
        << "  --seed-index <index>           Run one deterministic seed index.\n"
        << "  --generated-seeds <count>      Run count generated seeds per selected spec.\n"
        << "  --replay <path>                Replay a generated failure/regression JSON.\n"
        << "  --record-failure <path>        Write the first failure as JSON.\n"
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
    char* end = nullptr;
    value = std::strtoull(text, &end, 10);
    return end && *end == '\0';
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
        if (arg == "--no-manual") {
            options.run_manual = false;
            continue;
        }
        if (arg == "--no-regression-corpus") {
            options.run_regression_corpus = false;
            continue;
        }
        if (arg == "--case" || arg == "--filter-exact") {
            if (++index >= argc) {
                std::cerr << "missing value for " << arg << "\n";
                return ParseResult::Error;
            }
            options.exact_case = argv[index];
            continue;
        }
        if (arg == "--filter") {
            if (++index >= argc) {
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
            continue;
        }
        if (arg == "--replay") {
            if (++index >= argc) {
                std::cerr << "missing value for --replay\n";
                return ParseResult::Error;
            }
            options.replay_path = argv[index];
            continue;
        }
        if (arg == "--record-failure") {
            if (++index >= argc) {
                std::cerr << "missing value for --record-failure\n";
                return ParseResult::Error;
            }
            options.failure_record_path = argv[index];
            continue;
        }

        std::cerr << "unknown option: " << arg << "\n";
        return ParseResult::Error;
    }
    if (!options.exact_case.empty() && !options.filter.empty()) {
        std::cerr << "--case/--filter-exact cannot be combined with --filter\n";
        return ParseResult::Error;
    }
    return ParseResult::Ok;
}

}

int main(int argc, char** argv) {
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

    return cpueaxh_test::run_all_tests(options) ? 0 : 1;
}
