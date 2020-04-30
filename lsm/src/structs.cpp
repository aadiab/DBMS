#include "structs.hpp"

unordered_map<string, shared_ptr<Leveled_LSM>> lsm_ptrs;
std::shared_ptr<LsmBuffer> lsmBuffer;
std::shared_ptr<Logger> logger;
std::shared_ptr<RecoveryManager> recoveryManager;
