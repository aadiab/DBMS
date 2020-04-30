#include "lock_table.hpp"

Transaction * newTransaction(string transactionID) {
    Transaction * temp = new Transaction;
    temp->transactionID = transactionID;
    temp->next = NULL; 
    temp->recid = "";
    return temp; 
}

LockTableEntry * newLockEntry(string data, string transaction){
    LockTableEntry *entry = new LockTableEntry; 
    entry->dataElement = data; 
    entry->next = NULL; 
    Transaction* new_node = newTransaction(transaction); 
    entry->transactionElement = new_node;
    return entry; 
}

LockTable::LockTable() {
    lt = new LockTableEntry*[NUM_BUCKETS]; 
    for(int i = 0 ; i < NUM_BUCKETS; i++){
        lt[i] = NULL; 
    }
}

int LockTable::HashFunc(string data) {
    return hash<string>{}(data) % NUM_BUCKETS;
}

LockReleaseResult * newLockRelease(){
    LockReleaseResult * temp = new LockReleaseResult; 
    temp->new_lock_status = "NONE"; 
    temp->new_lock_type = ""; 
    temp->new_transaction = ""; 
    temp->signal = "NONE"; 
    temp->recid = ""; 
    return temp; 
}

// Releases the current lock on the data element. 
// Returns the next Transaction that was waiting on this lock so the scheduler can start that transaction. 
// Returns NONE if no other transaction are holding a lock on this data element. 
// Returns HEADCHANGE if the next transaction's lock has already been GRANTED. (Happens if it's a read).
// Returns LOCKED if the transaction removed was not at the head.  
LockReleaseResult * LockTable::releaseLock(string data, string transaction, string recid){
    LockTableEntry * lte = lt[HashFunc(data)];
    LockTableEntry * tmp; 
    if(lte->dataElement.compare(data) != 0){
        while(lte->next != NULL && lte->dataElement.compare(data) != 0){
            lte = lte->next; 
        }
    }
    if(lte->dataElement.compare(data) != 0){
        cout << "DATA NOT FOUND" << "\n";
        return newLockRelease();
    }
    
    tmp = lte;
    Transaction * t = tmp->transactionElement; 
    cout << "release lock transaction: " + transaction << "\n"; 

    // if transaction was found at the head of transaction list. 
    if(t->transactionID.compare(transaction) == 0 && t->recid.compare(recid) == 0){
        if(t->next == NULL){
            
            // Deleting the only transaction with a lock on this data element. 
            // Since no other transactions in the Linked list we return NONE.
            // if This data element is the only one in the bucket. 
            LockTableEntry * rowHead = lt[HashFunc(data)]; 
            if(rowHead->next == NULL && rowHead->dataElement.compare(data) == 0){
                lt[HashFunc(data)] = NULL;
                return newLockRelease(); 
            }

            // If this data element is not the only one in the bucket. 
            LockTableEntry * prev = lt[HashFunc(data)]; 
            tmp = prev; 
            while(tmp->next != NULL && tmp->dataElement.compare(data) != 0){
                prev = tmp;
                tmp = tmp->next; 
            }
            prev->next = prev->next->next; 
            cout << "Removing last Lock on a data element: " + data << "\n";
            return newLockRelease(); 
        }

        // Replace the head transaction with the data from the next transaction in the queue. 
        t->transactionID = t->next->transactionID;
        t->lock_type = t->next->lock_type; 
        t->status = t->next->status;
        t->recid = t->next->recid; 

        // Delete the head transaction. 
        t->next = t->next->next;

        // If the new head transaction is already granted then we don't send a transaction to the scheduler. 
        // Send back the lock type that is currently holding the lock. 
        if(t->status.compare("GRANTED") == 0){
            LockReleaseResult * result = newLockRelease();
            result->new_transaction = t->transactionID;
            result->new_lock_status = "GRANTED"; 
            result->new_lock_type = t->lock_type;
            result->recid = t->recid; 
            result->signal = "HEADCHANGE"; 
            return result; 
        } else {
            // Otherwise change the status and send the scheduler the transaction to restart. 
            t->status = "GRANTED"; 
            LockReleaseResult * result = newLockRelease();
            result->new_transaction = t->transactionID;
            result->new_lock_status = "GRANTED"; 
            result->new_lock_type = t->lock_type;
            result->signal = t->transactionID;  
            result->recid = t->recid; 
            return result; 
        }
    } 

    // if transaction is not at the head. 
    // Remove it from the linked list and Don't Grant access to anyone else. 
    // Only when the head transaction is removed, do we grant access to a new transaction!
    Transaction * prev = t; 
    Transaction * curr = t; 
    while(curr->next != NULL && (curr->transactionID.compare(transaction) != 0 || curr->recid.compare(recid) != 0)){
        prev = curr; 
        curr = curr->next; 
    }

    if(curr->next == NULL && curr->transactionID.compare(transaction) == 0 && curr->recid.compare(recid) == 0){
        prev->next = curr->next; 
    } else {
        prev->next = prev->next->next; 
    }
    LockReleaseResult *result = newLockRelease(); 
    result->signal = "LOCKED"; 
    cout << "Lock removed" << "\n";
    return result; 
}

// Returns True if the requesting transaction got the lock. 
// Returns False if the requesting transaction needs to wait to get the lock. 
// Regardless of return, we add the transaction to the queue of transactions waiting. 
bool LockTable::acquireLock(string data, string transaction, string lock_type, string recid){
    LockTableEntry * lte = lt[HashFunc(data)];
    LockTableEntry * tmp; 
    cout << "Acquire Lock data: " + data << " Lock Type: " + lock_type << "\n"; 
    if (lte == NULL) {
        LockTableEntry * new_lte = newLockEntry(data, transaction);
        new_lte->transactionElement->lock_type = lock_type; 
        new_lte->transactionElement->status = "GRANTED"; 
        new_lte->transactionElement->recid = recid;
        lt[HashFunc(data)] = new_lte; 
        return true;
    } else {
        // The head is the desired data block
        if(lte->dataElement.compare(data) == 0){
            return handle_lock_request(transaction, lock_type, lte, recid); 
        } else {
            // head is not the desired data block. Potential collision. 
            cout << "testing collison" << "\n";
            while(lte->next != NULL) {
                if(lte->dataElement.compare(data) == 0){
                    return handle_lock_request(transaction, lock_type, lte, recid);
                }
                lte = lte->next; 
            }
            // if here, we've gone to the end of the bucket and haven't found the data. So add one.
            LockTableEntry *newLock = newLockEntry(data, transaction); 
            newLock->transactionElement->lock_type = lock_type; 
            newLock->transactionElement->status = "GRANTED"; 
            newLock->transactionElement->recid = recid; 
            lte->next = newLock; 
            return true; 
        }
    }

    cout << "ENDED UP HERE IN ACQURIE LOCK. " << "\n";
    return false; 
}

// Returns true if the requesting transaction has the lock already.
bool LockTable::check_repeat_transaction(string lock_type, string transaction, LockTableEntry *lte, string recid){
    Transaction * thead = lte->transactionElement; 
    while(thead->next != NULL && thead->status.compare("GRANTED") == 0){
        if(thead->transactionID.compare(transaction) == 0 && thead->lock_type.compare(lock_type) == 0 && thead->recid.compare(recid) == 0)
            return true; 

        thead = thead->next; 
    }

    if(thead->next == NULL && thead->status.compare("GRANTED") == 0){
        if(thead->transactionID.compare(transaction) == 0 && thead->lock_type.compare(lock_type) == 0 && thead->recid.compare(recid) == 0)
            return true; 
    }

    return false; 
}

bool LockTable::handle_lock_request(string transaction, string lock_type, LockTableEntry *lte, string recid) {
    if(check_repeat_transaction(lock_type, transaction, lte, recid))
        return true;

    // A transaction currently holds a lock on this data element. 
    // If the lock is a read and the incoming lock is a read then we grant lock. 
    // Other wise we return false. 
    // HAVE TO CHECK THE HEAD OF TRANSACTIONS LINKED LIST
    if(recid.compare("") == 0 && lock_type.compare("T-DELETE") != 0 && lock_type.compare("P-DELETE") != 0){
        // Requested lock is on a record.
        return handle_record_lock(lock_type, transaction, lte); 
    } else {
        return handle_file_lock(lock_type, transaction, lte, recid); 
    } 
}

bool LockTable::handle_record_lock(string lock_type, string transaction, LockTableEntry *lte){
    Transaction * thead = lte->transactionElement; 
    Transaction *new_trans = newTransaction(transaction); 
    new_trans->lock_type = lock_type; 
    new_trans->recid = ""; 
    new_trans->status = "GRANTED";

    while(thead->next != NULL && thead->status.compare("GRANTED") == 0){
        if(thead->lock_type.compare("T-ERASE") == 0 || thead->lock_type.compare("T-WRITE") == 0){
            new_trans->status = "WAITING"; 
            break;
        }else if(thead->lock_type.compare("T-READ") == 0 && (lock_type.compare("T-WRITE") == 0 || lock_type.compare("T-ERASE") == 0 || lock_type.compare("P-WRITE") == 0 || lock_type.compare("P-ERASE") == 0)){
            new_trans->status = "WAITING";
            break; 
        }
        thead = thead->next; 
    }

    if(thead->next == NULL && thead->status.compare("GRANTED") == 0){
        if(thead->lock_type.compare("T-ERASE") == 0 || thead->lock_type.compare("T-WRITE") == 0){
            new_trans->status = "WAITING"; 
        }else if(thead->lock_type.compare("T-READ") == 0 && (lock_type.compare("T-WRITE") == 0 || lock_type.compare("T-ERASE") == 0 || lock_type.compare("P-WRITE") == 0 || lock_type.compare("P-ERASE") == 0)){
            new_trans->status = "WAITING";
        }
    }

    if(new_trans->status.compare("WAITING") == 0){
        // All Locks not granted get put at the end of the transaction linked list.
        if(thead->next != NULL){
            while(thead->next!= NULL){
                thead = thead->next; 
            }
        } 
        thead->next = new_trans;
        return false; 
    } else {
        // All Granted locks get put at the beginning of the transaction linked list. 
        new_trans->next = lte->transactionElement; 
        lte->transactionElement = new_trans;
        return true; 
    }
}

bool LockTable::handle_file_lock(string lock_type, string transaction, LockTableEntry *lte, string recid){
    Transaction * thead = lte->transactionElement; 
    Transaction *new_trans = newTransaction(transaction); 
    new_trans->lock_type = lock_type; 
    new_trans->recid = recid; 
    new_trans->status = "GRANTED";

    while(thead->next != NULL && thead->status.compare("GRANTED") == 0){
        if(thead->lock_type.compare("T-DELETE") == 0){
            new_trans->status = "WAITING"; 
            break; 
        } else if(thead->lock_type.compare("T-READ") == 0 && (lock_type.compare("T-DELETE") == 0 || lock_type.compare("P-DELETE") == 0)){
            new_trans->status = "WAITING"; 
            break;
        } else if((thead->lock_type.compare("T-ERASE") == 0 || thead->lock_type.compare("T-WRITE") == 0) 
            && (lock_type.compare("T-DELETE") == 0 || lock_type.compare("P-DELETE") == 0 || lock_type.compare("T-MREAD") == 0 || lock_type.compare("P-MREAD") == 0)){
            new_trans->status = "WAITING"; 
            break;
        } else if(thead->lock_type.compare("T-MREAD") == 0 && (lock_type.compare("T-DELETE") == 0 || lock_type.compare("T-WRITE") == 0 || lock_type.compare("T-ERASE") == 0 || 
        lock_type.compare("P-DELETE") == 0 || lock_type.compare("P-WRITE") == 0 || lock_type.compare("P-ERASE") == 0)){
            new_trans->status = "WAITING"; 
            break;
        }
        thead = thead->next; 
    }

    if(thead->next == NULL && thead->status.compare("GRANTED") == 0){
        if(thead->lock_type.compare("T-DELETE") == 0){
            new_trans->status = "WAITING"; 
        } else if(thead->lock_type.compare("T-READ") == 0 && (lock_type.compare("T-DELETE") == 0 || lock_type.compare("P-DELETE") == 0)){
            new_trans->status = "WAITING"; 
        } else if((thead->lock_type.compare("T-ERASE") == 0 || thead->lock_type.compare("T-WRITE") == 0) 
            && (lock_type.compare("T-DELETE") == 0 || lock_type.compare("P-DELETE") == 0 || lock_type.compare("T-MREAD") == 0 || lock_type.compare("P-MREAD") == 0)){
            new_trans->status = "WAITING"; 
        } else if(thead->lock_type.compare("T-MREAD") == 0 && (lock_type.compare("T-DELETE") == 0 || lock_type.compare("T-WRITE") == 0 || lock_type.compare("T-ERASE") == 0 || 
        lock_type.compare("P-DELETE") == 0 || lock_type.compare("P-WRITE") == 0 || lock_type.compare("P-ERASE") == 0)){
            new_trans->status = "WAITING"; 
        }
    }

    if(new_trans->status.compare("WAITING") == 0){
        // All Locks not granted get put at the end of the transaction linked list.
        if(thead->next != NULL){
            while(thead->next!= NULL){
                thead = thead->next; 
            }
        } 
        thead->next = new_trans;
        return false; 
    } else {
        // All Granted locks get put at the beginning of the transaction linked list. 
        new_trans->next = lte->transactionElement; 
        lte->transactionElement = new_trans;
        return true; 
    }
}

void LockTable::printLTE(LockTableEntry *lte){
    if(lte == NULL){
        cout << "EMPTY" << "\n"; 
        return;
    }

    while(lte->next != NULL){
        cout << "Data: " + lte->dataElement << "\n";
        printTransaction(lte->transactionElement); 
        lte = lte->next; 
    }
    cout << "Data: " + lte->dataElement << "\n";
    printTransaction(lte->transactionElement); 
}

void LockTable::printTable() {
    for(int i = 0; i < NUM_BUCKETS; i++){
        cout << to_string(i) << "\n";
        LockTableEntry *lte = lt[i];
        printLTE(lte);
    }
}

void LockTable::printTransaction(Transaction * head){
    while(head->next != NULL){
        cout << "Transaction id: " + head->transactionID <<  " Lock Type: " + head->lock_type << " Status: " + head->status + " Record ID: " + head->recid << "\n";
        head = head->next; 
    }
    cout << "Transaction id: " + head->transactionID <<  " Lock Type: " + head->lock_type << " Status: " + head->status + " Record ID: " + head->recid << "\n";
}

vector<string> LockTable::deadLock_detection(){
	vector<string> removed_transactions;
	WFG wfg = WFG();

	//printTable();

	// iterate over the whole lt, build WFG
	for(int i = 0; i < NUM_BUCKETS; i++){
		//cout<<"Processing Bucket: "<<i<<endl;
		LockTableEntry * lte = lt[i];

		while (lte != NULL){ // // I am holding a resource mapped to Bucket i
			Transaction * t_list = lte->transactionElement;
			while (t_list != NULL && t_list->status.compare("GRANTED") == 0){
				if (aborted_ts.find(stoi(t_list->transactionID)) == aborted_ts.end()){
					string t_granted = t_list->transactionID;
					//cout<<"Lock is held by: "<<t_granted<<endl;
					Transaction * t_elements = t_list->next;
					while(t_elements != NULL) { // iterate the T list, to find all dependencies
						if (t_elements->status.compare("WAITING") == 0){
							//cout<<"Dependency discovered, adding edge to "<<t_elements->transactionID<<endl;
							if (aborted_ts.find(stoi(t_elements->transactionID)) == aborted_ts.end())
								wfg.addEdge(t_elements->transactionID, t_granted);
						}
						t_elements = t_elements->next;
					}
				}
				t_list = t_list->next;
			}
			lte = lte->next;
		}
	}

	cout<<"WFG before detecting deadlocks: "<<endl;
	wfg.print();

	// Detect Deadlocks based on the created graph
	removed_transactions = wfg.detect_deadlocks();

	for(auto i : removed_transactions) {
		aborted_ts[stoi(i)] = 1;}

	cout<<"WFG after solving possible deadlocks: "<<endl;
	wfg.print();

	return removed_transactions;
}
