//
// Created by Ismael Alonso on 4/12/20.
//

#ifndef SRC_RECOVERY_MANAGER_HPP
#define SRC_RECOVERY_MANAGER_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "lsm.hpp"

using namespace std;

class RecoveryManager {
private:
    unordered_map<int, int> transactionToLine;

    // Map of transaction -> [map of table name -> ordered list of deleted tables]
    unordered_map<int, unordered_map<string, vector<shared_ptr<Leveled_LSM>>>> transactionDeletions;

public:
    void start(int transactionId);
    void recordDelete(int transactionId, std::string tableName);
    void abort(int transactionId);
    void commit(int transactionId);
};

#endif //CS2550_PROJECT_TEAM4_RECOVERY_MANAGER_HPP
