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
        << "  --filter <substring>           Run/list specs whose names contain substring.\n"
        << "  --seed-index <0..127>          Run one deterministic seed index.\n"
        << "  --record-failure <path>        Write the first failure as JSON.\n"
        << "  --no-manual                    Skip manual special/regression cases.\n"
        << "\n"
        << "Default mode runs all generated true-CPU differential tests plus manual\n"
        << "special cases. Filtered or single-seed runs skip manual cases by design.\n";
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
        if (arg == "--no-manual") {
            options.run_manual = false;
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

    return cpueaxh_test::run_all_tests(options) ? 0 : 1;
}
