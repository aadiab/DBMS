//============================================================================
// Name        : sequential_files
// Author      : Ahmad Diab
// Version     : 1.0
// Copyright   : 
// Description : Phase1 CS2550 Project, Sequential Files implementation.
//============================================================================


# include <iostream>
# include <string>
# include <cstdio>
# include <fstream>
# include <vector>
# include <filesystem>

using namespace std;


class Record {
	public:
		int ID;              // 4-byte
		string clientName;   // 16-byte
		string phone;        // 12-byte

        Record(int ID, string name, string phone);
};

Record::Record(int ID_n, string name, string phone_n){
    ID = ID_n;
    clientName = name;
    phone = phone_n;
}


// Global variables
int block_size = 102; // This is a test, holds 3 records (each 32 bytes) and 3 pointers (each 2byte short)
int number_records_in_block = 3; // this is calculated
int memory_size = 5 * block_size;   // this is a test, I want to hold only 5 slotted-pages from DB
int replacement_size = 5;           // related to memory_size

string *memory = new string[memory_size];
short *replacement_time = new short[replacement_size];
short counter = 1;


int memory_index(){
    int min = -1;
    int index = -1;
    for (int i=0; i<replacement_size; i++){
        int status = replacement_time[i];
        if (status == -1){
            replacement_time[i] = counter;
            counter++;
            return i;
        }
        if (status < min){
            min = status;
            index = i;
        }
    }
    // Buffer is full, decrease LRU value for each by 1, set the chosen one to MRU (max value)
    for (int i=0; i<replacement_size; i++){
        replacement_time[i] = replacement_time[i] - 1;
    }
    replacement_time[index] = counter;
    return index;
}

string create_str_record(int ID, string name, string phone){
	string str_ID = to_string(ID);
	if (str_ID.length() < 4){
		str_ID = string(4-str_ID.length(), '0') + str_ID;
	}
	string fill_name = "";
	if (name.length() < 16){
		fill_name = string(16 - name.length(), ' ');
	}
	return str_ID + name + fill_name + phone;
}

/**
Returns the record that has the given (unique) ID
**/
Record* return_record_from_ID(string table, int ID_value){
    fstream file;
    file.open(table);
    if (!file.is_open()){
        cout<<"Table does not exist"<<endl;
        return nullptr;
    }
    char* block = new char [block_size];
    while (file.read(block, block_size)){
        int index = memory_index();
        memory[index] = block;
        // search the block for the ID, return if found
        string pointers(block, block_size-number_records_in_block*2, number_records_in_block*2);
        for (int i=0; i<number_records_in_block; i++){
            short ptr = stoi(pointers.substr(i*2, 2));
            string str_block = string(block);
            int ID = atoi(str_block.substr(ptr, 4).c_str());
            if (ID == ID_value){
                string name = str_block.substr(ptr+4, 16);
                string phone = str_block.substr(ptr+20, 12);
                file.close();
                return new Record(ID, name, phone);
            }
        }
    }

    cout<<"Table does not have record with ID "<<ID_value<<endl;
    file.close();
    return nullptr;
}

/**
Returns a list of records that contains code_value in phone
**/
vector<Record> return_records_from_phone(string table, string code_value){
    vector<Record> records;
    fstream file;
    file.open(table);
    if (!file.is_open()){
        cout<<"Table does not exist"<<endl;
        return records;
    }
    char* block = new char [block_size];
    while (file.read(block, block_size)){
        int index = memory_index();
        string str_block = string(block);
        memory[index] = str_block;
        // search the block for the ID, return if found
        string pointers(block, block_size-number_records_in_block*2, number_records_in_block*2);
        for (int i=0; i<number_records_in_block; i++){
            short ptr = stoi(pointers.substr(i*2, 2));
            string phone = str_block.substr(ptr+24, 12);
            if (code_value.compare(phone.substr(0, 3)) == 0){
                // The record matches the query, retrieve the record
                int ID = atoi(str_block.substr(ptr, 4).c_str());
                string name = str_block.substr(ptr+4, 16);
                string phone = str_block.substr(ptr+24, 12);
                Record *record = new Record(ID, name, phone);
                records.push_back(*record);
            }
        }
    }
    file.close();
    return records;
}

void update_record(string table, Record record){
	fstream file;
	    file.open(table);
	    if (!file.is_open()){
	        cout<<"Table does not exist"<<endl;
	    }
	    char* block = new char [block_size];
	    while (file.read(block, block_size)){
	        int index = memory_index();
	        string str_block = string(block);
	        memory[index] = str_block;
	        // search the block for the ID, return if found
	        string pointers(str_block, block_size-number_records_in_block*2, number_records_in_block*2);
	        for (int i=0; i<number_records_in_block; i++){
	            short ptr = stoi(pointers.substr(i*2, 2));
	            int ID = atoi(str_block.substr(ptr, 4).c_str());
	            if (ID == record.ID){
	                string str_record = create_str_record(record.ID, record.clientName, record.phone);
	                str_block.replace(ptr, 32, str_record);
	                // Save the new block in file in the correct position
	                file.seekp(file.tellg() - block_size);
	                file.write(str_block.c_str(), block_size);
	                file.close();
	                break;
	            }
	        }
	    }
}

void write_new_record(string table, Record record){
	fstream file;
	    file.open(table);
	    if (!file.is_open()){
	        // TODO: Needs testing
	        cout<<"Table does not exist, create the table"<<endl;
	    }
	    char* block = new char [block_size];
	    bool found_flag = false;
	    while (file.read(block, block_size)){
	        int index = memory_index();
	        string str_block = string(block);
	        memory[index] = str_block;
	        // search the block for the ID, return if found
	        string pointers(block, block_size-number_records_in_block*2, number_records_in_block*2);
	        for (int i=0; i<number_records_in_block; i++){
	            short ptr = stoi(pointers.substr(i*2, 2));
	            cout<<ptr<<endl;
	            if (ptr == -1){
	            	cout<<"Adding new record in existing Block"<<endl;
	            	string fill_name = "";
					if (record.clientName.length() < 16){
						fill_name = string(16 - record.clientName.length(), ' ');
					}
					string str_record = to_string(record.ID) + record.clientName + fill_name + record.phone;
	            	str_block.replace(ptr, 32, str_record);
	            	int ptr_loc = atoi(pointers.substr(i*2, 2).c_str());
	            	int value = str_block.length() - ptr_loc;
	            	str_block.replace(value, 2, to_string(short(-1)));
	            	file.seekp(file.tellg() - block_size);
	            	file.write(str_block.c_str(), block_size);
	            	found_flag = true;
	            	break;
	            }
	        }
	    }

	    file.close();
	    if (!found_flag){
	    	cout<<"Adding a new record in a new block"<<endl;
	    	// create a new block and save the new record in it
	    	string str_record = create_str_record(record.ID, record.clientName, record.phone);
	    	string ptrs = "00";
	    	string initial_ptr_value = to_string(short(-1));
	    	for (int i=1; i<number_records_in_block; i++){
	    		ptrs += initial_ptr_value;
	    	}
	    	int empty_size = block_size - 32 - number_records_in_block*2;
	    	string empty = string(empty_size, ' ');
	    	string block = str_record + empty + ptrs;

	    	ofstream file;
	    	file.open(table, ios::app);
	    	file.write(block.c_str(), block_size);
	    	file.close();
	    }
}

/**
Writes a record
Insert: if the record does not exist in the DB
Update: if the record exists in the DB
**/
void write_record(string table, Record record){
	Record* r = return_record_from_ID(table, record.ID);
	if (r != nullptr){
		cout<<"Record found, updating"<<endl;
		update_record(table, record);
	} else {
		write_new_record(table, record);
	}
}

/**
Erases a record in the given table if exists.
**/
void erase_record(string table, int ID_value){
    fstream file;
    file.open(table);
    if (!file.is_open()){
        cout<<"Table does not exist"<<endl;
    }
    char* block = new char [block_size];
    while (file.read(block, block_size)){
        int index = memory_index();
        string str_block = string(block, block_size);
        cout<<"size of block before change"<<str_block.length()<<" block size "<<block_size<<endl;
        memory[index] = str_block;
        // search the block for the ID, return if found
        string pointers(str_block, block_size-number_records_in_block*2, number_records_in_block*2);
        cout<<"pointers: "<<pointers<<endl;
        for (int i=0; i<number_records_in_block; i++){
            short ptr = stoi(pointers.substr(i*2, 2));
            int ID = atoi(str_block.substr(ptr, 4).c_str());
            if (ID == ID_value){
                string empty = string(32, ' ');
                str_block.replace(ptr, 32, empty);
                int ptr_loc = pointers.length() - 1 - i*2;
                int value = str_block.length() - 1 - ptr_loc;
                cout<<"size of block "<<str_block.length()<<" place to replace with new value "<<value<<endl;
                str_block.replace(value, 2, to_string(short(-1)));
                // Save the new block in file in the correct position
                file.seekp(file.tellg() - block_size);
                file.write(str_block.c_str(), block_size);
                file.close();
                cout<<"Record with ID "<<ID_value<<" was deleted."<<endl;
                return;
            }
        }
    }

    cout<<"Table does not have record with ID "<<ID_value<<endl;
    file.close();
}

/**
Deletes a table in database if exists.
**/
void delete_table(string table){
	ifstream ifile(table.c_str());
    if ( (bool)ifile ){
    	ifile.close();
        remove(table.c_str());
        cout<<"Table "<<table<<" was deleted"<<endl;
    } else {
        cout<<"Table does not exist"<<endl;
    }
}

void run_read(string cmd){
	string op = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, op.size() + 1);
	string table_name = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, table_name.size() + 1);
	string id = cmd.substr(0, cmd.find(" ", 0));

	//return_record_from_ID(table_name, id);

	// Call Logger's Read method with returned tupple;
}

void run_write(string cmd) {
	string op = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, op.size() + 1);
	string table_name = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, table_name.size() + 1);
	string tuple = cmd.substr(0, cmd.size());

	//write_record(table_name, );
}

void run_mread(string cmd){
	string op = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, op.size() + 1);
	string table_name = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, table_name.size() + 1);
	string id = cmd.substr(0, cmd.find(" ", 0));

	return_records_from_phone(table_name, id);
	// Call Logger's MRead method with returned tupple;
}

void run_erase(string cmd){
	string op = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, op.size() + 1);
	string table_name = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, table_name.size() + 1);
	string id = cmd.substr(0, cmd.size());

	erase_record(table_name, atoi(id.c_str()));
	// Call Logger's Erase method with erased tupple;
	//logger.log_erase(table_name, id);
}

void run_delete(string cmd){
	string op = cmd.substr(0, cmd.find(" ", 0));
	cmd.erase(0, op.size() + 1);
	string table_name = cmd.substr(0, cmd.size());

	delete_table(table_name);
	// Call Logger's Delete method with deleted table
	//logger.log_delete(table_name);
}

int main(int argc, char *argv[]){

	fill_n(replacement_time, replacement_size, -1);

	for (int i = 0; i < argc; ++i)
	        cout << argv[i] << "\n";

	    string input_file = argv[1];
	    ifstream f(input_file);
	    if(!f.good()){
	        cout << "Entered File: " << input_file << " Not found." << "\n";
	        exit(1);
	    }

	    string impl = argv[2];
	    if(impl.compare("SEQ") != 0 && impl.compare("LSA") != 0){
	        cout << "Unrecogized input for Implementation. Expected SEQ or LSA. Received: " << impl << "\n";
	        exit(1);
	    }
	    string line;
	    ifstream file = ifstream(input_file);
		while(getline(file, line)){
			//cout << line << "\n";
			//logger.log_cmd(line);

			switch(line.at(0)){
				// Read
				case 'R':
					run_read(line);
					break;
				// Write
				case 'W':
					run_write(line);
					break;
				// MRead
				case 'M':
					run_mread(line);
					break;
				// Erase
				case 'E':
					run_erase(line);
					break;
				// Delete
				case 'D':
					run_delete(line);
					break;
				default:
					cout << "Unrecognized file input" << "\n";
			}
		}
}
