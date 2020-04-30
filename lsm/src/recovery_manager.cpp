//
// Created by Ismael Alonso on 4/18/20.
//

#include <fstream>
#include <utility>
#include "recovery_manager.hpp"
#include "structs.hpp"

using namespace std;

void RecoveryManager::start(int transactionId) {
    cout << "Start of " << transactionId << endl;

    transactionToLine[transactionId] = logger->getLinesWritten();

    unordered_map<string, vector<shared_ptr<Leveled_LSM>>> map;
    transactionDeletions[transactionId] = map;
}

void RecoveryManager::recordDelete(int transactionId, string tableName) {
    cout << "Record delete on " << transactionId << ", " << tableName << endl;
    auto itr = transactionDeletions[transactionId].find(tableName);
    if (itr == transactionDeletions[transactionId].end()) {
        vector<shared_ptr<Leveled_LSM>> deletedTables;
        transactionDeletions[transactionId][tableName] = deletedTables;
    }

    transactionDeletions[transactionId][tableName].push_back(lsm_ptrs[tableName]);
}

void RecoveryManager::abort(int transactionId) {
    int line = transactionToLine[transactionId];
    ifstream in("Log_file.txt");
    for (int i = 0; i < line; i++) {
        in.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    int lastLine = 0;
    string currentLine;
    vector<string> lines;
    while (!in.eof()) {
        getline(in, currentLine);
        // Only care about the transaction
        if (currentLine.find(to_string(transactionId)) == 0) {
            lines.push_back(currentLine);
            lastLine++;
        }
    }

    unordered_map<string, int> restores;
    for (pair<string, vector<shared_ptr<Leveled_LSM>>> element : transactionDeletions[transactionId]) {
        restores[element.first] = element.second.size()-1;
    }

    lastLine--;
    while (lastLine >= 0) {
        string line = lines[lastLine];
        cout << "PROCESSING " << line << endl;
        int pos = line.find(" "); // Transaction id
        line = line.substr(pos+1); // Skip transaction id
        cout << "PROCESSING " << line << endl;
        pos = line.find(" ");
        string operation = line.substr(0, pos); // Extract the operation
        line = line.substr(pos+1);
        
        cout << "PROCESSING " << line << endl;
        cout << "ROLLING BACK " << operation << endl;

        if (operation == "DTBL") {
            // Restore table
            string tableName = line;
            cout << "TABLE NAME " << tableName << endl;
            lsm_ptrs[tableName] = transactionDeletions[transactionId][tableName][restores[tableName]];
            restores[tableName] = restores[tableName]-1;
        } else if (operation == "CTBL") {
            // Delete table
            string tableName = line;
            auto itr = lsm_ptrs.find(tableName);
            if (itr != lsm_ptrs.end()) { lsm_ptrs.erase(itr); }
        } else if (operation == "W") {
            string tableName = line.substr(0, line.find(" "));
            line = line.substr(line.find("(")+1);
            string after = line.substr(0, line.find(")"));
            line = line.substr(line.find(")")+1);
            if (line.find("(") == string::npos) {
                cout << "DELETING " << after << endl;
                int id = stoi(after.substr(0, after.find(",")));
                Record r = new_record();
                r.id = id;
                r.deleted = 1;
                lsmBuffer->pushRecord(tableName, r);
            } else {
                line = line.substr(line.find("(")+1);
                string before = line.substr(0, line.find(")"));
                cout << "RESTORING " << before << endl;
                
                Record r = new_record();
                r.id = stoi(before.substr(0, before.find(",")));
                before = before.substr(before.find(",")+1);
                // Read name
                for (int j = 0; j < 16; j++) {
                    r.name[j] = before.at(j);
                }
                before = before.substr(before.find(",")+1);
                // Read phone
                for (int j = 0; j < 12; j++) {
                    r.phone[j] = before.at(j);
                }
                
                lsmBuffer->pushRecord(tableName, r);
            }
            string removingCurrent = line.substr(line.find(")")+1);
            
        } else if (operation == "E") {
            string tableName = line.substr(0, line.find(" "));
            line = line.substr(line.find("(")+1);
            string before = line.substr(0, line.find(")"));
            cout << "RESTORING " << before << endl;
            
            Record r = new_record();
            r.id = stoi(before.substr(0, before.find(",")));
            before = before.substr(before.find(",")+1);
            // Read name
            for (int j = 0; j < 16; j++) {
                r.name[j] = before.at(j);
            }
            before = before.substr(before.find(",")+1);
            // Read phone
            for (int j = 0; j < 12; j++) {
                r.phone[j] = before.at(j);
            }
            
            lsmBuffer->pushRecord(tableName, r);
        } else {
            // Anything else is communist nonsense and shall be ignored
        }

        lastLine--;
    }
    
    auto itr = transactionDeletions.find(transactionId);
    if (itr != transactionDeletions.end()) {
        transactionDeletions.erase(itr);
    }
}

void RecoveryManager::commit(int transactionId) {
    auto itr = transactionDeletions.find(transactionId);
    if (itr != transactionDeletions.end()) {
        transactionDeletions.erase(itr);
    }
}

