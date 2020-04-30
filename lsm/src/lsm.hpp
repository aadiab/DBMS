#ifndef _SRC_LSM_HPP
#define _SRC_LSM_HPP

#include <cstdint>
#include <ostream>
#include <set>
#include <algorithm>
#include <tuple>
#include <vector>
#include <cmath>
#include <iterator>
#include <sstream>
#include <fstream>
#include <array>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>
#include <filesystem>

using namespace std;

static string FILE_DIRECTORY = "/tmp/mypta/";
const int N_LSM_LEVELS = 7;
const int N_BLOCKS = 1;

/* Utility functions */
inline string
get_filename()
{
    static uint64_t id = 0;
    return to_string(id++);
}

template<typename Iter, typename Dist>
void
checked_advance(Iter& itr, const Iter& last, Dist dist)
{
    Dist maxdist(distance(itr, last));
    advance(itr, dist > maxdist ? maxdist : dist);
}

/* End utilitiy functions */

/* Structs and such */

struct Record
{
    union
    {
        uint32_t value;
        struct
        {
            uint32_t id      : 31;
            uint32_t deleted : 1;
        };
    };
    array<char, 16> name;
    array<char, 12> phone;
    enum Operation {
        NONE = 0,
        DELETE = 1
    };
};

Record
new_record();

inline bool operator<(const Record& l, const Record& r) { return l.id < r.id; }

struct RecordPtrCmp
{
    bool
    operator() (const Record* l, const Record* r) const { return l->id < r->id; }
};

struct SSTable
{
    const static uint32_t DISK_BLOCK_SIZE = 4096; /* Assumed */
    static uint32_t N_DISK_BLOCKS;
    static uint32_t N_RECORDS_PER_TABLE;
    static uint32_t TABLE_SIZE;

    SSTable(Record* ptr)
        : records(ptr)
    {
    }

    static void
    configure(int n_blocks)
    {
        N_DISK_BLOCKS = n_blocks;
        N_RECORDS_PER_TABLE = (DISK_BLOCK_SIZE * N_DISK_BLOCKS / sizeof(Record));
        TABLE_SIZE = sizeof(Record) * N_RECORDS_PER_TABLE;
    }

    string file_handle;
    Record* const records; /* Should point to a memory location of N_RECORDS_PER_TABLE of records */
    set<Record*, RecordPtrCmp> record_ptrs;
};

struct Index {
    /* FD, Start ID, End ID */
    using Entry = tuple<string, int, int>;
    using Container = vector<Entry>;

    Container entries;

    Container::iterator
    find(const Record& rec)
    {
        return find_if(entries.begin(), entries.end(),
                            [&] (const auto& entry){ return get<1>(entry) <= rec.id &&
                                    get<2>(entry) >= rec.id; });
    }

    Container::iterator
    find(int id)
    {
        return find_if(entries.begin(), entries.end(),
                            [&] (const auto& entry){ return get<1>(entry) <= id &&
                                    get<2>(entry) >= id; });
    }

    Container::iterator
    find(string str)
    {
        return find_if(entries.begin(), entries.end(),
                       [&] (const auto& entry){ return get<0>(entry) == str; });
    }

    Container::iterator
    find_prev(const Record& rec)
    {
        auto itr = entries.begin();
        auto max = entries.begin();
        while (itr != entries.end())
        {
            if (get<2>(*itr) < rec.id && get<2>(*itr) > get<2>(*max))
            {
                max = itr;
            }
            itr = next(itr);
        }
        return max;
    }
};

struct Level_Container {
    Level_Container(int table_limit, int rank, string table_name)
        : limit(table_limit), rank(rank), table_name(table_name) {}

    unsigned int limit;
    unsigned int rank;
    string table_name;
    vector<string> tables;
    Index index;
    bool is_top = false;
};

struct Leveled_LSM {
    vector<Level_Container> containers;
};

/* DB Table Name -> LSM */


/* End structs and stuff */

/* Stream operators */

template<typename T, size_t N>
ostream&
operator<<(ostream& os, const array<T, N>& fixed_str)
{
    for (const auto& c : fixed_str)
    {
        os << c;
    }
    return os;
}

template<typename T, size_t N>
istream&
operator>>(istream& is, array<T, N>& fixed_str)
{
    for (size_t i=0; i<N; i++)
    {
        is >> fixed_str[i];
    }
    return is;
}

ostream&
operator<<(ostream& os, const Record& rec);

string
to_string(const Record& rec);

string rep(const Record& rec);

istream&
operator>>(istream& is, Record& rec);

istream&
operator>>(istream& is, SSTable& table);

ostream&
operator<<(ostream& os, const SSTable& table);

/* End stream operators */

/* General function prototypes */

shared_ptr<SSTable>
copy_sstable(shared_ptr<SSTable> src, shared_ptr<SSTable> dst);

void
delete_index_entry(Index& index, string& fd);

void
update_index(Index& index, shared_ptr<SSTable> table, string& fd);

void
create_levels(int n_levels, string table_name);

set<Record*, RecordPtrCmp>&
merge_record_with_table(set<Record*, RecordPtrCmp>& rec_ptrs, Record* rec);

vector<string>::iterator
pick_table(Level_Container& level);

/* Find a record in the LSM tree.

   Specifically, consult each level's indices to find which table
   ought to contain the record. If located, bring that table to
   memory, read the record, and return it.
 */
tuple<bool, Record>
find_record(uint32_t id, Leveled_LSM& lsm);

set<Record>
mread(uint32_t area_code, Leveled_LSM& lsm);

void
flush_table(Level_Container& level, shared_ptr<SSTable> table, const string& handle);

string
build_fd(Level_Container& level, string partial_fd);

void
check_levels(Leveled_LSM& lsm);

void
flush_to_level(Level_Container& level, shared_ptr<SSTable> table);

void
compact_level(Level_Container& lower, Level_Container& upper);

#endif /* _SRC_LSM_HPP */
