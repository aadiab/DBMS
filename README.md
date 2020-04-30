# DBMS

Authors: Ahmad Diab, Sai Konduru, Nick Gordon, and Ismael H. Alonso
Date: April - 2020
Title: Database Management System Implementation - C++

Description: Implementation of a transactional row store database that efficiently supports concurrent execution of OLTP (i.e., transactions) workloads. It consists of three components: Data Manager, Scheduler, and Transaction Manager. 

Data Manager: is responsible for managing the data on the primary and secondary storage. 
The Scheduler: is responsible for scheduling operations from transactions. 
The Transaction Manager is responsible for reading in transaction operations. 

Specifications:
* Script files can be executed either as “transactions” or as “processes.” 
* Transactions access files in an atomic mode whereas processes access files in a normal mode. 
* The program supports concurrent access to files by both transactions and processes by employing the Strict Two-Phase Locking protocol
to ensure serializability for transactions.
* The program also uses wait-for graphs for deadlock detection and is free from livelocks. It ensures transactions’ atomicity by adopting an undo recovery strategy where all before images are kept in the buffer and they are written to the file on the disk at commit time. 
* Transactions aborted by the system due to deadlocks should be ignored with no restarts.

List of commands:
 B EMode : Begin (Start) a new transaction (EMode=1) or new process (EMode=0).
 C : Commit (End) current transaction (process).
 A : Abort (End) current transaction (process).
 R table val: Retrieve the record with ID=val in table. If table does not exist, the read is aborted.
 M table val: Retrieve the record(s) which have val as area code in Phone attribute in table. If table
does not exist, the read is aborted.
 W table (t): Write (i.e., Insert or Update) a record t into table. If table does not exist, it is created.
 E table val: Erase/Delete the record with ID=val in table.
 D table: Delete table.


Example of script file to be executed:
-----------------------------------------------
B 1
R X 13
W X (2, Thalia, 412-656-2212)
R X 2
M X 412
C
-----------------------------------------------


