#ifndef CS2550_PROJECT_TEAM4_LOGGER_HPP
#define CS2550_PROJECT_TEAM4_LOGGER_HPP

#include <iostream>
#include <fstream>

#include "lsm.hpp"

using namespace std;

class Logger {
private:
    int linesWritten;
    ofstream log;

    void write_to_file(string msg);

public:
    Logger();

    int getLinesWritten();

    // Transactions
    void log_transaction_start(int transactionId);
    void log_transaction_commit(int transactionId);
    void log_transaction_abort(int transactionId);

    // Tables
    void log_create_table(int transactionId, string tableName);
    void log_delete_table(int transactionId, string tableName);

    // Entries
    void log_write(int transactionId, string tableName, Record& beforeTuple);
    void log_write(int transactionId, string tableName, Record& tuple, Record& beforeTuple);
    void log_read(int transactionId, string tableName, int id);
    void log_read(int transactionId, string tableName, string areaCode);
    void log_erase(int transactionId, string tableName, Record& tuple); // <- Erase tuple
};

#endif
