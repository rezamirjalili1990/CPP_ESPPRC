// MIP.cpp
#include "MIP.h"
#include <unordered_map>
#include <iostream>
#include <gurobi_c++.h>
#include "Graph.h"
#include "Utils.h"



void solveMIP(Graph& graph, bool LP_relaxation) {
    //std::cout << "Initializing Based Model" << std::endl;
    GRBEnv env = GRBEnv(true);
    //env.set(GRB_IntParam_OutputFlag, 0);
    //env.set(GRB_IntParam_LogToConsole, 0);

    env.start();
    // Starting LP relaxation of the problem
    GRBModel model = GRBModel(env);
    model.set(GRB_IntParam_Threads, 1);
    model.set(GRB_IntParam_Method, -1);
    model.set(GRB_IntParam_Presolve, 0); // Disable presolve (optional)
    model.set(GRB_IntParam_Cuts, 0);  // Disables all automatic cuts
    model.set(GRB_IntParam_CliqueCuts, 0);
    model.set(GRB_IntParam_CoverCuts, 0);
    model.set(GRB_IntParam_FlowCoverCuts, 0);
    model.set(GRB_IntParam_FlowPathCuts, 0);
    model.set(GRB_IntParam_GUBCoverCuts, 0);
    model.set(GRB_IntParam_ImpliedCuts, 0);
    model.set(GRB_IntParam_MIRCuts, 0);
    model.set(GRB_IntParam_ModKCuts, 0);
    model.set(GRB_IntParam_NetworkCuts, 0);
    model.set(GRB_IntParam_SubMIPCuts, 0);
    model.set(GRB_IntParam_ZeroHalfCuts, 0);
    model.set(GRB_IntParam_StrongCGCuts, 0);
    model.set(GRB_IntParam_RLTCuts, 0);
    model.set(GRB_IntParam_GomoryPasses, 0);  // Disable Gomory cuts
    model.set(GRB_IntParam_Presolve, 0);
    model.set(GRB_DoubleParam_NoRelHeurTime, 0);
    model.set(GRB_IntParam_RINS, 0);
    model.set(GRB_IntParam_Crossover, 0);






    GRBLinExpr obj = 0, inflow, outflow;
    std::unordered_map<int, std::unordered_map<int, GRBVar>> x;
    std::unordered_map<int, GRBVar> u;
    
    for (int i = 0; i < graph.num_nodes; ++i) {
        for (const auto e : graph.OutList[i]) {
            x[i][e->to] = model.addVar(0, 1, e->cost, !LP_relaxation ? GRB_BINARY:GRB_CONTINUOUS);
            obj += x[i][e->to] * e->cost;
        }
    }
    int cnt = 0;
    for (int i = 0; i < graph.num_nodes; i++) {
        u[i] = model.addVar(0.0, graph.num_nodes, 0.0, GRB_CONTINUOUS, "u[" + std::to_string(i) + "]");
    }

    // Flow conservation constraints
    for (int i = 0; i < graph.num_nodes; i++) {
        outflow = 0;
        inflow = 0;
        for (const auto e : graph.OutList[i]) {
            outflow += x[e->from][e->to];
        }
        for (const auto e : graph.InList[i]) {
            inflow += x[e->from][e->to];
        }
        if (i == 0) {
            model.addConstr(inflow == 1, "source");
            model.addConstr(outflow == 1, "sink");
        }
        else {
            model.addConstr(inflow == outflow, "flow_" + std::to_string(i));
        }
    }
    // Resource constraints
    for (int k = 0; k < graph.num_res; ++k) {
        GRBLinExpr constraint = 0;
        for (int i = 0; i < graph.num_nodes; ++i) {
            for (const auto e : graph.OutList[i]) {
                constraint += x[e->from][e->to] * e->resources[k];
            }
        }
        model.addConstr(constraint <= graph.res_max[k], "resource_" + std::to_string(k));
    }
    // Subtour elimination constraints
    for (int i = 0; i < graph.num_nodes; i++) {
        for (const auto e : graph.OutList[i]) {
            if (e->from != 0 && e->to != 0) {
                model.addConstr(u[e->from] + 1 <= u[e->to] + graph.num_nodes * (1 - x[e->to][e->from]), "subtour_"
                    + std::to_string(e->from) + "_" + std::to_string(e->to));
            }
        }
    }
    model.setObjective(obj, GRB_MINIMIZE);
    model.optimize();
    //std::cout << "Exact Solution: " << std::endl;
    std::cout << "Gurobi Optimal Objective Value: " << model.get(GRB_DoubleAttr_ObjVal) << std::endl;
    std::vector<std::pair<int,int>> sol;
    for (int i = 0; i < graph.num_nodes; i++) {
        for (const auto e : graph.OutList[i]) {
            if (x[i][e->to].get(GRB_DoubleAttr_X) > 0.5) {
                sol.emplace_back(std::make_pair(e->from, e->to));
                //std::cout << "Edge: " << e.from << " -> " << e.to << std::endl;
            }
        }
    }
    std::vector<int> path = path_maker(sol);
	std::cout << "Gurobi Path: ";
    print_vector(path);
}