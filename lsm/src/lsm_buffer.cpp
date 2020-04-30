//
// Created by Ismael Alonso on 3/25/20.
//

#include <cstdlib>
#include <fstream>
#include "lsm_buffer.hpp"
#include "lsm.hpp"
#include "structs.hpp"
#include <iostream>

#define E_NOT_ENOUGH_CASH -1

static const int BLOCK_SIZE = 4096;

static const int MEMTABLE_TABLE_COUNT = 2;
static const int COMPACTION_TABLE_OFFSET = MEMTABLE_TABLE_COUNT;
static const int COMPACTION_TABLE_COUNT = 4;
static const int CACHE_OFFSET = MEMTABLE_TABLE_COUNT + COMPACTION_TABLE_COUNT;
// One extra because things need to be read from disk
static const int MIN_VIABLE_TABLE_COUNT = MEMTABLE_TABLE_COUNT + COMPACTION_TABLE_COUNT + 1;

LsmBuffer::LsmBuffer(int blocks, int blocksPerTable) {
    // Calculate the number of records per table
    int recordsPerBlock = BLOCK_SIZE / sizeof(Record);
    this->recordsPerTable = recordsPerBlock * blocksPerTable;

    // Calculate the number of tables to see if we meet requirements
    int totalMemory = blocks * BLOCK_SIZE;
    this->tableCount = totalMemory / (this->recordsPerTable * sizeof(Record));
    if (this->tableCount < MIN_VIABLE_TABLE_COUNT) {
        throw E_NOT_ENOUGH_CASH;
    }

    // Allocate memory for records
    this->records = (Record*)malloc(this->recordsPerTable * this->tableCount * sizeof(Record));

    // Initialize memtable bookkeeping
    this->memtables[0] = "";
    this->memtables[1] = "";
    this->lastMemtableUsed = 1;

    // Create the SSTables
    Record *firstRecord = this->records;
    for (int i = 0; i < this->tableCount; i++) {
        this->tables.push_back(std::make_shared<SSTable>(SSTable(firstRecord)));
        firstRecord += this->recordsPerTable;
    }

    this->tail = CACHE_OFFSET;
}

LsmBuffer::~LsmBuffer() {
    free(this->records);
}

int LsmBuffer::writeNonLruMemtable() {
    int swapIndex = 1-this->lastMemtableUsed; // Cause LRU
    if (this->memtables[swapIndex] != "") { // Only flush if exists
        ofstream fd("memtables/" + memtables[swapIndex]);
        fd << *this->tables[swapIndex];
    }
    return swapIndex;
}

std::shared_ptr<SSTable> LsmBuffer::getOrLoadAndGetMemtable(std::string table) {
    std::shared_ptr<SSTable> memtable;
    auto itr = this->memtableOccupancy.find(table);
    if (itr != tableMap.end()) { // The table already exists
        if (table == this->memtables[0]) {
            memtable = this->tables[0];
            this->lastMemtableUsed = 0;
        } else if (table == this->memtables[1]) {
            memtable = this->tables[1];
            this->lastMemtableUsed = 1;
        } else {
            int swapIndex = writeNonLruMemtable();

            // Load from disk
            this->memtables[swapIndex] = table;
            memtable = tables[swapIndex];
            ifstream fd("memtables/" + table);
            fd >> *memtable;

            this->lastMemtableUsed = swapIndex;
        }
    } else { // Need to create the table
        int swapIndex = writeNonLruMemtable();

        this->memtables[swapIndex] = table;
        memtable = tables[swapIndex];
        memtableOccupancy[table] = 0;

        this->lastMemtableUsed = swapIndex;
    }

    return memtable;
}

void LsmBuffer::pushRecord(std::string table, Record record) {
    cout << "Pushing record: " << to_string(record) << endl;
    std::shared_ptr<SSTable> memtable = getOrLoadAndGetMemtable(table);

    // Calculate the position of the record
    Record* recPtr = memtable->records + memtableOccupancy[table];
    // Insert the record
    cout << "(N: " << memtable->record_ptrs.size() << ") Inserting record: " << to_string(record) << endl;
    *recPtr = record;

    auto it = memtable->record_ptrs.find(recPtr);
    if (it != memtable->record_ptrs.end())
    {
        memtable->record_ptrs.erase(it);
    }

    memtable->record_ptrs.insert(recPtr);
    // Update occupancy
    memtableOccupancy[table] = memtableOccupancy[table] + 1;

    // If we've reached the end of the table, flush
    if (recPtr+1 == this->tables[this->lastMemtableUsed+1]->records) {
        /* This writes too many records */
        flush_to_level(lsm_ptrs[table]->containers[0], this->tables[this->lastMemtableUsed]);
        lsm_ptrs[table]->containers[0].tables.push_back(this->tables[this->lastMemtableUsed]->file_handle);
        check_levels(*lsm_ptrs[table]);
        this->tables[0]->record_ptrs.clear();
        memtableOccupancy[table] = 0;
    }
}

std::shared_ptr<SSTable> LsmBuffer::getUnusedCompactionTable() {
    for (int i = 0; i < COMPACTION_TABLE_COUNT; i++) {
        if (this->tables[COMPACTION_TABLE_OFFSET+i].use_count() == 1) {
            return tables[COMPACTION_TABLE_OFFSET+i];
        }
    }
    return nullptr;
}

std::shared_ptr<SSTable> LsmBuffer::getOrLoadAndGet(std::string tableHandle) {
    auto itr = this->tableMap.find(tableHandle);
    if (itr != tableMap.end()) { // Found!
        return this->tables[itr->second];
    } else { // Not found...
        // Keep track of the old table's handle
        std::string oldTableHandle = (*tables[this->tail]).file_handle;

        // Read the table into memory and update the handle
        std::shared_ptr<SSTable> table = tables[this->tail];
        ifstream fd(tableHandle);
        fd >> *table;
        (*table).file_handle = tableHandle;

        // Update the indexing
        this->tableMap[tableHandle] = this->tail;
        auto itr2 = this->tableMap.find(oldTableHandle);
        if (itr2 != tableMap.end()) {
            tableMap.erase(itr2);
        }

        // And the circle position
        if (++this->tail == this->tableCount) { // ???
            this->tail = CACHE_OFFSET;
        }

        return table;
    }
}

void createBuffer(int blocks, int blocksPerTable) {
    lsmBuffer = std::make_shared<LsmBuffer>(blocks, blocksPerTable);
}
