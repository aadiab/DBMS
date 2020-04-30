# include "structs.hpp"
# include "time.h"
# include <list>
# include <vector>

struct GNode {
	string transaction; // transaction ID
	time_t tt; // time the node added
};

GNode
* newNode(string transaction);

class WFG {
	int V; // number of nodes
	list<int> *adj; // represents the graph
	vector<GNode> transaction_ids{};
	GNode* isCyclicUtil(int v, bool visited[], bool *recStack);
public:
	WFG();
	void print();
	void addEdge(string v, string w);
	int graph_size();
	GNode* isCyclic();
	int get_id_from_transaction(GNode* t);
	int add_new_transaction(GNode* v_node);
	void remove_trasaction(GNode* t);
	vector<string> detect_deadlocks();
};
