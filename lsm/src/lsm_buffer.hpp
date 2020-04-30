//
// Created by Ismael Alonso on 3/24/20.
//

#ifndef __SRC_LSM_BUFFER_HPP__
#define __SRC_LSM_BUFFER_HPP__

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

using namespace std;

struct SSTable;
struct Record;

class LsmBuffer {
private:
    int tableCount; // Max allowed tables in the buffer
    int recordsPerTable;
    Record* records;

    // Memtable bookkeeping
    std::string memtables[2];
    int lastMemtableUsed;
    std::unordered_map<std::string, int> memtableOccupancy;

    int tail;
    std::vector<std::shared_ptr<SSTable>> tables;
    std::unordered_map<std::string, int> tableMap;

    int writeNonLruMemtable();


public:
    LsmBuffer(int blocks, int blocksPerTable);
    ~LsmBuffer();

    void pushRecord(string table, Record record);

    // Needy Nick's API
    shared_ptr<SSTable> getUnusedCompactionTable();
    shared_ptr<SSTable> getOrLoadAndGet(string table_id);

    shared_ptr<SSTable> getOrLoadAndGetMemtable(std::string table);
};

void createBuffer(int blocks, int blocksPerTable);

#endif // __SRC_LSM_BUFFER_HPP__
