#include "Logger.hpp"
#include <ctime>

// Constructor - Creates the log file is it doesn't exist. 
Logger::Logger() {
    linesWritten = 0;

    log.open("Log_file.txt", ios::trunc); 
    time_t now = time(0);
    char* dt = ctime(&now);
    log << "Log Opened on: " << dt; 
    cout << "Logger Initialized!" << "\n";
}

int Logger::getLinesWritten() {
    return linesWritten;
}

void Logger::log_transaction_start(int transactionId) {
    write_to_file(to_string(transactionId) + " T");
}

void Logger::log_transaction_commit(int transactionId) {
    write_to_file(to_string(transactionId) + " C");
}

void Logger::log_transaction_abort(int transactionId) {
    write_to_file(to_string(transactionId) + " A");
}

void Logger::log_create_table(int transactionId, string tableName) {
    write_to_file(to_string(transactionId) + " CTBL " + tableName);
}

void Logger::log_delete_table(int transactionId, string tableName) {
    write_to_file(to_string(transactionId) + " DTBL " + tableName);
}

void Logger::log_write(int transactionId, string tableName, Record& tuple, Record& beforeTuple) {
    write_to_file(to_string(transactionId) + " W " + tableName + " " + rep(tuple) + " " + rep(beforeTuple));
}

void Logger::log_write(int transactionId, string tableName, Record& tuple) {
    write_to_file(to_string(transactionId) + " W " + tableName + " " + rep(tuple));
}

void Logger::log_read(int transactionId, string tableName, int id) {
    write_to_file(to_string(transactionId) + " R " + tableName + " " + to_string(id));
}

void Logger::log_read(int transactionId, string tableName, string areaCode) {
    write_to_file(to_string(transactionId) + " R " + tableName + " " + areaCode);
}

void Logger::log_erase(int transactionId, string tableName, Record& tuple) {
    write_to_file(to_string(transactionId) + " E " + tableName + " " + rep(tuple));
}

// Add new line to the message and writes to file.
// This method is used by all others to write. 
void Logger::write_to_file(string msg) {
    linesWritten++;
    log << msg << endl;
}

