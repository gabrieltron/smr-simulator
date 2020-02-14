#include "graph_cut_manager.h"

namespace workload {

GraphCutManager::GraphCutManager(
    int n_variables,
    int n_partitions,
    int repartition_interval,
    std::vector<idx_t> data_partition,
    model::CutMethod cut_method)
    :   MinCutManager{
            n_variables, n_partitions,
            repartition_interval, data_partition
        },
        access_graph_{model::Graph(n_variables)},
        cut_method_{cut_method}
{}

// Distribute data in partitions with round-robin
GraphCutManager::GraphCutManager(
    int n_variables,
    int n_partitions,
    int repartition_interval,
    model::CutMethod cut_method)
    :   MinCutManager{
            n_variables, n_partitions,
            repartition_interval
        },
        access_graph_{model::Graph(n_variables)},
        cut_method_{cut_method}
{}

void GraphCutManager::repartition_data(int n_partitions) {
    auto data_partitions = model::cut_graph(
        cut_method_, access_graph_, n_partitions
    );
    partition_scheme_ = data_partitions;
}

void GraphCutManager::update_access_structure(Request request) {
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


model::Graph GraphCutManager::access_graph() {
    return access_graph_;
}

}