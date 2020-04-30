#include "lsm.hpp"
#include "lsm_buffer.hpp"
#include <fstream>
#include <utility>
#include <regex>
#include "structs.hpp"
#include <numeric>
#include <memory>
#include "stats.hpp"
#include "lock_manager.hpp"
# include <iostream>

vector<string> tokenize(string str);

struct Lock{
    string lock_type; 
    string table;
    string record_id;
    string area_code;
};

struct ExecutingItem { 
   vector<Lock *> acquiredLocks;   // List of acquired Locks.
   string waitingOperation; // Operation that the acquire lock failed at. 
   vector<string> delayedOperation; // List of Delayed operations.
   std::chrono::time_point<std::chrono::steady_clock> t_start;
   int execution_mode; 
}; 

class PTA {
    Statistics stats;
    LockManager lm; 
    unordered_map<int, ExecutingItem> transaction_list;
    vector<string> aborted_ts;

public:
    explicit PTA()
    {
        stats = Statistics();
        lm = LockManager();
    }

    Lock * new_lock(string lock_type, string table){
        Lock *temp = new Lock;
        temp->lock_type = lock_type; 
        temp->table = table; 
        return temp; 
    }

    ExecutingItem new_item(int exec){
        ExecutingItem temp = ExecutingItem(); 
        temp.execution_mode = exec; 
        temp.t_start = std::chrono::steady_clock::now();
        return temp; 
    }

    void print_list(){
        vector<Lock *> locks; 
        Lock * lock; 
        for(auto const& item : transaction_list){
            cout << "ID: " + to_string(item.first) << endl; 
            ExecutingItem temp = item.second; 
            cout << "Execution: " << (temp.execution_mode == EMODE_PROCESS ? "Process" : "Transaction") << endl; 
            cout << "Waiting Op: " + temp.waitingOperation << endl; 

            locks = temp.acquiredLocks; 
            cout << "Acquired Locks" << endl; 
            for(auto it = locks.begin(); it != locks.end(); it++){
                lock = *it; 
                cout << "Lock Type: " + lock->lock_type + " Table: " + lock->table + " Record ID: " + lock->record_id + " Area Code: " + lock->area_code << endl; 
            }

            cout << "Delayed Operations" << endl; 
            for(auto it = temp.delayedOperation.begin(); it != temp.delayedOperation.end(); it++){
                cout << *it << endl; 
            }
        }
        cout << "Done Printing Transaction List" << endl; 
    }

    void run_read(vector<string>& tokens, int txid){
        string op = tokens[0];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, op.size() + 1);
        string table_name = tokens[1];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, table_name.size() + 1);
        string id_s = tokens[2];//cmd.substr(0, cmd.find(" ", 0));
        stringstream ss(id_s);
        int id = 0;
        ss >> id;

        // Call LSM's Read Method
        cout << "LSM Reading..." << "\n";
        if (lsm_ptrs.count(table_name) == 0) {
            cout << "Table not found. Aborting." << endl;
            return;
        }
        bool found;
        Record rec;
        cout << "Finding record with ID: " << id << endl;
        tie(found, rec) = find_record(id, *lsm_ptrs[table_name]);
        if (found)
        {
            cout << "Found record:" << endl;
            cout << "\t" << to_string(rec) << endl;
        }
        else
        {
            cout << "Record not found" << endl;
        }

        // Call Logger's Read method with returned tuple
        logger->log_read(txid, table_name, id);
    }

    void run_write(vector<string>& tokens, int txid) {
        string op = tokens[0];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, op.size() + 1);
        string table_name = tokens[1];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, table_name.size() + 1);
        stringstream ss(tokens[2]);
        uint32_t id;
        ss >> id;

        ss = stringstream(tokens[3]);
        array<char, 16> name;
        memset(&name, '\0', sizeof(array<char, 16>));
        ss >> name;

        array<char, 12> phone;
        memset(&phone, '\0', sizeof(array<char, 12>));
        ss = stringstream(tokens[4]);
        ss >> phone;
        Record r = new_record();
        r = { .value = id, .name = name, .phone = phone };
        cout << "Got record :" << to_string(r) << endl;
        // Call LSM's Read Method
        cout << "LSM Writing..." << "\n";
        if (lsm_ptrs.count(table_name) == 0) {
            cout << "New table: " << table_name << endl;
            create_levels(N_LSM_LEVELS, table_name);

            logger->log_create_table(txid, table_name);
        }

        bool found;
        Record beforeRec;
        cout << "Finding record with ID: " << id << endl;
        tie(found, beforeRec) = find_record(id, *lsm_ptrs[table_name]);

        lsmBuffer->pushRecord(table_name, r);
        if (found) {
            logger->log_write(txid, table_name, r, beforeRec);
        } else {
            logger->log_write(txid, table_name, r);
        }
    }

    void run_mread(vector<string>& tokens, int txid){
        string op = tokens[0];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, op.size() + 1);
        string table_name = tokens[1];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, table_name.size() + 1);
        string raw = tokens[2];//cmd.substr(0, cmd.find(" ", 0));
        stringstream ss(raw);
        uint32_t area_code = 0;
        ss >> area_code;

        // Call LSM's Read Method
        cout << "LSM MReading..." << "\n";
        if (lsm_ptrs.count(table_name) == 0) { return; }
        set<Record> results = mread(area_code, *lsm_ptrs[table_name]);
        cout << "Got these:" << endl;
        for (const auto& r : results)
        {
            cout << "\t" << to_string(r) << endl;
        }
        // Call Logger's MRead method with returned tuple
        logger->log_read(txid, table_name, area_code);
    }

    void run_erase(vector<string>& tokens, int txid){
        string op = tokens[0];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, op.size() + 1);
        string table_name = tokens[1];//cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, table_name.size() + 1);
        string id_str = tokens[2]; //cmd.substr(0, cmd.size());
        stringstream ss(id_str);
        uint32_t id = 0;
        ss >> id;
        Record r = new_record();
        r.id = id;
        r.deleted = 1;
        // Call LSM's Erase Method
        cout << "LSM Erasing..." << "\n";
        if (lsm_ptrs.count(table_name) == 0) { return; }

        bool found;
        Record beforeRec;
        cout << "Finding record with ID: " << id << endl;
        tie(found, beforeRec) = find_record(id, *lsm_ptrs[table_name]);

        lsmBuffer->pushRecord(table_name, r);
        // Call Logger's Erase method with erased tuple
        logger->log_erase(txid, table_name, beforeRec);
    }

    void run_delete(vector<string>& tokens, int txid){
        string op = tokens[0]; // cmd.substr(0, cmd.find(" ", 0));
        //cmd.erase(0, op.size() + 1);
        string table_name = tokens[1]; //cmd.substr(0, cmd.size());

        recoveryManager->recordDelete(txid, table_name);

        // Call LSM's Erase Method
        cout << "LSM Deleting..." << "\n";
        auto itr = lsm_ptrs.find(table_name);
        if (itr != lsm_ptrs.end()) {
            lsm_ptrs.erase(itr);
            // Call Logger's Delete method with deleted table
            logger->log_delete_table(txid, table_name);
        }
    }

    bool 
    repeat_loc(int txid, Lock * lock){
        ExecutingItem item = transaction_list.at(txid);
        vector<Lock *> locks = item.acquiredLocks; 
        Lock * lock_held;
        for(auto it = locks.begin(); it != locks.end(); it++){
            lock_held = *it; 
            if(lock_held->record_id == lock->record_id && lock_held->area_code == lock->area_code && lock_held->lock_type == lock->lock_type && lock_held->table == lock->table)
                return true; 
        }

        return false; 
    }

    bool 
    acquire_lock(vector<string>& tokens, int txid, string cmd){   
        // Read
        if (tokens[0] == "R") {
            ExecutingItem item = transaction_list.at(txid);
            string exec = item.execution_mode == EMODE_TRANSACTION ? "T" : "P"; 
            string op = tokens[0];
            string table_name = tokens[1];
            string id_s = tokens[2];

            // If Transaction is delayed it's waiting operation will not be empty. 
            if(item.waitingOperation.compare("") != 0){
                item.delayedOperation.push_back(cmd); 
                transaction_list[txid] = item; 
                return false; 
            }

            if(lm.acquire_rlock(table_name, id_s, to_string(txid), exec)){
                Lock * lock = new_lock("READ", table_name); 
                lock->record_id = id_s; 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                    transaction_list[txid] = item; 
                }
                return true; 
            }
            item.waitingOperation = cmd; 
            transaction_list[txid] = item; 
            return false; 
        }
        else if (tokens[0] == "W") {// Write
            ExecutingItem item = transaction_list.at(txid);
            string exec = item.execution_mode == EMODE_TRANSACTION ? "T" : "P"; 
            string op = tokens[0];
            string table_name = tokens[1];
            stringstream ss(tokens[2]);
            uint32_t id;
            ss >> id;

            // If Transaction is delayed it's waiting operation will not be empty. 
            if(item.waitingOperation.compare("") != 0){
                item.delayedOperation.push_back(cmd); 
                transaction_list[txid] = item; 
                return false; 
            }

            if(lm.acquire_wlock(table_name, to_string(id), to_string(txid), exec)){
                Lock * lock = new_lock("WRITE", table_name); 
                lock->record_id = to_string(id); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                    transaction_list[txid] = item; 
                }
                return true; 
            }
            item.waitingOperation = cmd; 
            transaction_list[txid] = item; 
            return false; 
        }
        else if (tokens[0] == "M") {// MRead
            ExecutingItem item = transaction_list.at(txid);
            string exec = item.execution_mode == EMODE_TRANSACTION ? "T" : "P"; 
            string op = tokens[0];
            string table_name = tokens[1];
            string raw = tokens[2];
            stringstream ss(raw);
            uint32_t area_code = 0;
            ss >> area_code;

            // If Transaction is delayed it's waiting operation will not be empty. 
            if(item.waitingOperation.compare("") != 0){
                item.delayedOperation.push_back(cmd); 
                transaction_list[txid] = item; 
                return false; 
            }

            if(lm.acquire_mlock(table_name, to_string(area_code), to_string(txid), exec)){
                Lock * lock = new_lock("MREAD", table_name); 
                lock->area_code = to_string(area_code); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                    transaction_list[txid] = item; 
                } 
                return true; 
            }
            item.waitingOperation = cmd; 
            transaction_list[txid] = item; 
            return false; 
        }
        else if (tokens[0] == "E") {// Erase
            ExecutingItem item = transaction_list.at(txid);
            string exec = item.execution_mode == EMODE_TRANSACTION ? "T" : "P"; 
            string op = tokens[0];
            string table_name = tokens[1];
            string id_str = tokens[2]; 
            stringstream ss(id_str);
            int id = 0;
            ss >> id;

            // If Transaction is delayed it's waiting operation will not be empty. 
            if(item.waitingOperation.compare("") != 0){
                item.delayedOperation.push_back(cmd); 
                transaction_list[txid] = item; 
                return false; 
            }

            if(lm.acquire_elock(table_name, to_string(id), to_string(txid), exec)){
                Lock * lock = new_lock("ERASE", table_name); 
                lock->record_id = to_string(id); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                    transaction_list[txid] = item; 
                }
                return true; 
            }
            item.waitingOperation = cmd; 
            transaction_list[txid] = item; 
            return false; 
        }
        else if (tokens[0] == "D") {// Delete
            ExecutingItem item = transaction_list.at(txid);
            string exec = item.execution_mode == EMODE_TRANSACTION ? "T" : "P"; 
            string op = tokens[0]; 
            string table_name = tokens[1]; 

            // If Transaction is delayed it's waiting operation will not be empty. 
            if(item.waitingOperation.compare("") != 0){
                item.delayedOperation.push_back(cmd); 
                transaction_list[txid] = item; 
                return false; 
            }

            if(lm.acquire_dlock(table_name, to_string(txid), exec)){
                Lock * lock = new_lock("DELETE", table_name); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                    transaction_list[txid] = item; 
                }
                return true; 
            }
            item.waitingOperation = cmd; 
            transaction_list[txid] = item; 
            return false; 
        }
        else if (tokens[0] == "B"){
            return true; 
        }
        else if (tokens[0] == "A"){
        	// To avoid having commands in waiting list before finishing the transaction
        	deadLockDetection(tokens);
            ExecutingItem item = transaction_list.at(txid);
            if(item.waitingOperation.compare("") != 0){
                item.delayedOperation.push_back(cmd); 
                transaction_list[txid] = item; 
                return false; 
            }
            return true; 
        }
        else if (tokens[0] == "C"){
        	// To avoid having commands in waiting list before finishing the transaction
        	deadLockDetection(tokens);
            ExecutingItem item = transaction_list.at(txid);
            if(item.waitingOperation.compare("") != 0){
                item.delayedOperation.push_back(cmd); 
                transaction_list[txid] = item; 
                return false; 
            }
            return true; 
        }
        else{
            cout << "Unrecognized file input" << "\n";
        }
        return false; 
    }

    void print_sch_info(){
        cout << "Lock Table" << endl; 
        lm.print_lock_table();
        cout << "Granularity Tree" << endl; 
        lm.print_gran_tree(); 
        cout << "Execution List" << endl; 
        print_list(); 
    }

    void update_abort_counts(string type, int num){
    	if (type == "R")
		{
    		stats.total_read_count += num;
			stats.abort_read_count += num;
		}
		else if (type == "W")
		{
			stats.total_write_count += num;
			stats.abort_write_count += num;
		}
		else if (type == "M")
		{
			stats.total_mread_count += num;
			stats.abort_mread_count += num;
		}
		else if (type == "E")
		{
			stats.total_erase_count += num;
			stats.abort_erase_count += num;
		}
		else if (type == "D")
		{
			stats.total_delete_count += num;
			stats.abort_delete_count += num;
		}
		else if (type == "A")
		{
			// no counter for A
		}
		else if (type == "C")
		{
			// no counter for C
		}
    }

    void execute(string& line, int file_index) {
        stats.total_commands += 1;
        static unordered_map<int, int> ids;
        auto next_id = [] () { static unsigned int id = 0; return id++; };

        vector<string> tokens = tokenize(line);
        int txid = -1;

        if (tokens[0] != "B")
        {
            if (ids.count(file_index) == 0)
            {
                cout << "No txid for line!\n" << endl;
                return;
            }
            txid = ids.at(file_index);
            cout << "TXID: " << txid << "::" << line << endl;
        }

        // Avoid executing commands from an aborted transaction
		if (std::find(aborted_ts.begin(), aborted_ts.end(), to_string(file_index)) != aborted_ts.end()){
			//cout<<"Command to avoid executing: "<<tokens[2]<<endl;
			update_abort_counts(tokens[0], 1);
			return;
		}

        if(acquire_lock(tokens, txid, line)){
            // Read
            if (tokens[0] == "R")
            {
                stats.total_read_count += 1;
                run_read(tokens, txid);
                if(transaction_list[txid].execution_mode == EMODE_PROCESS)
                    release_process_locks(txid); 
            }
            else if (tokens[0] == "W")
            {// Write
                stats.total_write_count += 1;
                run_write(tokens, txid);
                if(transaction_list[txid].execution_mode == EMODE_PROCESS)
                    release_process_locks(txid); 
            }
            else if (tokens[0] == "M")
            {// MRead
                stats.total_mread_count += 1;
                run_mread(tokens, txid);
                if(transaction_list[txid].execution_mode == EMODE_PROCESS)
                    release_process_locks(txid); 
            }
            else if (tokens[0] == "E")
            {// Erase
                stats.total_erase_count += 1;
                run_erase(tokens, txid);
                if(transaction_list[txid].execution_mode == EMODE_PROCESS)
                    release_process_locks(txid); 
            }
            else if (tokens[0] == "D")
            {// Delete
                stats.total_delete_count += 1;
                run_delete(tokens, txid);
                if(transaction_list[txid].execution_mode == EMODE_PROCESS)
                    release_process_locks(txid); 
            }
            else if (tokens[0] == "B")
            {
                txid = next_id();
                ids[file_index] = txid;
                cout << "BEGIN Adding txid " << ids.at(file_index) << " to file index " << file_index << endl;
                run_begin(tokens, txid);
            }
            else if (tokens[0] == "A")
            {
                cout << "ABORT Erasing txid " << ids.at(file_index) << " from file index " << file_index << endl;
                ids.erase(file_index);
                run_abort(tokens, txid);
            }
            else if (tokens[0] == "C")
            {
                cout << "COMMIT txid " << ids.at(file_index) << " from file index " << file_index << endl;
                ids.erase(file_index);
                run_commit(tokens, txid);
            }
            else
            {
                cout << "Unrecognized file input" << "\n";
            }
        }

        // Check if the run method is process and if it ran release it's lock.

        // run deadlock detection after every 20 commands processed
        if (stats.total_commands % 20 == 0){
        	cout<<"Running DeadLock Detection after command "<<stats.total_commands<<"processed"<<endl;
        	deadLockDetection(tokens);
        }
    }

    void deadLockDetection(vector<string> tokens){
    	vector<string> t_ids = lm.deadLock_detection();
		if (t_ids.size() == 0){
			cout<<"No deadlocks detected, resume normally"<<endl;
		} else {
			cout<<"DeadLock detected, aborting the following transactions:"<<endl;
			vector<string>::iterator i_t = t_ids.begin();
			for(auto i : t_ids) {
				cout << i <<endl;
				aborted_ts.push_back(i);
				//print_sch_info();
				run_abort(tokens, stoi(i));
				//print_sch_info();
			}
		}
    }


    void
    release_process_locks(int txid){
        ExecutingItem item = transaction_list.at(txid);
        // Process should always only have one lock. 
        if(!item.acquiredLocks.empty() ){
            Lock *lock = item.acquiredLocks[0]; 
            string release_result = "";
            if(lock->lock_type.compare("READ") == 0) {
                release_result = lm.release_rlock(lock->table, lock->record_id, to_string(txid), "P"); 
            } else if (lock->lock_type.compare("MREAD") == 0) {
                release_result = lm.release_mlock(lock->table, lock->area_code, to_string(txid), "P"); 
            } else if (lock->lock_type.compare("WRITE") == 0) {
                release_result = lm.release_wlock(lock->table, lock->record_id, to_string(txid), "P"); 
            } else if (lock->lock_type.compare("ERASE") == 0) {
                release_result = lm.release_elock(lock->table, lock->record_id, to_string(txid), "P"); 
            } else if (lock->lock_type.compare("DELETE") == 0) {
                release_result = lm.release_dlock(lock->table, to_string(txid), "P"); 
            }
            item.acquiredLocks.erase(item.acquiredLocks.begin());  
            transaction_list[txid] = item;
            cout << "Process Release Result: " + release_result + " Expected: NONE " << endl; 
            handle_release_result(release_result);
        } else {
            cout << "Process Holds no locks. No need to release." << endl;
        }
        
        
    }

    void
    run_start_process(vector<string>& tokens, int txid)
    {
        /* Implement me! */
        transaction_list.insert({ txid, new_item(EMODE_PROCESS) });
    }

    void
    run_start_transaction(vector<string>& tokens, int txid)
    {
        /* Implement me! */
        recoveryManager->start(txid);
        logger->log_transaction_start(txid);
        transaction_list.insert({ txid, new_item(EMODE_TRANSACTION) });
    }

    void run(vector<string>& tokens, int txid){
        if (tokens[0] == "R"){
            stats.total_read_count += 1;
            run_read(tokens, txid);
			if(transaction_list.at(txid).execution_mode == EMODE_PROCESS)
                release_process_locks(txid);
        }
        else if (tokens[0] == "W"){// Write
            stats.total_write_count += 1;
            run_write(tokens, txid);
            if(transaction_list.at(txid).execution_mode == EMODE_PROCESS)
                release_process_locks(txid);
        }
        else if (tokens[0] == "M"){// MRead
            stats.total_mread_count += 1;
            run_mread(tokens, txid);
			if(transaction_list.at(txid).execution_mode == EMODE_PROCESS)
                release_process_locks(txid);
        }
        else if (tokens[0] == "E"){// Erase
            stats.total_erase_count += 1;
            run_erase(tokens, txid);
            if(transaction_list.at(txid).execution_mode == EMODE_PROCESS)
                release_process_locks(txid);
        }
        else if (tokens[0] == "D"){// Delete
            stats.total_delete_count += 1;
            run_delete(tokens, txid);
            if(transaction_list.at(txid).execution_mode == EMODE_PROCESS)
                release_process_locks(txid);
        } 
        else if (tokens[0] == "C") {
            cout << "Running Delayed Commit for ID: " + to_string(txid) << endl; 
            if(transaction_list.at(txid).execution_mode == EMODE_PROCESS)
                release_process_locks(txid); 
            run_commit(tokens, txid); 
        }
        else if (tokens[0] == "A") {
            cout << "Running Delayed Abort for ID: " + to_string(txid)  << endl;
            if(transaction_list.at(txid).execution_mode == EMODE_PROCESS)
                release_process_locks(txid); 
            run_abort(tokens, txid); 
        }
        else{
            cout << "Unrecognized file input" << "\n";
        }
    }

    // If release result doesn't equal NONE then a new transaction can run. 
    // Find that transaction in the list and execute the waiting operation. 
    void
    handle_release_result(string release_result){
        cout << "Handle result: " + release_result << endl;
        
        if(release_result.compare("NONE") != 0){
            int txid = stoi(release_result); 
            ExecutingItem item = transaction_list.at(txid);
            string waiting_op = item.waitingOperation; 
            vector<string> waiting_tokens = tokenize(waiting_op);
            cout << "WAITING OP: " + waiting_op << endl; 
            
            // Rerun Op but don't have to call the lock manager's acquire lock since that lock is granted already. 
            // Add the lock to the acquired locks list for this transaction and then run the op.
            item.waitingOperation = ""; 
            string table_name = waiting_tokens[1];
            if (waiting_tokens[0] == "R"){
                stringstream ss(waiting_tokens[2]);
                uint32_t id;
                ss >> id;
                Lock * lock = new_lock("READ", table_name); 
                lock->record_id = to_string(id); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                }
            }
            else if (waiting_tokens[0] == "W"){// Write
                stringstream ss(waiting_tokens[2]);
                uint32_t id;
                ss >> id;
                Lock * lock = new_lock("WRITE", table_name); 
                lock->record_id = to_string(id); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                }
            }
            else if (waiting_tokens[0] == "M"){// MRead
                string raw = waiting_tokens[2];
                stringstream ss(raw);
                uint32_t area_code = 0;
                ss >> area_code;
                Lock * lock = new_lock("MREAD", table_name); 
                lock->area_code = to_string(area_code); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                }
            }
            else if (waiting_tokens[0] == "E"){// Erase
                string id_str = waiting_tokens[2]; 
                stringstream ss(id_str);
                int id = 0;
                ss >> id;
                Lock * lock = new_lock("ERASE", table_name); 
                lock->record_id = to_string(id); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                }
            }
            else if (waiting_tokens[0] == "D"){// Delete
                Lock * lock = new_lock("DELETE", table_name); 
                if(!repeat_loc(txid, lock)){
                    item.acquiredLocks.push_back(lock); 
                }
            } 
            else{
                cout << "Unrecognized Lock input" << "\n";
            }
            transaction_list[txid] = item; 
            run(waiting_tokens, txid); 
            item = transaction_list.at(txid);
            //item.waitingOperation = "";
            //transaction_list[txid] = item;
            
            // NEED TO LOOP THROUGH DELAYED LIST. ACQUIRE LOCK ON EACH OP AND RUN IF WE CAN ACQUIRE THE LOCK.
            // IF ACQUIRE FAILS AT ANY POINT WE SET THAT OP AS WAITING OP. 
            vector<string> delayedOperations = item.delayedOperation; 
            vector<string> delayed_tokens; 
            for(auto it = delayedOperations.begin(); it != delayedOperations.end();){
                delayed_tokens = tokenize(*it); 
                if(acquire_lock(delayed_tokens, txid, *it)){
                    run(delayed_tokens, txid); 
                    delayedOperations.erase(it); 
                } else {
                    // Updated waiting op to this delayed op
                    item.waitingOperation = delayedOperations[0]; 
                    delayedOperations.erase(it);
                    item.delayedOperation = delayedOperations; 
                    transaction_list[txid] = item; 
                    break; 
                }
            }
        }
    }
	
	void 
    release_locks(int txid, bool abort_flag){
        ExecutingItem item = transaction_list.at(txid);
        std::chrono::time_point<std::chrono::steady_clock> time_now = std::chrono::steady_clock::now(); // to calculate the response-time
        //if (item.waitingOperation.size() > 1)
        //	update_abort_counts(to_string(item.waitingOperation.at(0)), 1);
        
        if(item.execution_mode == EMODE_TRANSACTION){
            vector<Lock *> locks = item.acquiredLocks;
            Lock * lock;
            string release_result;
            // Loop through each of the acquired locks and release them in reverse order. 
            for (auto it = locks.rbegin(); it != locks.rend(); ++it){
                lock = *it;
                if(lock->lock_type.compare("READ") == 0) {
                    release_result = lm.release_rlock(lock->table, lock->record_id, to_string(txid), "T");
                    if (abort_flag) {stats.abort_read_count+=1;}
                    stats.read_time += (time_now - item.t_start);
                } else if (lock->lock_type.compare("MREAD") == 0) {
                    release_result = lm.release_mlock(lock->table, lock->area_code, to_string(txid), "T");
                    if (abort_flag) {stats.abort_mread_count+=1;}
                    stats.mread_time += (time_now - item.t_start);
                } else if (lock->lock_type.compare("WRITE") == 0) {
                    release_result = lm.release_wlock(lock->table, lock->record_id, to_string(txid), "T");
                    if (abort_flag) {stats.abort_write_count+=1;}
                    stats.write_time += (time_now - item.t_start);
                } else if (lock->lock_type.compare("ERASE") == 0) {
                    release_result = lm.release_elock(lock->table, lock->record_id, to_string(txid), "T");
                    if (abort_flag) {stats.abort_erase_count+=1;}
                    stats.erase_time += (time_now - item.t_start);
                } else if (lock->lock_type.compare("DELETE") == 0) {
                    release_result = lm.release_dlock(lock->table, to_string(txid), "T");
                    if (abort_flag) {stats.abort_delete_count+=1;}
                    stats.delete_time += (time_now - item.t_start);
                }
                handle_release_result(release_result); 
                release_result = "";
            }
            item.acquiredLocks.clear(); 
            transaction_list[txid] = item; 

        }
        transaction_list.erase(txid); 
        // For a Process we don't have anything to release.
    }

	void release_waiting_locks(int txid){
		ExecutingItem item = transaction_list.at(txid);
		string waiting_op = item.waitingOperation;
		vector<string> waiting_tokens = tokenize(waiting_op);
		cout << "*** NEW WAITING OP: " + waiting_op << endl;

		string release_result = "";
		string exec = item.execution_mode == EMODE_TRANSACTION ? "T" : "P";
		item.waitingOperation = "";
		string table_name = waiting_tokens[1];
		update_abort_counts(waiting_tokens[0], 1); // count the waiting op
		// count all delayed ops
		for (auto op : item.delayedOperation){
			update_abort_counts(tokenize(op)[0], 1);
		}
		if (waiting_tokens[0] == "R"){
			stringstream ss(waiting_tokens[2]);
			uint32_t id;
			ss >> id;
			release_result = lm.release_rlock(table_name, to_string(id), to_string(txid), exec);
		}
		else if (waiting_tokens[0] == "W"){// Write
			stringstream ss(waiting_tokens[2]);
			uint32_t id;
			ss >> id;
			release_result = lm.release_wlock(table_name, to_string(id), to_string(txid), exec);
		}
		else if (waiting_tokens[0] == "M"){// MRead
			string raw = waiting_tokens[2];
			stringstream ss(raw);
			uint32_t area_code = 0;
			ss >> area_code;
			release_result = lm.release_mlock(table_name, to_string(area_code), to_string(txid), exec);
		}
		else if (waiting_tokens[0] == "E"){// Erase
			string id_str = waiting_tokens[2];
			stringstream ss(id_str);
			int id = 0;
			ss >> id;
			release_result = lm.release_elock(table_name, to_string(id), to_string(txid), exec);
		}
		else if (waiting_tokens[0] == "D"){// Delete
			release_result = lm.release_dlock(table_name, to_string(txid), exec);
		}
		else{
			cout << "Unrecognized Lock input" << "\n";
		}
	}

    void
    run_abort(vector<string>& tokens, int txid)
    {
        /* Implement me! */
        // Call Abort at DM first!
        recoveryManager->abort(txid);
        logger->log_transaction_abort(txid);
        if (transaction_list.at(txid).waitingOperation != "")
    		release_waiting_locks(txid);
        release_locks(txid, true);
    }

    void
    run_commit(vector<string>& tokens, int txid)
    {
        /* Implement me! */
        recoveryManager->commit(txid);
        logger->log_transaction_commit(txid);
        release_locks(txid, false);
    }

    enum {
        EMODE_PROCESS = 0,
        EMODE_TRANSACTION = 1
    };

    void
    run_begin(vector<string>& tokens, int txid)
    {
        switch(stoi(tokens[1])) {
        case EMODE_PROCESS: {
            run_start_process(tokens, txid);
            break;
        }
        case EMODE_TRANSACTION: {
            run_start_transaction(tokens, txid);
            break;
        }
        default:
            cout << "Unrecognized EMode." << endl;
            break;
        }
    }

    void print_latency(){
    	cout<<"\n+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-\n";
    	cout<<"Operations Latency :"<<endl;

    	cout<<"Read Latency: "<<stats.read_time/std::chrono::milliseconds(1)<<" mSeconds"<<endl;

    	cout<<"Write Latency: "<<stats.write_time/std::chrono::milliseconds(1)<<" mSeconds"<<endl;

    	cout<<"MRead Latency: "<<stats.mread_time/std::chrono::milliseconds(1)<<" mSeconds"<<endl;

    	cout<<"Erase Latency: "<<stats.erase_time/std::chrono::milliseconds(1)<<" mSeconds"<<endl;

    	cout<<"Delete Latency: "<<stats.delete_time/std::chrono::milliseconds(1)<<" mSeconds"<<endl;

    	cout<<"Total Latency: "<<(stats.read_time + stats.write_time + stats.mread_time + stats.erase_time + stats.delete_time)/std::chrono::milliseconds(1)<<" mSeconds"<<endl;

		cout<<"\n+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-\n";

    }

void print_stats(){
    	int total_aborts = stats.abort_read_count + stats.abort_write_count + stats.abort_mread_count + stats.abort_erase_count + stats.abort_delete_count;
    	int total_successful = stats.total_commands - total_aborts;
    	cout<<"\n**********************************************************\n";
    	float perc = 0;
    	if (stats.total_commands != 0) {perc = ((float)total_successful/stats.total_commands) * 100;}
    	cout<<"Statistics: "<<endl;
    	cout<<"Commands: \nTotal: "<<stats.total_commands<<"\nCommitted: "<<total_successful;
    	cout<<"\nAborted: "<<total_aborts<<"\nPercantage of Success: "<<perc<<"%"<<endl<<endl;

    	perc = 0;
    	if (stats.total_read_count != 0) {perc = (1 - ((float)stats.abort_read_count/stats.total_read_count)) * 100;}
    	cout<<"Reads: \nTotal: "<<stats.total_read_count<<"\nCommitted: "<<(stats.total_read_count - stats.abort_read_count);
    	cout<<"\nAborted: "<<stats.abort_read_count<<"\nPercantage of Success: "<<perc<<"%"<<endl<<endl;

    	perc = 0;
    	if (stats.total_write_count != 0) {perc = (1 - ((float)stats.abort_write_count/stats.total_write_count)) * 100;}
    	cout<<"Writes: \nTotal: "<<stats.total_write_count<<"\nCommitted: "<<(stats.total_write_count - stats.abort_write_count);
    	cout<<"\nAborted: "<<stats.abort_write_count<<"\nPercantage of Success: "<<perc<<"%"<<endl<<endl;

    	perc = 0;
    	if (stats.total_mread_count != 0) {perc = (1 - ((float)stats.abort_mread_count/stats.total_mread_count)) * 100;}
    	cout<<"MReads: \nTotal: "<<stats.total_mread_count<<"\nCommitted: "<<(stats.total_mread_count - stats.abort_mread_count);
    	cout<<"\nAborted: "<<stats.abort_mread_count<<"\nPercantage of Success: "<<perc<<"%"<<endl<<endl;

    	perc = 0;
    	if (stats.total_erase_count != 0) {perc = (1 - ((float)stats.abort_erase_count/stats.total_erase_count)) * 100;}
    	cout<<"Erase: \nTotal: "<<stats.total_erase_count<<"\nCommitted: "<<(stats.total_erase_count - stats.abort_erase_count);
    	cout<<"\nAborted: "<<stats.abort_erase_count<<"\nPercantage of Success: "<<perc<<"%"<<endl<<endl;

    	perc = 0;
    	if (stats.total_delete_count != 0) {perc = (1 - ((float)stats.abort_delete_count/stats.total_delete_count)) * 100;}
    	cout<<"Deletes: \nTotal: "<<stats.total_delete_count<<"\nCommitted: "<<(stats.total_delete_count - stats.abort_delete_count);
    	cout<<"\nAborted: "<<stats.abort_delete_count<<"\nPercantage of Success: "<<perc<<"%"<<endl<<endl;

    	cout<<"\n**********************************************************\n";

    }
};

static shared_ptr<PTA> pta;

/*
int main(int argc, char *argv[]) {

    for (int i = 0; i < argc; ++i) {
        cout << argv[i] << "\n";
    }

    string input_file = argv[1];
    ifstream input(input_file);
    if (!input.good()) {
        cout << "Entered File: " << input_file << " Not found." << "\n";
        exit(1);
    }

    string impl = argv[2];
    cout << impl << endl;
    if (!(impl == "SEQ" || impl == "LSM")) {
        cout << "Unrecogized input for Implementation. Expected SEQ or LSM. Received: " << impl << "\n";
        exit(1);
    }

    if (impl == "LSM")
    {
        cout << "Configuring LSM..." << endl;
        createBuffer(10, N_BLOCKS);
        SSTable::configure(N_BLOCKS);
    }

    PTA myPTA = PTA(impl);
    string line;
    while (getline(input, line)) {
        myPTA.execute(line);
    }
}
*/

vector<string>
tokenize(string str)
{
    static regex reg("[\\(\\),\\s]+?");
    sregex_token_iterator itr(str.begin(), str.end(), reg, -1);
    sregex_token_iterator end;
    vector<string> results(itr, end);
    vector<string> non_null;
    for (const auto& r : results)
    {
        if (r != "") { non_null.push_back(r); }
    }
    return non_null;
}

void
random_dispatch(vector<ifstream>& input_files);

void
roundrobin_dispatch(vector<ifstream>& input_files);

void
random_dispatch(vector<ifstream>& input_files)
{
    unsigned int index;
    string line;
    vector<tuple<vector<ifstream>::iterator, int>> ifs;
    auto itr = input_files.begin();
    for (int i = 0; i < input_files.size(); i++, itr++)
    {
        ifs.push_back({itr, i});
    }
    do
    {
        index = rand() % ifs.size();
        if (!get<0>(ifs[index])->eof())
        {
            getline(*get<0>(ifs[index]), line);
            if (line != "")
            {
                pta->execute(line, get<1>(ifs[index]));
            }
        }
        else
        {
            auto itr = ifs.begin();
            advance(itr, index);
            ifs.erase(itr);
        }
    } while (ifs.size() > 0);
}

void
roundrobin_dispatch(vector<ifstream>& input_files)
{
    string line;
    do
    {
        for (int i = 0; i < input_files.size(); i++)
        {
            auto& ifstream = input_files[i];
            if (!ifstream.eof())
            {
                getline(ifstream, line);
                if (line != "")
                {
                    pta->execute(line, i);
                }
            }
        }
    } while (accumulate(input_files.begin(), input_files.end(), false,
                        [] (bool accum, const auto& ifstream)
                        { return accum || !ifstream.eof(); }));
}

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        cout << "Usage: ./myPTA <dispatch> <bs> <seed> ...               \n\n"
            "<dispatch>\tone of \"roundrobin\" or \"random\"               \n"
            "<bs>\t\tan integer buffer size                                \n"
            "<seed>\t\tan integer to seed the random number generator with \n"
            "...\t\tinput file names" << endl;
        return -1;
    }
    int tok_idx = 1;
    string linefeed_type = string{argv[tok_idx++]};
    if (linefeed_type != "random" && linefeed_type != "roundrobin")
    {
        cout << "Invalid line dispatch type." << endl;
        return -1;
    }
    int databuffer_size = stoi(argv[tok_idx++]);
    int random_seed = stoi(argv[tok_idx++]);
    srand(random_seed); /* Seed the random number generator */
    vector<ifstream> ifs;
    pta = make_shared<PTA>();
    createBuffer(databuffer_size, 10);
    logger = make_shared<Logger>();
    recoveryManager = make_shared<RecoveryManager>();
    for ( ; tok_idx<argc; tok_idx++) {
        ifs.push_back(ifstream{argv[tok_idx]});
    }
    if (linefeed_type == "random")
    {
        random_dispatch(ifs);
    }
    else
    {
        roundrobin_dispatch(ifs);
    }

    // print / log statistics
    pta->print_stats();

    pta->print_latency();

    return 0;
}
