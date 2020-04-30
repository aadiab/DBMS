#ifndef _SRC_STRUCTS_HPP
#define _SRC_STRUCTS_HPP

#include "lsm_buffer.hpp"
#include "lsm.hpp"
#include "Logger.hpp"
#include "recovery_manager.hpp"

extern unordered_map<string, shared_ptr<Leveled_LSM>> lsm_ptrs;
extern std::shared_ptr<LsmBuffer> lsmBuffer;
extern std::shared_ptr<Logger> logger;
extern std::shared_ptr<RecoveryManager> recoveryManager;

#endif /* _SRC_STRUCTS_HPP */
