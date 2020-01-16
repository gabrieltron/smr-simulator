#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <toml11/toml.hpp>
#include <unordered_map>

#include "cbase_manager.h"
#include "execution_log.h"
#include "graph_cut.h"
#include "manager.h"
#include "min_cut_manager.h"
#include "request_generation.h"
#include "write.h"
#include <memory>

typedef toml::basic_value<toml::discard_comments, std::unordered_map> toml_config;

enum ManagerType {MIN_CUT, CBASE};
const std::unordered_map<std::string, ManagerType> string_to_manager({
    {"MIN_CUT", ManagerType::MIN_CUT},
    {"CBASE", ManagerType::CBASE},
});

std::vector<workload::Request> generate_single_data_requests(
    const toml_config& config, workload::Manager& manager
) {
    const auto single_data_distribution_ = toml::find<std::string>(
        config, "workload", "requests", "single_data", "distribution_pattern"
    );
    auto single_data_distribution = rfunc::string_to_distribution.at(
        single_data_distribution_
    );

    if (single_data_distribution == rfunc::FIXED) {
        const auto requests_per_data = toml::find<int>(
            config, "workload", "requests", "single_data", "requests_per_data"
        );

        return workload::generate_fixed_data_requests(
            manager.n_variables(), requests_per_data
        );
    } else {
        const auto n_requests = toml::find<int>(
            config, "workload", "requests", "single_data", "n_requests"
        );
        if (single_data_distribution == rfunc::UNIFORM) {
            auto data_rand = rfunc::uniform_distribution_rand(
                0, manager.n_variables()-1
            );

            return workload::generate_single_data_requests(
                n_requests, data_rand
            );
        } else if (single_data_distribution == rfunc::BINOMIAL) {
            const auto success_probability = toml::find<double>(
                config, "workload", "requests", "single_data", "success_probability"
            );
            auto data_rand = rfunc::binomial_distribution(
                manager.n_variables()-1, success_probability
            );

            return workload::generate_single_data_requests(
                n_requests, data_rand
            );
        }
    }
}

std::vector<workload::Request> generate_multi_data_requests(
    const toml_config& config, workload::Manager& manager
) {
    const auto n_requests = toml::find<int>(
        config, "workload", "requests", "multi_data", "n_requests"
    );

    const auto min_involved_data = toml::find<int>(
        config, "workload", "requests", "multi_data",
        "min_involved_data"
    );
    const auto max_involved_data = toml::find<int>(
        config, "workload", "requests", "multi_data",
        "max_involved_data"
    );

    const auto size_distribution_ = toml::find<std::string>(
        config, "workload", "requests", "multi_data", "size_distribution_pattern"
    );
    auto size_distribution = rfunc::string_to_distribution.at(
        size_distribution_
    );
    rfunc::RandFunction size_rand;
    if (size_distribution == rfunc::UNIFORM) {
        size_rand = rfunc::uniform_distribution_rand(
            min_involved_data, max_involved_data
        );
    } else if (size_distribution == rfunc::FIXED) {
        size_rand = rfunc::fixed_distribution(max_involved_data);
    } else if (size_distribution == rfunc::BINOMIAL) {
        const auto success_probability = toml::find<double>(
            config, "workload", "requests", "multi_data",
            "size_success_probability"
        );
        size_rand = rfunc::ranged_binomial_distribution(
            min_involved_data, max_involved_data, success_probability
        );
    }

    const auto data_distribution_ = toml::find<std::string>(
        config, "workload", "requests", "multi_data", "data_distribution_pattern"
    );
    auto data_distribution = rfunc::string_to_distribution.at(
        data_distribution_
    );
    rfunc::RandFunction data_rand;
    if (data_distribution == rfunc::UNIFORM) {
        data_rand = rfunc::uniform_distribution_rand(2, manager.n_variables()-1);
    } else if (data_distribution == rfunc::BINOMIAL) {
        const auto success_probability = toml::find<double>(
            config, "workload", "requests", "multi_data",
            "data_success_probability"
        );
        data_rand = rfunc::binomial_distribution(
            manager.n_variables()-1, success_probability
        );
    }

    return workload::generate_multi_data_requests(
        n_requests,
        manager.n_variables(),
        data_rand,
        size_rand
    );
}

void generate_random_requests(
    const toml_config& config, workload::Manager& manager
) {
    auto single_data_requests = generate_single_data_requests(config, manager);
    auto multi_data_requests = generate_multi_data_requests(config, manager);

    const auto single_data_pick_probability = toml::find<int>(
        config, "workload", "requests", "single_data_pick_probability"
    );
    auto requests = workload::merge_requests(
        single_data_requests,
        multi_data_requests,
        single_data_pick_probability
    );

    manager.set_requests(requests);
}

void import_requests(const toml_config& config, workload::Manager& manager) {
    auto import_path = toml::find<std::string>(
        config, "workload", "requests", "import_path"
    );
    manager.import_requests(import_path);
}

std::unique_ptr<workload::CBaseManager> create_cbase_cut_manager(
    const toml_config& config, int n_variables
) {
    const auto n_threads = toml::find<int>(
        config, "execution", "n_threads"
    );

    return std::make_unique<workload::CBaseManager>(
        workload::CBaseManager(n_variables, n_threads)
    );
}

std::unique_ptr<workload::MinCutManager> create_min_cut_manager(
    const toml_config& config, int n_variables
) {
    const auto n_partitions = toml::find<int>(
        config, "workload", "initial_partitions", "n_partitions"
    );

    auto repartition_interval = 0;
    const auto should_repartition_during_execution = toml::find<bool>(
        config, "execution", "repartition_during_execution"
    );
    if (should_repartition_during_execution) {
        repartition_interval = toml::find<int>(
            config, "execution", "repartition_interval"
        );
    }

    const auto should_import_partitions = toml::find<bool>(
        config, "workload", "initial_partitions", "import"
    );

    if (should_import_partitions) {
        const auto path = toml::find<std::string>(
            config, "workload", "initial_partitions", "path"
        );
        const auto initial_partitions_file = toml::parse(path);
        auto data_partitions = toml::find<std::vector<long int>>(
            initial_partitions_file, "data_partitions"
        );
        return std::make_unique<workload::MinCutManager>(
            workload::MinCutManager(
            n_variables, n_partitions, repartition_interval, data_partitions
            )
        );
    }

    return std::make_unique<workload::MinCutManager>(
        workload::MinCutManager(
        n_variables, n_partitions, repartition_interval
        )
    );
}

void export_requests(const toml_config& config, workload::Manager& manager) {
    const auto output_path = toml::find<std::string>(
        config, "output", "requests", "path"
    );
    std::ofstream ofs(output_path, std::ofstream::out);
    manager.export_requests(ofs);
    ofs.close();
}

void export_execution_info(
    const toml_config& config,
    workload::ExecutionLog& execution_log
) {
    const auto output_path = toml::find<std::string>(
        config, "output", "info_path"
    );
    std::ofstream output_stream(output_path, std::ofstream::out);
    output::write_log_info(execution_log, output_stream);
    output_stream.close();
}

workload::ExecutionLog execute_requests(
    const toml_config& config, workload::Manager& manager
) {
    return manager.execute_requests();
}

int main(int argc, char* argv[]) {
    const auto config = toml::parse(argv[1]);

    const auto manager_type_ = toml::find<std::string>(
        config, "execution", "manager"
    );
    const auto manager_type = string_to_manager.at(manager_type_);

    const auto n_variables = toml::find<int>(
        config, "workload", "n_variables"
    );
    auto manager = [&]() -> std::unique_ptr<workload::Manager>{
        if (manager_type == ManagerType::MIN_CUT) {
            return create_min_cut_manager(config, n_variables);
        }

        return create_cbase_cut_manager(config, n_variables);
    }();


    const auto should_import_requests = toml::find<bool>(
        config, "workload", "requests", "import_requests"
    );
    if (should_import_requests) {
        import_requests(config, *manager);
    } else {
        generate_random_requests(config, *manager);
    }

    const auto should_export_requests = toml::find<bool>(
        config, "output", "requests", "export"
    );
    if (should_export_requests) {
        export_requests(config, *manager);
    }

    auto execution_log = execute_requests(config, *manager);
    export_execution_info(config, execution_log);

    const auto manager_output_path = toml::find<std::string>(
        config, "output", "manager", "path"
    );
    manager->export_data(manager_output_path);

    return 0;
}
