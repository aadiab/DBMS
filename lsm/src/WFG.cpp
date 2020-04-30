/*
 * WFG.cpp
 *
 *  Created on: Apr 12, 2020
 *  Author: ADiab
 */

#include "WFG.hpp"

GNode *newNode(string transaction)
{
    GNode *temp = new GNode;
    temp->transaction = transaction;
    temp->tt = time(0);  // current time when the node is created
    return temp;
}

WFG::WFG() {
	this->V = 0;
	this->adj = new list<int>[100];
	//this->transaction_ids = new vector<Node>[V];
}


// returns -1 if transaction is not already in the graph
int WFG::get_id_from_transaction(GNode* t){
	GNode temp;
	for(int i =0; i<transaction_ids.size(); i++){
		temp = transaction_ids.at(i);
		if ((temp.transaction).compare(t->transaction) == 0)
			return i;
	}
	return -1;
}

int WFG::add_new_transaction(GNode* v_node){
	int new_id = V;
	V++;
	transaction_ids.push_back(*v_node);
	//adj->resize(V);
	return new_id;
}

void WFG::print(){
	cout<<"\n*************************\n";
	cout<<"Transactions:\n";
	for(int i =0; i<transaction_ids.size(); i++){
		cout<<transaction_ids.at(i).transaction<<" ";
	}
	cout<<"\nAdjacents:\n";
	for (int i=0; i<V; i++){
		cout<<transaction_ids.at(i).transaction<<" : ";
		list<int>::iterator k;
		for(k = adj[i].begin(); k != adj[i].end(); ++k)
			cout<<transaction_ids.at(*k).transaction<<" ";
		cout<<endl;
	}
	cout<<"*************************\n";
}

void WFG::addEdge(string v_t, string w_t){
	cout<<"Add Edge between "<<v_t<<"and "<<w_t<<endl;
	GNode* v = newNode(v_t);
	GNode* w = newNode(w_t);
	int v_id, w_id;
	v_id = get_id_from_transaction(v);
	w_id = get_id_from_transaction(w);
	if (v_id == -1){
		cout<<"adding new "<<v->transaction<<" to the graph"<<endl;
		v_id = add_new_transaction(v);}
	if (w_id == -1){
		cout<<"adding new "<<w->transaction<<" to the graph"<<endl;
		w_id = add_new_transaction(w);}
	adj[v_id].push_back(w_id);
	//print();

	// Run cycle-detection algorithm
	/*
	cout<<"Running dead-lock detection"<<endl;
	Node* result = isCyclic();
	if (result->transaction.compare("") == 0){
		cout<<"No deadlocks detected, continue normally.\n";
	} else {
		cout<<"Deadlock detected, removing transaction: "<<result->transaction<<endl;
		remove_trasaction(result);
		// TODO : Tell anyone else that this transaction is aborted
	}*/
}

void WFG::remove_trasaction(GNode* t){
	int id = get_id_from_transaction(t);
	cout<<"erasing node"<<t->transaction<<" from Graph"<<endl;
	//list<int>::iterator i_adj = adj->begin();

	// Remove any connection to the removed node, adjust all indices to the new one
	//cout<<"Remove edges to node from other nodes: "<<endl;
	for (int v=0; v<V; v++){
		if (v == id)
			continue;
		list<int>::iterator i = adj[v].begin(); int size = adj[v].size();
		//for(i = adj[v].begin(); i != adj[v].end(); ++i){
		for(int k = 0; k < size; ++k){
			int value = *i;
			if(value == id){
				adj[v].remove(id);
			} else if (value > id){
				adj[v].insert(i, value-1);
				adj[v].erase(i);
			}
			++i;
		}
	}
	vector<GNode>::iterator i_t = transaction_ids.begin();
	cout<<"Erasing the node from transactions list"<<endl;
	transaction_ids.erase(i_t+id);
	cout<<"Erasing the node from WFG"<<endl;
	for (int i=id+1; i<V; i++){
		adj[i-1] = adj[i];
	}
	V--;
	print();
}

GNode* WFG::isCyclicUtil(int v, bool visited[], bool *recStack){
	bool cycle_flag = false;
	if(visited[v] == false){
		// Mark the current node as visited and part of recursion stack
		visited[v] = true;
		recStack[v] = true;

		// Recur for all the vertices adjacent to this vertex
		list<int>::iterator i;
		for(i = adj[v].begin(); i != adj[v].end(); ++i){
        	if ( !visited[*i] && isCyclicUtil(*i, visited, recStack)->transaction.compare("") != 0)
                cycle_flag = true;
        	else if (recStack[*i])
        		cycle_flag = true;
            // If cycle detected, choose the transaction to be removed
            // based on "Victim Choice" policy
            if(cycle_flag == true){
            	cycle_flag = false;
            	GNode *t1 = &(transaction_ids.at(v));
            	GNode *t2 = &(transaction_ids.at(*i));
            	//cout<<"t1 has: "<<t1->transaction<<" t2 has: "<<t2->transaction<<endl;
            	// remove the transaction with highest dependencies
            	if (adj[v].size() == adj[*i].size()){
            		if(t1->tt >= t2->tt)
            			return t1;
            		else
            			return t2;
            	}
            	else if (adj[v].size() > adj[*i].size())
            		return t1;
            	else
            		return t2;
            }
        }
    }
    recStack[v] = false;  // remove the vertex from recursion stack
    return newNode("");
}

GNode* WFG::isCyclic()
{
	// Mark all the vertices as not visited and not part of recursion
	// stack
	bool *visited = new bool[V];
	bool *recStack = new bool[V];
	for(int i = 0; i < V; i++)
	{
		visited[i] = false;
		recStack[i] = false;
	}

	// Call the recursive helper function to detect cycle in different
	// DFS trees
    for(int i = 0; i < V; i++){
    	GNode *res = isCyclicUtil(i, visited, recStack);
    	//cout<<"Iteration: "<<i<<" res->trans "<<res->transaction<<endl;
        if (res->transaction.compare("") != 0)
            return res;
    }
    return newNode("");
}

int WFG::graph_size(){
	return this->V;
}

vector<string> WFG::detect_deadlocks(){
	vector<string> removed_transactions;
	bool flag = true;
	while(flag){
		GNode* result = isCyclic();
		if (result->transaction.compare("") == 0){
			cout<<"No deadlocks detected, return.\n";
			flag = false;
		} else {
			cout<<"Deadlock detected, removing transaction: "<<result->transaction<<endl;
			removed_transactions.push_back(result->transaction);
			remove_trasaction(result);
		}
	}
	return removed_transactions;
}

/*
int main (int argc, char *argv[]) {
	WFG wfg = WFG();
	cout<<"Adding T1 -> T2 ---------------"<<endl;
	wfg.addEdge("T1", "T2");
	cout<<"Graph size: "<<wfg.graph_size()<<endl;
	cout<<"Adding T2 -> T3 ---------------"<<endl;
	wfg.addEdge("T2", "T1");
	//cout<<"Adding T3 -> T1 ---------------"<<endl;
	//wfg.addEdge("T3", "T1");
	//cout<<"Graph size: "<<wfg.graph_size()<<endl;
	//cout<<"Adding T2 -> T3 ---------------"<<endl;
	//wfg.addEdge("T2", "T3");
	cout<<"Graph size: "<<wfg.graph_size()<<endl;


	cout<<"Program Ends"<<endl;
}
*/
