#include "structs.hpp"
#include <queue>
#include <unordered_map>
#include "lock_table.hpp"

// A node of N-ary tree 
struct Node { 
   string type; // DB, File, Block/area code or Record
   string id;   // Specific ID 
   string lock_status; 
   string lock_type; 
   vector<Node *>child;   // An array of pointers for N children 
}; 
  
Node
*newNode(string type, string id); 

class LockManager {
    private: 
        LockTable lt = LockTable(); 
        Node *granularity_tree; 
        Node * get_file_level(string table); 
        bool add_lock(string table, string data, string transaction, string lock_type); 
        string remove_lock(string table, string data, string transaction, string lock_type); 
        string record_level_update(Node * file, string data, string new_lock_type, string new_transaction, string new_lock_status);
    public: 
        LockManager();
        bool acquire_dlock(string table, string id, string id_type);
        bool acquire_elock(string table, string data, string id, string id_type);
        bool acquire_mlock(string table, string data, string id, string id_type);
        bool acquire_rlock(string table, string data, string id, string id_type);
        bool acquire_wlock(string table, string data, string id, string id_type);
        string release_dlock(string table, string id, string id_type);
        string release_elock(string table, string data, string id, string id_type);
        string release_mlock(string table, string data, string id, string id_type);
        string release_rlock(string table, string data, string id, string id_type);
        string release_wlock(string table, string data, string id, string id_type);
        vector<string> deadLock_detection();
        void print_lock_table(); 
        void print_gran_tree(); 
};