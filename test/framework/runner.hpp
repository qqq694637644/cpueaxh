#pragma once
// Split from test/demo/framework.hpp: top-level generated/replay/test runner.
// Included through test/framework/framework.hpp; keep include order there.

#include "manual_registry.hpp"

namespace cpueaxh_test {

inline bool run_generated_case_by_name(
    Harness& harness,
    const std::vector<ProgramSpec>& specs,
    const std::string& case_name,
    std::uint64_t seed_index,
    Failure& failure)
{
    const ProgramSpec* spec = find_spec_by_name(specs, case_name);
    if (!spec) {
        failure.case_name = case_name;
        failure.detail = "generated test spec not found";
        return false;
    }

    const std::uint64_t seed = seeded(
        seed_index,
        static_cast<std::uint64_t>(spec->op) +
        (static_cast<std::uint64_t>(spec->family) << 8) +
        (static_cast<std::uint64_t>(spec->variant) << 16));
    BuiltCase built = build_case(*spec, seed);
    if (!harness.run_case(built, failure)) {
        attach_built_case_to_failure(failure, built, seed_index);
        return false;
    }
    return true;
}

inline bool run_all_tests(const TestOptions& options) {
    const HostFeatures features = query_host_features();
    auto record_failure = [&](Failure& failure) -> bool {
        attach_host_features_to_failure(failure, features);
        return write_failure_record(options.failure_record_path, failure);
    };

    if (!write_host_feature_record(options.feature_record_path, features)) {
        return false;
    }

    const std::vector<ProgramSpec> specs = make_specs(features);
    if (!write_generated_spec_manifest(options.spec_manifest_path, specs, features)) {
        return false;
    }

    if (options.list_manual_only) {
        print_manual_case_index();
        return true;
    }
    if (options.list_gates_only) {
        print_stage3_gate_index();
        return true;
    }

    if (options.generated_seed_count == 0) {
        std::cerr << "generated seed count must be greater than zero" << std::endl;
        return false;
    }

    if (!options.manual_case.empty()) {
        const ManualCaseIndexEntry* entry = find_manual_case_index_entry(options.manual_case);
        if (!entry) {
            std::cerr << "manual replay case is not in the manual index: " << options.manual_case << std::endl;
            return false;
        }
        std::uint64_t executed = 0;
        const std::uint64_t total = manual_special_case_count(features);
        std::cout << "cpueaxh manual replay case: " << entry->name << " [" << entry->category << "]" << std::endl;
        std::cout << "coverage: " << entry->coverage << std::endl;
        std::cout << "manual replay mode: full manual special suite" << std::endl;
        std::cout << "cpueaxh test cases: " << total << std::endl;
        Failure failure;
        if (!run_manual_special_tests(features, executed, total, &failure)) {
            record_failure(failure);
            return false;
        }
        std::cout << "PASS " << executed << "/" << total << std::endl;
        return true;
    }

    std::uint64_t selected_specs = 0;
    for (const ProgramSpec& spec : specs) {
        if (selected_by_filter(spec, options)) {
            ++selected_specs;
        }
    }

    if (options.list_only) {
        std::cout << "cpueaxh generated test specs: " << selected_specs << "/" << specs.size() << std::endl;
        for (const ProgramSpec& spec : specs) {
            if (selected_by_filter(spec, options)) {
                std::cout << spec.name << std::endl;
            }
        }
        if (options.filter.empty() && options.exact_case.empty() && !options.has_seed_index && options.run_manual) {
            std::cout << "manual special cases: " << manual_special_case_count(features) << std::endl;
        }
        if (options.filter.empty() && options.exact_case.empty() && !options.has_seed_index && options.run_regression_corpus) {
            std::vector<std::string> regression_files;
            std::string regression_error;
            if (!list_regression_replay_files(regression_files, regression_error)) {
                std::cerr << regression_error << std::endl;
                return false;
            }
            std::cout << "regression replay files: " << regression_files.size() << std::endl;
        }
        return true;
    }

    const bool run_manual = options.run_manual && options.filter.empty() && options.exact_case.empty() && !options.has_seed_index;
    const bool run_regression_corpus = options.run_regression_corpus && options.filter.empty() && options.exact_case.empty() && !options.has_seed_index;
    std::vector<std::string> regression_files;
    if (run_regression_corpus) {
        std::string regression_error;
        if (!list_regression_replay_files(regression_files, regression_error)) {
            Failure failure;
            failure.case_name = "regression_corpus";
            failure.detail = regression_error;
            std::cerr << regression_error << std::endl;
            record_failure(failure);
            return false;
        }
    }
    const std::uint64_t selected_seed_count = options.has_seed_index ? 1ull : options.generated_seed_count;
    const std::uint64_t total = selected_specs * selected_seed_count +
        (run_manual ? manual_special_case_count(features) : 0ull) +
        static_cast<std::uint64_t>(regression_files.size());
    if (total == 0) {
        std::cerr << "no test cases selected" << std::endl;
        return false;
    }
    std::cout << "cpueaxh test cases: " << total << std::endl;
    if (!options.filter.empty()) {
        std::cout << "filter: " << options.filter << std::endl;
    }
    if (!options.exact_case.empty()) {
        std::cout << "case: " << options.exact_case << std::endl;
    }
    if (options.has_seed_index) {
        std::cout << "seed-index: " << options.seed_index << std::endl;
    }
    if (options.generated_seed_count != kSeedCount && !options.has_seed_index) {
        std::cout << "generated-seeds: " << options.generated_seed_count << std::endl;
    }
    if (!run_manual) {
        std::cout << "manual special cases: skipped" << std::endl;
    }
    if (run_regression_corpus) {
        std::cout << "regression replay files: " << regression_files.size() << std::endl;
    }

    Harness harness;
    if (!harness.ready()) {
        Failure failure;
        failure.case_name = "harness_init";
        failure.detail = harness.init_error();
        std::cerr << failure.detail << std::endl;
        record_failure(failure);
        return false;
    }

    std::uint64_t executed = 0;
    auto run_generated_seed = [&](const ProgramSpec& spec, std::uint64_t seed_index) -> bool {
        const std::uint64_t seed = seeded(seed_index, static_cast<std::uint64_t>(spec.op) + (static_cast<std::uint64_t>(spec.family) << 8) + (static_cast<std::uint64_t>(spec.variant) << 16));
        BuiltCase built = build_case(spec, seed);
        Failure failure;
        if (!harness.run_case(built, failure)) {
            attach_built_case_to_failure(failure, built, seed_index);
            std::cerr << "FAIL " << failure.case_name << std::endl;
            std::cerr << failure.detail << std::endl;
            record_failure(failure);
            return false;
        }
        ++executed;
        if ((executed % 1024) == 0 || executed == total) {
            std::cout << "progress " << executed << "/" << total << std::endl;
        }
        return true;
    };

    for (const ProgramSpec& spec : specs) {
        if (!selected_by_filter(spec, options)) {
            continue;
        }
        if (options.has_seed_index) {
            if (!run_generated_seed(spec, options.seed_index)) {
                return false;
            }
        }
        else {
            for (std::uint64_t seed_index = 0; seed_index < options.generated_seed_count; ++seed_index) {
                if (!run_generated_seed(spec, seed_index)) {
                    return false;
                }
            }
        }
    }

    if (run_manual) {
        Failure failure;
        if (!run_manual_special_tests(features, executed, total, &failure)) {
            record_failure(failure);
            return false;
        }
    }

    for (const std::string& regression_file : regression_files) {
        TestOptions replay_options;
        std::string replay_error;
        if (!apply_replay_file(regression_file, replay_options, replay_error)) {
            Failure failure;
            failure.case_name = regression_file;
            failure.detail = replay_error;
            std::cerr << "FAIL regression " << regression_file << std::endl;
            std::cerr << replay_error << std::endl;
            record_failure(failure);
            return false;
        }

        Failure failure;
        if (!run_generated_case_by_name(harness, specs, replay_options.exact_case, replay_options.seed_index, failure)) {
            std::cerr << "FAIL regression " << regression_file << std::endl;
            std::cerr << failure.case_name << std::endl;
            std::cerr << failure.detail << std::endl;
            record_failure(failure);
            return false;
        }
        ++executed;
        if ((executed % 1024) == 0 || executed == total) {
            std::cout << "progress " << executed << "/" << total << std::endl;
        }
    }

    std::cout << "PASS " << executed << "/" << total << std::endl;
    return true;
}

inline bool run_all_tests() {
    return run_all_tests(TestOptions{});
}

} // namespace cpueaxh_test

