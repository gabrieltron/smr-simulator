#include "manager.h"

namespace workload {

Manager::Manager(
    int n_variables,
    int n_partitions,
    std::vector<long int> data_partition)
    :   n_variables_{n_variables},
        access_graph_{model::Graph(n_variables)},
        partition_scheme_{PartitionScheme(n_partitions, data_partition)}
{}

void Manager::create_single_data_random_requests(
    int n_requests,
    rfunc::Distribution distribution_pattern
) {
    // Idk how to manage this properly with other random distributions.
    // This seems general enough, but it may prove to be a bad decision
    auto random_function = rfunc::get_random_function(
        distribution_pattern, n_variables_-1
    );

    for (auto i = 0; i < n_requests; i++) {
        auto data = random_function();
        Request request = {data};
        requests_.push_back(request);
    }
}

void Manager::create_multi_data_random_requests(
    int n_requests,
    rfunc::Distribution distribution_pattern,
    int max_involved_data
) {
    auto random_function = rfunc::get_random_function(
        distribution_pattern, n_variables_-1
    );

    for (auto i = 0; i < n_requests; i++) {
        // Randomly select involved vertex
        auto request = Request();
        auto n_involved_data = (random_function()%(max_involved_data-1))+2;
        for (auto j = 0; j < n_involved_data; j++) {
            auto data = random_function();
            while (request.find(data) != request.end()) {
                data = (data + 1) % max_involved_data;
            }
            request.insert(data);
        }

        requests_.push_back(request);
    }
}

void Manager::create_fixed_quantity_requests(int requests_per_data) {
    for (auto i = 0; i < n_variables_; i++) {
        for (auto j = 0; j < requests_per_data; j++) {
            Request request = {i};
            requests_.push_back(request);
        }
    }
}

void Manager::create_multi_all_data_requests(int n_all_data_requests) {
    for (auto i = 0; i < n_all_data_requests; i++) {
        auto request = Request();
        for (auto j = 0; j < n_variables_; j++) {
            request.insert(j);
        }
        requests_.push_back(request);
    }
}

ExecutionLog Manager::execute_requests() {
    auto log = ExecutionLog(partition_scheme_.n_partitions());

    while (!requests_.empty()) {
        auto request = requests_.front();
        requests_.pop_front();

        auto involved_partitions = std::unordered_set<int>();
        for (auto data : request) {
            involved_partitions.insert(
                partition_scheme_.data_partition(data)
            );
        }

        if (involved_partitions.size() == 1) {
            auto partition = *(involved_partitions.begin());
            log.increase_elapsed_time(partition);
        } else {
            auto max_elapsed_time = log.max_elapsed_time(involved_partitions);
            for (auto partition : involved_partitions) {
                log.skip_time(partition, max_elapsed_time);
                log.increase_elapsed_time(partition);
            }
            log.increase_sync_counter();
        }
        
        update_access_graph(request);
    }

    return log;
}

void Manager::update_access_graph(Request request) {
    for (auto first_data : request) {
        access_graph_.increase_vertice_weight(first_data);
        for (auto second_data : request) {
            if (first_data == second_data) {
                continue;
            }

            if (!access_graph_.are_connected(first_data, second_data)) {
                access_graph_.add_edge(first_data, second_data);
                access_graph_.add_edge(second_data, first_data);
            }

            access_graph_.increase_edge_weight(first_data, second_data);
        }
    }
}

void Manager::repartition_data(int n_partitions) {
    auto data_partitions = model::cut_graph(access_graph_, n_partitions);
    partition_scheme_.update_partitions(n_partitions, data_partitions);
}

void Manager::export_requests(std::ostream& output_stream) {
    toml::array toml_array;
    for (auto request : requests_) {
        toml_array.push_back(request);
    }
    const toml::value data(toml_array);
    output_stream << "requests = ";
    output_stream << std::setw(80) << data << std::endl;
}

// Maybe import should be done by stream too,
// I'll do it with toml just for simplicity
// to reuse the parser
void Manager::import_requests(std::string input_path) {
    const auto requests_file = toml::parse(input_path);
    const auto requests_vector = toml::find<std::vector<std::vector<int>>>(requests_file, "requests");
    for (auto request_vector : requests_vector) {
        auto request = Request(request_vector.begin(), request_vector.end());
        requests_.push_back(request);
    }
}

model::Graph Manager::access_graph() {
    return access_graph_;
}

std::vector<long int> Manager::distribute_rand_partitions(
    rfunc::Distribution distribution, int n_partitions
) {
    auto random_function = rfunc::get_random_function(
        distribution, n_partitions-1
    );

    auto data_partitions = std::vector<long int>();
    for (auto variable = 0; variable < n_variables_; variable++) {
        auto partition = random_function();
        data_partitions.push_back(partition);
    }

    return data_partitions;
}

PartitionScheme Manager::partiton_scheme() {
    return partition_scheme_;
}

}
