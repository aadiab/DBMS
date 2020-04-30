#include "structs.hpp"
#include <queue>
#include <unordered_map>
#include "WFG.hpp"
# include <map>


struct Transaction {
    string transactionID;
    string lock_type; 
    string status; 
    string recid; 
    Transaction *next; 
};

Transaction
* newTransaction(string transactionID);

struct LockTableEntry {
    string dataElement;
    Transaction *transactionElement;
    LockTableEntry *next;
}; 

LockTableEntry
* newLockEntry(string data, string transaction); 

struct LockReleaseResult {
    string new_lock_status; 
    string new_lock_type;  
    string new_transaction; 
    string recid; 
    string signal; 
};

LockReleaseResult
* newLockRelease(); 

const int NUM_BUCKETS = 50; 
const int EXE_PROCESS = 0;  
const int EXE_TRANSACTION = 1; 

class LockTable {
	map<int, int> aborted_ts;
    private: 
        LockTableEntry **lt;
        int HashFunc(string data);
        void printTransaction(Transaction * head);
        void printLTE(LockTableEntry *lte);
        bool handle_lock_request(string transaction, string lock_type, LockTableEntry *lte, string recid);
        bool handle_record_lock(string lock_type, string transaction, LockTableEntry *lte); 
        bool handle_file_lock(string lock_type, string transaction, LockTableEntry *lte, string recid);
        bool check_repeat_transaction(string lock_type, string transaction, LockTableEntry *lte, string recid); 
    public: 
        LockTable();
        bool acquireLock(string data, string transaction, string lock_type, string recid); 
        LockReleaseResult * releaseLock(string data, string tranaction, string recid); 
        void printTable(); 
        vector<string> deadLock_detection();
};
