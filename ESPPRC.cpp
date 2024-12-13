#include "gurobi_c++.h"
#include <cassert>
#include <sstream>
#include <iostream>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include<numeric>
#include<unordered_map>
#include<climits>

/*
* TODO: When adding a new node to a label, calculate the estimated cost of the complete path and compare it with UB 

* TODO: Since the labels turened into the unoredered map, then make sure the
* TODO: In the course of updating UB, make sure to get rid of the labels whose LB are greater than new UB
* TODO: Concatenate the forward and backward labels
*/
enum class NodeStatus {
    UNVISITED,
    OPEN,
    CLOSED
};

enum class LabelStatus {
    DOMINATED,
    INCOMPARABLE
};

// Class to represent a edge
class Edge {
public:
    int from;          // Source node
    int to;            // Destination node
    double cost;       // Cost of the edge
    std::vector<double> resources; // Resource consumption of the edge


    // Constructor
    Edge(int f, int t, double c, const std::vector<double>& r) : from(f), to(t), cost(c), resources(r) {}

};

class Graph {
private:
    std::vector<std::vector<Edge>> adjList; // Adjacency list 

public:
    int num_nodes;
	int num_res;
    std::vector<std::vector<double>> min_weight;
	std::vector<double> max_value;
    // Constructor
    Graph(int n, int m) : adjList(n),num_nodes(n),num_res(m) {
        std::vector<std::vector<double>> min_weight(n, std::vector<double>(m, 100.0));
        std::vector<double> max_value(n, 100.0);
    }

    // Add an edge to the graph
    void addEdge(int from, int to, double cost, const std::vector<double>& resources) {
        adjList[from].emplace_back(Edge(from, to, cost, resources));
    }

    // Get the neighbors of a node
    const std::vector<Edge>& getNeighbors(int node) const {
        return adjList[node];
    }
    // Display the graph
    void display() const {
        for (int i = 0; i < adjList.size(); ++i) {
            std::cout << "Node " << i << ":\n";
            for (const Edge& edge : adjList[i]) {
                std::cout << "  -> " << edge.to << " (Cost: " << edge.cost << ", Resources: [";
                for (size_t j = 0; j < edge.resources.size(); ++j) {
                    std::cout << edge.resources[j] << (j + 1 < edge.resources.size() ? ", " : "");
                }
                std::cout << "])\n";
            }
        }
    }

    std::vector<std::vector<double>> getMinWeights() {
        // outputing a vector of minimum resources' consumptions by node 
        
        for (int i = 0; i < num_nodes; i++) {
            for (const Edge& e : adjList[i]) {
                for (int k = 0; k < e.resources.size(); k++) {
                    if (min_weight[i][k] > e.resources[k]) {
                        min_weight[i][k] = e.resources[k];
                    }
                }
            }
        }
        return min_weight;
    }
    void getMaxValue() {
        // outputing a vector of maximum value by node 
        for (int i = 0; i < num_nodes; i++) {
            for (const Edge& e : adjList[i]) {
                if (max_value[i] > e.cost) {
                    max_value[i] = e.cost;
                }
            }
        }
    }
};


// Class to represent a label (partial path)
class Label {
public:
    int  vertex;                     // Current vertex
    std::vector<int> path;            // Nodes in the current path
    double cost;                      // Cost of the path
    std::vector<double> resources;    // Resource consumption of the path
    std::vector<bool> reachable;      // Reachability of the nodes
    bool half_point = false;   // Half point reached or not
    bool direction;
    double LB;
	LabelStatus status = LabelStatus::INCOMPARABLE;


    // Constructor
    Label(const int n, const std::vector<int>& p, double c, const std::vector<double>& r, const bool dr)
        : vertex(p.back()), path(p), cost(c), resources(r), reachable(n, true), direction(dr) {
        reachable[0] = false;
    }

    Label(const Label& parent, int v)
        : vertex(v), path(parent.path), cost(parent.cost), resources(parent.resources), reachable(parent.reachable), direction(parent.direction) {
    }

    // Add a node to the path
    void addNode(const Edge& edge, const Graph& graph, const std::vector<double>& res_max, const double UB) {
        if (edge.from == path.back()) {
            path.push_back(edge.to);
            reachable[edge.to] = false;
            cost += edge.cost;
			// Calculate LB, and compare it with UB
			LB = getLB(res_max,graph.num_nodes,graph.min_weight,graph.max_value)+cost;
            if (LB <= UB) {
                for (size_t i = 0; i < resources.size(); ++i) {
                    resources[i] += edge.resources[i];
                }
                for (const Edge& e : graph.getNeighbors(edge.to)) {
                    for (size_t i = 0; i < resources.size(); ++i) {
                        if (resources[i] + e.resources[i] > res_max[i]) {
                            reachable[e.to] = false; 
                            break;
                        }
                    }
                }
            }

        }
    }

    // Reaches half=point
    void reachHalfPoint(const std::vector<double>& res_max) {
        for (size_t i = 0; i < resources.size(); ++i) {
            if (resources[i] >= res_max[i] / 2) {
                half_point = true;
                break;
            }
        }
    }

    double getLB(const std::vector<double>& res_max,
        const int n,
        const std::vector<std::vector<double>>& min_weight,
        const std::vector<double>& max_value) {

        GRBEnv env = GRBEnv();
        env.set(GRB_IntParam_OutputFlag, 0);
        GRBModel model = GRBModel(env);
        std::vector<int> Items;


        GRBLinExpr obj = 0;
        std::vector<GRBVar> x(n);
        for (int i = 0; i < reachable.size(); i++) {
            int ub = reachable[i] ? 1 : 0;
            x[i] = model.addVar(0.0, ub, 0, GRB_BINARY, "x" + std::to_string(i));
            obj -= x[i] * max_value[i];
        }

        model.setObjective(obj, GRB_MAXIMIZE);

        for (int k = 0; k < res_max.size(); k++) {
            GRBLinExpr cntr = 0;
            for (int j = 0; j < x.size(); j++) {
                cntr += x[j] * min_weight[j][k];
            }
            model.addConstr(cntr <= res_max[k], "resource " + std::to_string(k));

        }
        // Optimize the model
        model.optimize();
        return model.get(GRB_DoubleAttr_ObjVal);
    }



    // Display the label
    void display() const {
        std::cout << "Path: ";
        for (int node : path) {
            std::cout << node << " ";
        }
        std::cout << ", Cost: " << cost << ", Resources: [";
        for (size_t i = 0; i < resources.size(); ++i) {
            std::cout << resources[i] << (i + 1 < resources.size() ? ", " : "");
        }
        std::cout << "]\n";
        for (size_t i = 0; i < reachable.size(); ++i) {
            if (reachable[i]) {
                std::cout << "Node " << i << " is reachable\n";
            }
        }

    }

    // Compare two labels based on cost
    LabelStatus dominance(const Label& rival) const {
        if (rival.cost >= cost) {
            for (size_t i = 0; i < resources.size(); ++i) {
                if (rival.resources[i] < resources[i]) {
                    return LabelStatus::INCOMPARABLE;
                }
            }
            for (size_t i = 0; i < reachable.size(); ++i) {
                if (rival.reachable[i] < reachable[i]) {
                    return LabelStatus::INCOMPARABLE;
                }
            }
            return LabelStatus::DOMINATED;

        }
        return LabelStatus::INCOMPARABLE;
    }
};

// Class to manage labels
class LabelManager {
private:
    std::unordered_map<int, std::vector<Label>> fw_labels, bw_labels; // Collection of labels
    double UB = 1000;
    /*
       The structure is std::unordered_map<vertex(int), label(Label)>
    */

public:
    void InSert(const Label& label) {
        unsigned int index;
        int v = label.vertex;
        if (label.direction) {
            // if label's cost smaller than index =0;
            if (label.cost < fw_labels.at(v)[0].cost) {
                fw_labels.at(v).insert(fw_labels.at(v).begin(), label);
            }
            // if label's cost greater than the last label
            if (label.cost > fw_labels.at(v).back().cost) {
                fw_labels.at(v).push_back(label);
            }
            for (int i = 0; i < fw_labels.at(v).size() - 1; i++) {
                if (label.cost >= fw_labels.at(v)[i].cost && label.cost <= fw_labels.at(v)[i + 1].cost) {
                    fw_labels.at(v).insert(fw_labels.at(v).begin() + i + 1, label);
                }
            }
        }
        else {
            // if label's cost smaller than index =0;
            if (label.cost < bw_labels.at(v)[0].cost) {
                bw_labels.at(v).insert(bw_labels.at(v).begin(), label);
            }
            // if label's cost greater than the last label
            if (label.cost > bw_labels.at(v).back().cost) {
                bw_labels.at(v).push_back(label);
            }
            for (int i = 0; i < bw_labels.at(v).size() - 1; i++) {
                if (label.cost >= bw_labels.at(v)[i].cost && label.cost <= bw_labels.at(v)[i + 1].cost) {
                    bw_labels.at(v).insert(bw_labels.at(v).begin() + i + 1, label);
                }
            }

        }

    }
    // Get all labels
    const std::unordered_map<int, std::vector<Label>>& getLabels(bool direction) const {
        if (direction) {
            return fw_labels;
        }
        return bw_labels;
    }

    // Prune labels based on a condition
    void pruneLabels(const std::function<bool(const Label&)>& condition) {
        labels.erase(std::remove_if(labels.begin(), labels.end(), condition), labels.end());
    }

    // Display all labels
    void displayLabels() const {
        for (const auto& label : labels) {
            label.display();
        }
    }
};

int main() {
    // Initialize a label manager
    LabelManager manager;
    int n = 5, m = 2;
	std::vector<double> res_max = { 5.0, 5.0 };

    // Initialize random seed
    std::srand(std::time(nullptr));
    // Create a graph
    Graph graph(n,m);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i != j) {
                // Generate m non-negative random numbers
                std::vector<double> randomResources(m);
                for (int k = 0; k < m; ++k) {
                    if (static_cast<double>(std::rand()) / RAND_MAX > 0.5) {
                        randomResources[k] = static_cast<double>(std::rand()) / RAND_MAX * 5;
                    }
                    else {
                        randomResources[k] = ceil(static_cast<double>(std::rand()) / RAND_MAX * 5);
                    }
                    graph.addEdge(i, j, (static_cast<double>(std::rand()) / RAND_MAX - 0.5) * 10, randomResources);
                }
            }
        }
    }
	graph.getMaxValue();
	graph.getMinWeights();
    graph.display();
    // Create initial label (starting from node 0)
    //Label initialLabel(n, { 0 }, 0.0, { 0.0, 0.0 }); // Path: {0}, Cost: 0.0, Resources: [0.0, 0.0]
    //manager.addLabel(initialLabel);
    //for (const Edge& edge : graph.getNeighbors(0)) {
    //    Label newLabel(initialLabel);
    //    newLabel.addNode(edge, graph, { 5.0, 5.0 });
    //    newLabel.reachHalfPoint({ 5.0, 5.0 });
    //    manager.addLabel(newLabel);
    }

    // Simulate adding labels
    // Label label1 = initialLabel;
    // label1.addNode(1, 10.0, {5.0, 2.0});
    // manager.addLabel(label1);

    // Label label2 = label1;
    // label2.addNode(2, 15.0, {3.0, 1.0});
    // manager.addLabel(label2);

    // Label label3 = label1;
    // label3.addNode(3, 12.0, {4.0, 1.5});
    // manager.addLabel(label3);

    // // Display all labels
    // std::cout << "All Labels:\n";
    // manager.displayLabels();

    // // Prune labels with cost > 20
    // manager.pruneLabels([](const Label& label) { return label.cost > 1000.0; });

    // Display remaining labels
    //std::cout << "\nLabels after pruning (Cost <= 20):\n";
    //manager.displayLabels();

    return 0;
}

