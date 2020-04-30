#include "lsm.hpp"
#include "lsm_buffer.hpp"
#include "structs.hpp"
#include <iostream>

uint32_t SSTable::N_DISK_BLOCKS;
uint32_t SSTable::N_RECORDS_PER_TABLE;
uint32_t SSTable::TABLE_SIZE;

static std::array<string, 4> L0_order;
static int order_index = 0;

shared_ptr<SSTable>
copy_sstable(shared_ptr<SSTable> src, shared_ptr<SSTable> dst)
{
    dst->file_handle = src->file_handle;
    dst->record_ptrs = src->record_ptrs;
    memcpy(dst->records, src->records, sizeof(Record) * SSTable::N_RECORDS_PER_TABLE);
    return dst;
}

void
delete_index_entry(Index& index, string& fd)
{
    auto itr = find_if(index.entries.begin(), index.entries.end(),
                             [&] (const auto& entry) { return get<0>(entry) == fd; });
    if (itr != index.entries.end()) { index.entries.erase(itr); }
}

void
update_index(Index& index, shared_ptr<SSTable> table, string& fd)
{
    int start = (*table->record_ptrs.begin())->id;
    int end = (*table->record_ptrs.rbegin())->id;
    index.entries.push_back({fd, start, end});
}

void
create_levels(int n_levels, string table_name)
{
    filesystem::path path(FILE_DIRECTORY);
    path /= table_name; /* This is pretty neat */
    filesystem::create_directories(path);
    auto ptr = make_shared<Leveled_LSM>();
    ptr->containers.push_back(Level_Container(4, 0, table_name));
    for (int i=0; i < n_levels; i++)
    {
        filesystem::path level_path(path);
        level_path /= to_string(i);
        filesystem::create_directory(level_path);
        ptr->containers.push_back(Level_Container(pow(10, i+1), i+1, table_name));
    }
    ptr->containers.back().is_top = true;
    lsm_ptrs[table_name] = ptr;
}

set<Record*, RecordPtrCmp>&
merge_record_with_table(set<Record*, RecordPtrCmp>& rec_ptrs, Record* rec)
{
    auto lambda = [&](const auto& l) { return l->id == rec->id; };
    switch (rec->deleted) {
    case Record::Operation::DELETE:
    {
        set<Record*>::iterator itr = find_if(rec_ptrs.begin(), rec_ptrs.end(), lambda);
        if (itr != rec_ptrs.end())
        {
            rec_ptrs.erase(itr);
            rec_ptrs.insert(rec);
        }
        break;
    }
    case Record::Operation::NONE:
    {
        auto itr = find_if(rec_ptrs.begin(), rec_ptrs.end(), lambda);
        if (itr != rec_ptrs.end())
        {
            rec_ptrs.erase(itr);
            rec_ptrs.insert(rec);
        }
        else
        {
            rec_ptrs.insert(rec);
        }
        break;
    }
    default:
        break;
    }
    return rec_ptrs;
}

vector<string>::iterator
pick_table(Level_Container& level)
{
    if (level.tables.size() == 0) { return level.tables.end(); }
    int index = rand() % level.tables.size();
    auto itr = level.tables.begin();
    checked_advance(itr, level.tables.end(), index);
    return itr;
}

static void
print_index(Index& i)
{
    for (const auto& e : i.entries)
    {
        cout << "\tFD: " << get<0>(e) << "\n\t\tBegin: " << get<1>(e) << " End: " << get<2>(e) << endl;
    }
}

/* Find a record in the LSM tree.

   Specifically, consult each level's indices to find which table
   ought to contain the record. If located, bring that table to
   memory, read the record, and return it.
 */
tuple<bool, Record>
find_record(uint32_t id, Leveled_LSM& lsm)
{
    auto ptr = lsmBuffer->getOrLoadAndGetMemtable(lsm.containers[0].table_name);
    if (ptr)
    {
        for (const auto& rec : ptr->record_ptrs)
        {
            if (rec->id == id && rec->deleted == Record::Operation::DELETE)
            {
                return make_tuple(false, Record{});
            }
            else if (rec->id == id)
            {
                return make_tuple(true, *rec);
            }
        }
    }
    for (auto& level : lsm.containers)
    {
        print_index(level.index);
        if (level.rank == 0)
        {
            for (int i = 0; i<4; i++)
            {
                int offset = order_index-i;
                if (offset < 0)
                {
                    offset += 4;
                }
                shared_ptr<SSTable> table = lsmBuffer->getOrLoadAndGet(L0_order[offset]);
                // for (const auto& p : table->record_ptrs)
                // {
                //     cout << "Record: " << to_string(*p) << endl;
                // }
                auto table_itr = find_if(table->record_ptrs.begin(), table->record_ptrs.end(),
                                         [&] (const auto& rec_ptr) { return rec_ptr->id == id; });
                if (table_itr != table->record_ptrs.end() &&
                    (*table_itr)->deleted == Record::Operation::DELETE)
                {
                    return make_tuple(false, Record{});
                }
                else if (table_itr != table->record_ptrs.end())
                {
                    return make_tuple(true, **table_itr);
                }
            }
        }
        else
        {
            auto itr = level.index.find(id);
            if (itr != level.index.entries.end())
            {
                shared_ptr<SSTable> table = lsmBuffer->getOrLoadAndGet(get<0>(*itr));
                // for (const auto& p : table->record_ptrs)
                // {
                //     cout << "Record: " << to_string(*p) << endl;
                // }
                auto table_itr = find_if(table->record_ptrs.begin(), table->record_ptrs.end(),
                                         [&] (const auto& rec_ptr) { return rec_ptr->id == id; });
                if (table_itr != table->record_ptrs.end() &&
                    (*table_itr)->deleted == Record::Operation::DELETE)
                {
                    return make_tuple(false, Record{});
                }
                else if (table_itr != table->record_ptrs.end())
                {
                    return make_tuple(true, **table_itr);
                }
            }
        }
    }
    return make_tuple(false, Record{});
}

set<Record>
mread(uint32_t area_code, Leveled_LSM& lsm)
{
    auto check_area_code = [] (array<char, 12>& to_check, uint32_t area_code) {
                               ostringstream ss;
                               ss << string{to_check[0]} << string{to_check[1]} << string{to_check[2]};
                               istringstream is(ss.str());
                               uint32_t ext = 0;
                               is >> ext;
                               return to_check.size() == 0 || ext == area_code;
                             };

    std::set<Record> results;

    auto ptr = lsmBuffer->getOrLoadAndGetMemtable(lsm.containers[0].table_name);
    if (ptr)
    {
        for (const auto& rec : ptr->record_ptrs)
        {
            if (check_area_code(rec->phone, area_code) && rec->deleted != Record::Operation::DELETE)
            {
                results.insert(*rec);
            }
        }
    }
    for (const auto& level : lsm.containers)
    {
        if (level.rank == 0)
        {
            for (int i = 0; i<4; i++)
            {
                int offset = order_index-i;
                if (offset < 0) { offset += 4; }
                shared_ptr<SSTable> table = lsmBuffer->getOrLoadAndGet(L0_order[offset]);
                for (const auto& rec : table->record_ptrs)
                {
                    if (rec->deleted == 0 && check_area_code(rec->phone, area_code))
                    {
                        results.insert(*rec);
                    }
                }
            }
        }
        else
        {
            for (const auto& index_entry_tuple : level.index.entries)
            {
                shared_ptr<SSTable> table = lsmBuffer->getOrLoadAndGet(get<0>(index_entry_tuple));
                for (const auto& record : table->record_ptrs)
                {
                    if (record->deleted == 0 && check_area_code(record->phone, area_code))
                    {
                        results.insert(*record);
                    }
                }
            }
        }
    }
    return results;
}

void
flush_table(Level_Container& level, shared_ptr<SSTable> table, const string& handle)
{
    cout << "\tChecking for DELETE records in " << table->record_ptrs.size() << " records." << endl;
    /* We don't need to retain DELETE records if we are at the top level. */
    if (level.is_top == true)
    {
        auto itr = table->record_ptrs.begin();
        while (itr != table->record_ptrs.end())
        {
            if ((*itr)->deleted == Record::Operation::DELETE)
            {
                itr = table->record_ptrs.erase(itr);
            }
            else
            {
                itr = next(itr);
            }
        }
    }
    if (level.rank == 0)
    {
        L0_order[order_index] = handle;
        order_index = (order_index + 1) % 4;
    }
    cout << "Writing (" << table->record_ptrs.size() << " records) to: " << handle << endl;
    /* Flush to a file */
    ofstream fd(handle, ios::binary);
    fd << *table;
    fd.close();
}

string
build_fd(Level_Container& level, string partial_fd)
{
    stringstream file_handle;
    file_handle << FILE_DIRECTORY << level.table_name << "/" << level.rank << "/" << partial_fd;
    return file_handle.str();
}

void
check_levels(Leveled_LSM& lsm)
{
    for (unsigned int i=0; i<lsm.containers.size()-1; i++)
    {
        if (lsm.containers[i].tables.size() > lsm.containers[i].limit)
        {
            compact_level(lsm.containers[i], lsm.containers[i+1]);
        }
    }
}

void
flush_to_level(Level_Container& level, shared_ptr<SSTable> table)
{
    cout << "\tFlushing table to table: " << level.table_name << " level: " << level.rank << endl;
    table->file_handle = build_fd(level, get_filename());
    flush_table(level, table, table->file_handle);
    update_index(level.index, table, table->file_handle);

}

void
compact_level(Level_Container& lower, Level_Container& upper)
{
    bool cont = lower.tables.size() > lower.limit;
    if (lower.rank == 0) { cont = lower.tables.size() > 0; }
    while (cont) /* In the case of L0 we empty it */
    {
        auto lower_tables_itr = pick_table(lower);
        if (lower_tables_itr == lower.tables.end()) { return; }
        shared_ptr<SSTable> tmp = lsmBuffer->getOrLoadAndGet(*lower_tables_itr);
        shared_ptr<SSTable> lower_table = lsmBuffer->getUnusedCompactionTable();
        copy_sstable(tmp, lower_table);
        /* Remove the lower table from the lower level bookkeeping */
        {
            delete_index_entry(lower.index, *lower_tables_itr);
            lower.tables.erase(lower_tables_itr);
        }

        /* Acquire the container of pointers to do cheaper manipulation */
        auto lower_ptrs_itr = lower_table->record_ptrs.begin();
        while (lower_ptrs_itr != lower_table->record_ptrs.end())
        {
            shared_ptr<SSTable> upper_table = lsmBuffer->getUnusedCompactionTable();
            if (!upper_table)
            {
                /* Panic */
            }
            { /* Find the table that the current element belongs to. */
                auto index_itr = upper.index.find(**lower_ptrs_itr);
                if (index_itr != upper.index.entries.end())
                {
                    auto t_itr = find_if(upper.tables.begin(), upper.tables.end(),
                                         [&](const auto& table_fd) { return table_fd == get<0>(*index_itr); });
                    if (t_itr != upper.tables.end()) { upper.tables.erase(t_itr); }
                    delete_index_entry(upper.index, get<0>(*index_itr));
                    tmp = lsmBuffer->getOrLoadAndGet(get<0>(*index_itr));
                    copy_sstable(tmp, upper_table);
                }
                else /* Maybe just unfilled */
                {
                    auto itr = upper.index.find_prev(**lower_ptrs_itr);
                    if (itr != upper.index.entries.end())
                    {
                        auto t_itr = find_if(upper.tables.begin(), upper.tables.end(),
                                             [&](const auto& table_fd) { return table_fd == get<0>(*itr); });
                        if (t_itr != upper.tables.end()) { upper.tables.erase(t_itr); }
                        delete_index_entry(upper.index, get<0>(*itr));
                        tmp = lsmBuffer->getOrLoadAndGet(get<0>(*itr));
                        if (tmp->record_ptrs.size() < SSTable::N_RECORDS_PER_TABLE)
                        {
                            copy_sstable(tmp, upper_table);
                        }
                    }
                }
            }
            merge_record_with_table(upper_table->record_ptrs, *lower_ptrs_itr);
            lower_ptrs_itr = lower_table->record_ptrs.erase(lower_ptrs_itr); //
            /* The table has been merged, time to chunk and flush */
            {
                shared_ptr<SSTable> table_to_flush = lsmBuffer->getUnusedCompactionTable();
                if (!table_to_flush)
                {
                    /* Freak out */
                }
                memset(table_to_flush->records, '0', SSTable::N_RECORDS_PER_TABLE*sizeof(Record));
                Record* ptr = table_to_flush->records;
                auto itr = upper_table->record_ptrs.begin();
                auto end_itr = upper_table->record_ptrs.begin();
                checked_advance(end_itr, upper_table->record_ptrs.end(), SSTable::N_RECORDS_PER_TABLE+1);
                while (itr != end_itr) // Fill the destination table
                {
                    *ptr = **itr;
                    table_to_flush->record_ptrs.insert(ptr);
                    ptr++;
                    itr = upper_table->record_ptrs.erase(itr); // Need to do this?
                }
                /* The table has been filled with as many items as
                 * possible, time to flush */
                flush_to_level(upper, table_to_flush);
                upper.tables.push_back(table_to_flush->file_handle);
                if (upper_table->record_ptrs.size() > 0)
                { /* Flush the remaining upper table elements back */
                    /* Even though the table currently has a
                     * filename we won't complicate the caching
                     * process by re-using it. We can always
                     * garbage-collect unused files later on.*/
                    flush_to_level(upper, upper_table);
                }
                else /* Remove the fully merged, empty table from the container */
                {
                    auto itr = find(upper.tables.begin(), upper.tables.end(), upper_table->file_handle);
                    if (itr != upper.tables.end()) { upper.tables.erase(itr); }
                }
            }
        }
        if (lower_table->record_ptrs.size() > 0)
        {
            /* Error!
             */
        }
        cont = lower.tables.size() > lower.limit;
        if (lower.rank == 0) { cont = lower.tables.size() > 0; }
    }
}

ostream&
operator<<(ostream& os, const Record& rec)
{
    os.write(reinterpret_cast<const char*>(&rec), sizeof(Record));
    return os;
}

string
to_string(const Record& rec)
{
    ostringstream name;
    name << rec.name;
    ostringstream phone;
    phone << rec.phone;
    ostringstream str;
    str << "ID: "  << rec.id << " Name: " << name.str() << " Phone: " << phone.str();
    return str.str();
}

string rep(const Record& rec) {
    ostringstream name;
    name << rec.name;
    ostringstream phone;
    phone << rec.phone;
    ostringstream str;
    str << "(" << rec.id << "," << name.str() << "," << phone.str() << ")";
    return str.str();
}

istream&
operator>>(istream& is, Record& rec)
{
    is.read(reinterpret_cast<char*>(&rec.value), sizeof(uint32_t));
    is.read(reinterpret_cast<char*>(&rec.name), sizeof(array<char, 12>));
    is.read(reinterpret_cast<char*>(&rec.phone), sizeof(array<char, 16>));
    return is;
}

istream&
operator>>(istream& is, SSTable& table)
{
    memset(table.records, '\0', sizeof(Record)*SSTable::N_RECORDS_PER_TABLE);
    is.read(reinterpret_cast<char*>(table.records), sizeof(Record)*SSTable::N_RECORDS_PER_TABLE);
    for (unsigned int i=0; i < SSTable::N_RECORDS_PER_TABLE; i++)
    {
        table.record_ptrs.insert(&table.records[i]);
    }
    return is;
}

ostream&
operator<<(ostream& os, const SSTable& table)
{
    auto itr = table.record_ptrs.begin();
    for (unsigned int i=0; i<SSTable::N_RECORDS_PER_TABLE; i++)
    {
        if (itr != table.record_ptrs.end())
        {
            os << **itr;
        }
        else
        {
            os << new_record();
        }
        itr = next(itr);
    }

    return os;
}

Record
new_record()
{
    Record r;
    memset(&r, '\0', sizeof(Record));
    return r;
}
