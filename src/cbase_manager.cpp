#include "cbase_manager.h"

namespace workload {

CBaseManager::CBaseManager(int n_threads):
    n_threads_{n_threads}
{}

ExecutionLog CBaseManager::execute_requests() {
    auto log = ExecutionLog(n_threads_);
    auto graph = generate_dependency_graph();
    auto threads_heap = initialize_threads_heap();
    auto font_heap = initialize_font_heap(graph);

    while (!font_heap.empty()) {
        // choose request to execute
        auto p = font_heap.top();
        font_heap.pop();
        auto request_ready_moment = p.first;
        auto vertice_id = p.second;

        // choose thread to execute it
        auto t = threads_heap.top();
        threads_heap.pop();
        auto thread_elapsed_time = t.first;
        auto thread_id = t.second;

        // execute request
        if (request_ready_moment > thread_elapsed_time) {
            log.skip_time(thread_id, request_ready_moment);
        }
        log.increase_elapsed_time(thread_id);

        // update graph and add new ready requests to heap
        for (auto kv : graph.vertice_edges(vertice_id)) {
            auto neighbour = kv.first;
            graph.increase_vertice_weight(neighbour);

            graph.remove_edge(vertice_id, neighbour);
            if (graph.in_degree(neighbour) == 0) {
                auto p = std::make_pair(graph.vertice_weight(neighbour), neighbour);
                font_heap.push(p);
            }
        }

        // update thread queue
        threads_heap.push(std::make_pair(log.elapsed_time(thread_id), thread_id));
    }

    return log;
}

Heap CBaseManager::initialize_threads_heap() {
    auto heap = Heap();
    for (auto i = 0; i < n_threads_; i++) {
        auto p = std::make_pair(0, i);
        heap.push(p);
    }
    return heap;
}

Heap CBaseManager::initialize_font_heap(const model::Graph& graph) {
    auto heap = Heap();
    for (auto kv : graph.in_degrees()) {
        auto vertice_id = kv.first;
        auto in_degree = kv.second;
        if (in_degree == 0) {
            heap.push(std::make_pair(0, vertice_id));
        }
    }
    return heap;
}

model::Graph CBaseManager::generate_dependency_graph() {
    auto last_vertice_with_variable = std::unordered_map<int, int>();
    auto vertex_counter = 0;
    auto dependency_graph = model::Graph();

    for (auto request : requests_) {
        dependency_graph.add_vertice(vertex_counter);

        for (auto variable : request) {
            auto neighbour = last_vertice_with_variable[variable];
            dependency_graph.add_edge(vertex_counter, neighbour);
            last_vertice_with_variable[variable] = vertex_counter;
        }

        vertex_counter++;
    }

    return dependency_graph;
}

}
