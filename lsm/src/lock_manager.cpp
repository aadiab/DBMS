#include "lock_manager.hpp"


 // Utility function to create a new tree node 
Node *newNode(string type, string id, string status) 
{ 
    Node *temp = new Node; 
    temp->type = type;
    temp->id = id; 
    temp->lock_status = status; 
    return temp; 
} 

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
bool LockManager::acquire_mlock(string table, string data, string id, string id_type){
    cout << "Called MLOCK" << "\n";
    return add_lock(table, data, id, id_type+"-MREAD");  
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
bool LockManager::acquire_rlock(string table, string data, string id, string id_type){
    cout << "Called RLOCK" << "\n"; 
    return add_lock(table, data, id, id_type+"-READ"); 
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
bool LockManager::acquire_wlock(string table, string data, string id, string id_type){
    // Check for M locks On table first
    cout << "Called WLOCK" << "\n"; 
    return add_lock(table, data, id, id_type+"-WRITE");  
}

// table = filename, id = transaction/process id, id_type either P or T
bool LockManager::acquire_dlock(string table, string id, string id_type){
    // Check for M locks on table first
    cout << "Called DLOCK" << "\n"; 
    
    // for Deletes we lock the table it self. 
    return add_lock(table, "", id, id_type+"-DELETE"); 
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
bool LockManager::acquire_elock(string table, string data, string id, string id_type){
    // Check for M locks On table first
    cout << "Called ELOCK" << "\n"; 
    return add_lock(table, data, id, id_type+"-ERASE");  
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
string LockManager::release_dlock(string table, string id, string id_type){
    cout << "Release Dlock" << "\n";
    return remove_lock(table, "", id, id_type+"-DELETE"); 
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
string LockManager::release_rlock(string table, string data, string id, string id_type) {
    cout << "Release Rlock" << "\n";
    return remove_lock(table, data, id, id_type+"-READ");
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
string LockManager::release_wlock(string table, string data, string id, string id_type) {
    cout << "Release Wlock" << "\n";
    return remove_lock(table, data, id, id_type+"-WRITE");
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
string LockManager::release_elock(string table, string data, string id, string id_type) {
    cout << "Release Elock" << "\n";
    return remove_lock(table, data, id, id_type+"-ERASE");
}

// table = filename, data = record id or area code, id = transaction/process id, id_type either P or T
string LockManager::release_mlock(string table, string data, string id, string id_type) {
    cout << "Release Mlock" << "\n";
    return remove_lock(table, data, id, id_type+"-MREAD");
}

void LockManager::print_lock_table(){
    lt.printTable(); 
}

Node * LockManager::get_file_level(string table){
    vector<Node *> files = granularity_tree->child; 
    for (auto it = files.begin(); it != files.end(); it++) {
        Node * v = *it;
        if(v->type.compare("FILE") == 0 && v->id.compare(table) == 0){
            return v; 
        }
    }

    return NULL; 
}

string LockManager::remove_lock(string table, string data, string transaction, string lock_type) {
    Node * file = get_file_level(table); 

    if(file == NULL) {
        cout << "NO LOCK FOUND" << "\n"; 
        return "FAILED"; 
    }

    // Shouldn't come here assuming valid remove lock calls. 
    if(file->lock_status.compare("GRANTED") != 0){
        cout << "Table not Locked. No Need to Release" << "\n"; 
        return ""; 
    }

    // If it's an Mread or a Delete lock, we just need to release the file level lock. 
    if(lock_type.compare("T-MREAD") == 0 || lock_type.compare("T-DELETE") == 0 || lock_type.compare("P-MREAD") == 0 || lock_type.compare("P-DELETE") == 0){
        cout << "Releasing MREAD or DELETE lock" << "\n"; 
        LockReleaseResult *result = lt.releaseLock(table, transaction, data); 

        // If Lock table returns NONE, then the table is unlocked. Change lock variables on the tree. 
        if(result->signal.compare("NONE") == 0){
            file->lock_status = "NOT GRANTED";
            file->lock_type = ""; 
            return "NONE"; 
        }

        // If Lock table return LOCKED, then the table is still locked by the head transaction. 
        // Do nothing to the tree and send NONE to the scheduler. 
        if(result->signal.compare("LOCKED") == 0) {
            return "NONE"; 
        }

        // Lock released but the next transaction in the queue has a lock thats already granted. 
        // Meaning table is still locked, just by a different transaction. 
        // In this case, we just set the file to the given lock type and return NONE. 
        if(result->signal.compare("HEADCHANGE") == 0){
            file->lock_type = result->new_lock_type; 
            file->lock_status = result->new_lock_status;  // SHOULD EQUAL GRANTED HERE. 
            return "NONE"; 
        }

        // If Lock table returns a transaction and the lock type held by this transaction is a DELETE or MREAD, we just return the transactionID
        // to scheduler to run. 
        if(result->new_lock_status.compare("GRANTED") == 0 && 
        (result->new_lock_type.compare("T-DELETE") == 0 || result->new_lock_type.compare("T-MREAD") == 0 || result->new_lock_type.compare("P-DELETE") == 0 || result->new_lock_type.compare("P-MREAD") == 0)){
            file->lock_status = result->new_lock_status; 
            file->lock_type = result->new_lock_type;
            return result->new_transaction; 
        }

        // If here, we released a MREAD or DELETE on the file and it allows a WRITE or ERASE to run on the file. 
        // acquire record level lock. If we can, return the new transaction. Return NONE otherwise. 
        if(lt.acquireLock(table+"_"+result->recid, result->new_transaction, result->new_lock_type, "")){
            file->lock_status = result->new_lock_status; 
            file->lock_type = result->new_lock_type;
            return record_level_update(file, result->recid, result->new_lock_type, result->new_transaction, "GRANTED");
        } else {
            return "NONE"; 
        }

        // Result is transaction ID that holds the next lock. 
        // Send to the scheduler to restart that transaction. 
        cout << "SHOULD NOT BE REACHED!!!!!!!!!!!!!!!!!!!!!!!!!!" << "\n";  
        file->lock_status = result->new_lock_status; 
        file->lock_type = result->new_lock_type;
        return result->new_transaction; 
    }

    // READ, WRITE OR ERASE. Remove Record lock then File lock. 
    LockReleaseResult * record_result = lt.releaseLock(table + "_" + data, transaction, "");
    LockReleaseResult * file_result = lt.releaseLock(table, transaction, data); 

    // No More locks remaining on this record. 
    // Remove lock on File.  
    if(record_result->signal.compare("NONE") == 0){
        record_level_update(file, data, "", "", "NOT GRANTED"); 

        if(file_result->signal.compare("NONE") == 0){
            file->lock_type = ""; 
            file->lock_status = "NOT GRANTED"; 
            return "NONE";
        }

        if(file_result->signal.compare("LOCKED") == 0){
            return "NONE"; 
        }

        if(file_result->signal.compare("HEADCHANGE") == 0){
            file->lock_status = file_result->new_lock_status;
            file->lock_type = file_result->new_lock_type;
            return "NONE"; 
        }

        // If Lock table returns a transaction and the lock type held by this transaction is a DELETE or MREAD, we just return the transactionID
        // to scheduler to run. 
        if(file_result->new_lock_status.compare("GRANTED") == 0 && 
        (file_result->new_lock_type.compare("T-DELETE") == 0 || file_result->new_lock_type.compare("T-MREAD") == 0 || file_result->new_lock_type.compare("P-DELETE") == 0 || file_result->new_lock_type.compare("P-MREAD") == 0)){
            file->lock_status = file_result->new_lock_status; 
            file->lock_type = file_result->new_lock_type;
            return file_result->new_transaction; 
        }

        // If here, we released a lock on the file and it allows a WRITE or ERASE to run on the file. 
        // acquire record level lock. If we can, return the new transaction. Return NONE otherwise. 
        if(lt.acquireLock(table+"_"+file_result->recid, file_result->new_transaction, file_result->new_lock_type, "")){
            record_level_update(file, file_result->recid, file_result->new_lock_type, file_result->new_transaction, "GRANTED");
            file->lock_status = file_result->new_lock_status; 
            file->lock_type = file_result->new_lock_type;
            return file_result->new_transaction;
        } else {
            file->lock_status = file_result->new_lock_status; 
            file->lock_type = file_result->new_lock_type;
            return "NONE"; 
        }
    }

    // Record Still locked. Update record node and remove lock on file. 
    if(record_result->signal.compare("LOCKED") == 0){
        // Record Is locked so no need to update tree. 

        if(file_result->signal.compare("NONE") == 0){
            cout << "UNEXPECTED NONE RETURNED ON FILE RELEASE" << "\n"; 
            exit(1); 
        }

        if(file_result->signal.compare("LOCKED") == 0){
            return "NONE"; 
        }

        if(file_result->signal.compare("HEADCHANGE") == 0){
            file->lock_status = file_result->new_lock_status;
            file->lock_type = file_result->new_lock_type;
            return "NONE"; 
        }

        // If Lock table returns a transaction and the lock type held by this transaction is a DELETE or MREAD, we just return the transactionID
        // to scheduler to run. 
        if(file_result->new_lock_status.compare("GRANTED") == 0 && 
        (file_result->new_lock_type.compare("T-DELETE") == 0 || file_result->new_lock_type.compare("T-MREAD") == 0 || file_result->new_lock_type.compare("P-DELETE") == 0 || file_result->new_lock_type.compare("P-MREAD") == 0)){
            file->lock_status = file_result->new_lock_status; 
            file->lock_type = file_result->new_lock_type;
            return file_result->new_transaction; 
        }

        // If here, we released a MREAD or DELETE on the file and it allows a WRITE or ERASE to run on the file. 
        // acquire record level lock. If we can, return the new transaction. Return NONE otherwise. 
        if(lt.acquireLock(table+"_"+file_result->recid, file_result->new_transaction, file_result->new_lock_type, "")){
            record_level_update(file, file_result->recid, file_result->new_lock_type, file_result->new_transaction, "GRANTED");
            file->lock_status = file_result->new_lock_status;
            file->lock_type = file_result->new_lock_type;
            return file_result->new_transaction; 
        } else {
            file->lock_status = file_result->new_lock_status;
            file->lock_type = file_result->new_lock_type;
            return "NONE"; 
        }
    }

    // Head Transaction was removed but the next transaction has previously been granted the lock. 
    // (Happens when two transactions are reading the same record id.)
    // Update record node and release file lock. 
    if(record_result->signal.compare("HEADCHANGE") == 0){
        record_level_update(file, data, record_result->new_lock_type, record_result->new_transaction, record_result->new_lock_status); 

        if(file_result->signal.compare("NONE") == 0){
            cout << "UNEXPECTED NONE RETURNED ON FILE RELEASE" << "\n"; 
            exit(1); 
        }

        if(file_result->signal.compare("LOCKED") == 0){
            return "NONE"; 
        }

        if(file_result->signal.compare("HEADCHANGE") == 0){
            file->lock_status = file_result->new_lock_status;
            file->lock_type = file_result->new_lock_type;
            return "NONE"; 
        }

        // If Lock table returns a transaction and the lock type held by this transaction is a DELETE or MREAD, we just return the transactionID
        // to scheduler to run. 
        if(file_result->new_lock_status.compare("GRANTED") == 0 && 
        (file_result->new_lock_type.compare("T-DELETE") == 0 || file_result->new_lock_type.compare("T-MREAD") == 0 || file_result->new_lock_type.compare("P-DELETE") == 0 || file_result->new_lock_type.compare("P-MREAD") == 0)){
            file->lock_status = file_result->new_lock_status; 
            file->lock_type = file_result->new_lock_type;
            return file_result->new_transaction; 
        }

        // If here, we released a MREAD or DELETE on the file and it allows a WRITE or ERASE to run on the file. 
        // acquire record level lock. If we can, return the new transaction. Return NONE otherwise. 
        if(lt.acquireLock(table+"_"+file_result->recid, file_result->new_transaction, file_result->new_lock_type, "")){
            record_level_update(file, file_result->recid, file_result->new_lock_type, file_result->new_transaction, "GRANTED"); 
            file->lock_status = file_result->new_lock_status;
            file->lock_type = file_result->new_lock_type;
            return file_result->new_transaction; 
        } else {
            file->lock_status = file_result->new_lock_status;
            file->lock_type = file_result->new_lock_type;
            return "NONE"; 
        }
    }

    // Last case: Removing this transaction allows a waiting transaction to start. 
    // Remove the file lock and return this transaction to the scheduler to begin the transaction. 
    return record_level_update(file, data, record_result->new_lock_type, record_result->new_transaction, "GRANTED"); 
}

string LockManager::record_level_update(Node * file, string data, string new_lock_type, string new_transaction, string new_lock_status){
    vector<Node *> records = file->child; 
    Node * record;
    for(auto it = records.begin(); it != records.end(); it++){
        record = *it; 
        if(record->id.compare(data)==0){
            record->lock_status = new_lock_status; 
            record->lock_type = new_lock_type;
            return new_transaction;
        }
    }
    record = newNode("RECORD", data, new_lock_status); 
    file->child.push_back(record); 
    return new_transaction; 
}

// Returns true if a lock was granted for the requesting transaction. 
// False otherwise. 
bool LockManager::add_lock(string table, string data, string transaction, string lock_type){
    Node * file = get_file_level(table); 

    // First Lock on this file. Push this table to the DB node. 
    if(file == NULL){
        // Acquire file Lock 
        if(lt.acquireLock(table, transaction, lock_type, data)){
            // for Mreads and Deletes we don't need to lock the record. Just the file. 
            if(lock_type.compare("T-MREAD") == 0 || lock_type.compare("T-DELETE") == 0 || lock_type.compare("P-MREAD") == 0 || lock_type.compare("P-DELETE") == 0) {
                Node * newFile = newNode("FILE", table, "GRANTED"); 
                newFile->lock_type = lock_type; 
                granularity_tree->child.push_back(newFile); 
                return true;
            }

            // Otherwise Acquire the record lock. 
            if(lt.acquireLock(table+"_"+data, transaction, lock_type, "")){
                Node * newFile = newNode("FILE", table, "GRANTED"); 
                newFile->lock_type = lock_type; 
                Node * newRec = newNode("RECORD", data, "GRANTED");
                newRec->lock_type = lock_type;  
                newFile->child.push_back(newRec); 
                granularity_tree->child.push_back(newFile); 
                return true; 
            }
        }
        cout << "File Does not exist and lt couldn't acquire a lock.  " << "\n"; 
        return false; 
    } 

    cout << "FOUND TABLE " + table << "\n"; 
    bool acquire_file_lock = lt.acquireLock(table, transaction, lock_type, data); 
    if(acquire_file_lock) {
        file->lock_status = "GRANTED"; 
        file->lock_type = lock_type; 

        if(lock_type.compare("T-MREAD") == 0 || lock_type.compare("T-DELETE") == 0 || lock_type.compare("P-MREAD") == 0 || lock_type.compare("P-DELETE") == 0){
            return true; 
        }

        // For READ, WRITE and ERASE we need to acquire the record lock. 
        bool acquire_record_lock = lt.acquireLock(table+"_"+data, transaction, lock_type, ""); 
        if(acquire_record_lock){
            vector<Node *> records = file->child;
            Node * record; 
            for(auto it = records.begin(); it != records.end(); it++){
                record = *it; 
                if(record->id.compare(data) == 0){
                    record->lock_status = "GRANTED"; 
                    record->lock_type = lock_type;
                    return true; 
                }
            }
            record = newNode("RECORD", data, "GRANTED"); 
            record->lock_type = lock_type; 
            file->child.push_back(record);
            return true; 
        }
    }
    return false; 
}


void LockManager::print_gran_tree(){
    vector<Node *> files = granularity_tree->child; 
    vector<Node *> records; 
    Node * file; 
    Node * record; 
    for(auto it = files.begin(); it != files.end(); it++){
        file = *it; 
        cout << "FILE: " + file->id + " Lock Type: " + file->lock_type + " Lock Status: " + file->lock_status << "\n";
        records = file->child; 
        for(auto it2 = records.begin(); it2 != records.end(); it2++){
            record = *it2; 
            cout << "RECORD: " + record->id + " Lock Type: " + record->lock_type + " Lock Status: " + record->lock_status << "\n";
        }
    }
}

// Connection method, detects DL from LT
vector<string> LockManager::deadLock_detection(){
	vector<string> removed_transactions;

	// Detect Deadlocks from LockTable
	removed_transactions = lt.deadLock_detection();

	// The actual abort will happen on the upper level
	return removed_transactions;
}

LockManager::LockManager() {
    granularity_tree = newNode("DB", "-1", "UNLOCKED"); 
    granularity_tree->lock_type = "NONE";
}
/*
int main (int argc, char *argv[]) {
    cout << "TEST" << "\n"; 
    LockManager lm = LockManager(); 
    // Test Case 1: 
    // Read Lock, Mread lock, write lock, erase lock and delete lock 
    // Expected: read and Mread lock are granted at file level, All others are waiting in File level. 
    // lm.acquire_rlock("TABLE", "RECORD1", "T1", "T"); 
    // lm.acquire_mlock("TABLE", "RECORD1", "T2", "T"); 
    // lm.acquire_wlock("TABLE", "RECORD1", "T3", "T"); 
    // lm.acquire_elock("TABLE", "RECORD1", "T4", "T"); 
    // lm.acquire_dlock("TABLE", "T5", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 

    // Test Case 2: 
    // Release Read lock. 
    // Expected: Return NONE, Lock on MREAD is still granted, all others are still waiting at the file level. 
    // string test2 = lm.release_rlock("TABLE", "RECORD1", "T1", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 
    // cout << "Returned: " + test2 << "\n"; 

    // Test Case 3: 
    // Release Mread lock. 
    // Expected: Return T3, Lock on Write is granted, all others are still waiting at the file level. 
    // string test3 = lm.release_mlock("TABLE", "RECORD1", "T2", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 
    // cout << "Returned: " + test3 << "\n"; 

    // Test Case 4: 
    // Release Write lock. 
    // Expected: Return T4, Lock on Erase is granted, all others are still waiting at the file level. 
    // string test4 = lm.release_wlock("TABLE", "RECORD1", "T3", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 
    // cout << "Returned: " + test4 << "\n"; 

    // Test Case 5: 
    // Release Erase lock 
    // Expected : Returns T5, Delete lock is granted. 
    // string test5 = lm.release_elock("TABLE", "RECORD1", "T4", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 
    // cout << "Returned: " + test5 << "\n";

    // Test Case 6: 
    // Since we have a delete on file, all incoming requests are waiting. 
    // lm.acquire_rlock("TABLE", "RECORD1", "T1", "T"); 
    // lm.acquire_wlock("TABLE", "RECORD3", "T3", "T"); 
    // lm.acquire_elock("TABLE", "RECORD4", "T4", "T"); 
    // lm.acquire_mlock("TABLE", "RECORD1", "T2", "T"); 
    // lm.acquire_dlock("TABLE", "T6", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 
    
    // Test case 7: 
    // Removing Delete lock
    // Expected: Return T1, let's T1's read run on the file and sets all others to wait. 
    // string test7 = lm.release_dlock("TABLE", "T5", "T");
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Returns: " + test7 << "\n"; 

    // Test Case 8: 
    // Multiple Record access on same file. 
    // Expected: Read, Write, and Erase are granted to the record level, MRead and delete waiting on file level. 
    // lm.acquire_rlock("TABLE", "RECORD1", "T1", "T"); 
    // lm.acquire_wlock("TABLE", "RECORD3", "T3", "T"); 
    // lm.acquire_elock("TABLE", "RECORD4", "T4", "T"); 
    // lm.acquire_mlock("TABLE", "RECORD1", "T2", "T"); 
    // lm.acquire_dlock("TABLE", "T5", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 

    // Test case 9: 
    // Multiple Files
    // Expected: Read, Write, and Erase are granted to the record level, MRead and delete waiting on file level. 
    // lm.acquire_rlock("TABLE1", "RECORD1", "T10", "T"); 
    // lm.acquire_wlock("TABLE1", "RECORD3", "T30", "T"); 
    // lm.acquire_elock("TABLE1", "RECORD4", "T40", "T"); 
    // lm.acquire_mlock("TABLE1", "RECORD1", "T20", "T"); 
    // lm.acquire_dlock("TABLE1", "T5", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree(); 

    // Test case 10: 
    // Multiple files
    // release a few from each file. 
    // lm.release_rlock("TABLE", "RECORD1", "T1", "T");
    // lm.release_mlock("TABLE", "RECORD1", "T2", "T");
    // lm.release_rlock("TABLE1", "RECORD1", "T10", "T"); 
    // lm.release_wlock("TABLE1", "RECORD3", "T30", "T"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree();

    // Test Case 11
    // Transaction - Process interactions (R-R) 
    // Expected: Both Lock should be granted. 
    // bool t_test = lm.acquire_rlock("TABLE", "RECORD1", "T1", "T"); 
    // bool p_test = lm.acquire_rlock("TABLE", "RECORD1", "T1", "P"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Transaction: " + to_string(t_test) + " Process: " + to_string(p_test) << endl; 

    // Test Case 11
    // Transaction - Process interactions (R-W) 
    // Expected: Read should be grant, Write and Erase should be waiting at the record level. 
    // bool t_test = lm.acquire_rlock("TABLE", "RECORD1", "T1", "T"); 
    // bool p_test = lm.acquire_wlock("TABLE", "RECORD1", "T1", "P"); 
    // bool p_test2 = lm.acquire_elock("TABLE", "RECORD1", "T1", "P"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Transaction: " + to_string(t_test) + " Process: " + to_string(p_test) + to_string(p_test2) << endl;

    // Test Case 12 
    // Transaction - Process Interactions (W-R) & (W-W)
    // Expected: Write is granted, Read/Write/Erase is waiting at the record level. 
    // bool t_test = lm.acquire_wlock("TABLE", "RECORD1", "T1", "T"); 
    // bool p_test = lm.acquire_rlock("TABLE", "RECORD1", "T2", "P"); 
    // bool p_testw = lm.acquire_wlock("TABLE", "RECORD1", "T3", "P");
    // bool p_teste = lm.acquire_elock("TABLE", "RECORD1", "T4", "P");
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Transaction: " + to_string(t_test) + " Process: " + to_string(p_test && p_teste && p_testw) << endl;

    // Test Case 13 
    // Transaction - Process Interactions (W-R) & (W-W)
    // Expected: Erase is granted, Read/Write/Erase is waiting at the record level. 
    // bool t_test = lm.acquire_elock("TABLE", "RECORD1", "T1", "T"); 
    // bool p_test = lm.acquire_rlock("TABLE", "RECORD1", "T2", "P"); 
    // bool p_testw = lm.acquire_wlock("TABLE", "RECORD1", "T3", "P");
    // bool p_teste = lm.acquire_elock("TABLE", "RECORD1", "T4", "P");
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Transaction: " + to_string(t_test) + " Process: " + to_string(p_test && p_teste && p_testw) << endl;

    // Test Case 14
    // Transaction - Process Interactions (E or W - Mread)
    // Expected: Erase or Write is granted, Mread is waiting
    // bool t_test = lm.acquire_elock("TABLE", "RECORD1", "T1", "T"); 
    // bool t_test = lm.acquire_wlock("TABLE", "RECORD1", "T1", "T"); 
    // bool p_test = lm.acquire_mlock("TABLE", "RECORD1", "T2", "P"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Transaction: " + to_string(t_test) + " Process: " + to_string(p_test) << endl;

    // Test Case 15
    // Transaction - Process Interactions (E or W - Delete)
    // Expected: Erase or Write is granted, Delete is waiting
    // bool t_test = lm.acquire_elock("TABLE", "RECORD1", "T1", "T"); 
    // bool t_test = lm.acquire_wlock("TABLE", "RECORD1", "T1", "T"); 
    // bool p_test = lm.acquire_dlock("TABLE", "T2", "P"); 
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Transaction: " + to_string(t_test) + " Process: " + to_string(p_test) << endl;

    // Test Case 16
    // Multiple locks on the same file by the same transaction. Releasing locks shouldn't remove file locks. 
    // Expected: Two read locks granted at the file level. One is removed when release is called. 
    // lm.acquire_rlock("TABLE", "RECORD1", "T1", "T");
    // lm.acquire_rlock("TABLE", "RECORD2", "T1", "T");
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // lm.release_rlock("TABLE", "RECORD1", "T1", "T");
    // lm.print_lock_table(); 
    // lm.print_gran_tree();

    // Test Case 17
    // Multiple locks on the same file by the same transaction. It's the exact same lock request twice. 
    // Expected: Only one lock is granted in the table. Both will return true. 
    // bool test_1 = lm.acquire_rlock("TABLE", "RECORD1", "T1", "T");
    // bool test_2 = lm.acquire_rlock("TABLE", "RECORD1", "T1", "T");
    // lm.print_lock_table(); 
    // lm.print_gran_tree();
    // cout << "Transaction: " + to_string(test_1 && test_2) << endl; 
}
*/
