//# include "structs.hpp"
# include <vector>
# include <chrono>


struct Statistics {
	// Total counters
    int total_commands = 0;
	int total_read_count = 0;
    int total_write_count = 0;
    int total_mread_count = 0;
    int total_erase_count = 0;
    int total_delete_count = 0;

    // Abort Counters:
    int abort_read_count = 0;
	int abort_write_count = 0;
	int abort_mread_count = 0;
	int abort_erase_count = 0;
	int abort_delete_count = 0;

	// Response time
	std::chrono::duration<double> read_time = std::chrono::duration<double>(0);
	std::chrono::duration<double> write_time = std::chrono::duration<double>(0);
	std::chrono::duration<double> mread_time = std::chrono::duration<double>(0);
	std::chrono::duration<double> erase_time = std::chrono::duration<double>(0);
	std::chrono::duration<double> delete_time = std::chrono::duration<double>(0);

};

Statistics
* newStatistics();


