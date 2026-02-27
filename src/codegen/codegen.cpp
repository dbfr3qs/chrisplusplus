#include "codegen/codegen.h"

#include "llvm/IR/Verifier.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"

#include <sstream>

namespace chris {

CodeGen::CodeGen(const std::string& moduleName, DiagnosticEngine& diagnostics)
    : diagnostics_(diagnostics) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(moduleName, *context_);
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);

    declareRuntimeFunctions();
}

void CodeGen::declareRuntimeFunctions() {
    auto* i8PtrTy = llvm::PointerType::getUnqual(*context_);
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* doubleTy = llvm::Type::getDoubleTy(*context_);
    auto* i1Ty = llvm::Type::getInt1Ty(*context_);
    auto* voidTy = llvm::Type::getVoidTy(*context_);

    // chris_print(const char* str) -> void
    auto* printFnTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimePrint_ = llvm::Function::Create(printFnTy, llvm::Function::ExternalLinkage,
                                            "chris_print", module_.get());

    // chris_strcat(const char* a, const char* b) -> const char*
    auto* strcatFnTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i8PtrTy}, false);
    runtimeStrcat_ = llvm::Function::Create(strcatFnTy, llvm::Function::ExternalLinkage,
                                             "chris_strcat", module_.get());

    // chris_int_to_str(int64_t val) -> const char*
    auto* intToStrFnTy = llvm::FunctionType::get(i8PtrTy, {i64Ty}, false);
    runtimeIntToStr_ = llvm::Function::Create(intToStrFnTy, llvm::Function::ExternalLinkage,
                                               "chris_int_to_str", module_.get());

    // chris_float_to_str(double val) -> const char*
    auto* floatToStrFnTy = llvm::FunctionType::get(i8PtrTy, {doubleTy}, false);
    runtimeFloatToStr_ = llvm::Function::Create(floatToStrFnTy, llvm::Function::ExternalLinkage,
                                                  "chris_float_to_str", module_.get());

    // chris_bool_to_str(bool val) -> const char*
    auto* boolToStrFnTy = llvm::FunctionType::get(i8PtrTy, {i1Ty}, false);
    runtimeBoolToStr_ = llvm::Function::Create(boolToStrFnTy, llvm::Function::ExternalLinkage,
                                                 "chris_bool_to_str", module_.get());

    // chris_char_to_str(char c) -> const char*
    auto* i8Ty = llvm::Type::getInt8Ty(*context_);
    auto* charToStrFnTy = llvm::FunctionType::get(i8PtrTy, {i8Ty}, false);
    runtimeCharToStr_ = llvm::Function::Create(charToStrFnTy, llvm::Function::ExternalLinkage,
                                                 "chris_char_to_str", module_.get());

    auto* i32Ty = llvm::Type::getInt32Ty(*context_);

    // chris_try_begin() -> int
    auto* tryBeginTy = llvm::FunctionType::get(i32Ty, {}, false);
    runtimeTryBegin_ = llvm::Function::Create(tryBeginTy, llvm::Function::ExternalLinkage,
                                               "chris_try_begin", module_.get());

    // chris_try_end() -> void
    auto* tryEndTy = llvm::FunctionType::get(voidTy, {}, false);
    runtimeTryEnd_ = llvm::Function::Create(tryEndTy, llvm::Function::ExternalLinkage,
                                             "chris_try_end", module_.get());

    // chris_throw(const char* message) -> void
    auto* throwTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeThrow_ = llvm::Function::Create(throwTy, llvm::Function::ExternalLinkage,
                                            "chris_throw", module_.get());

    // chris_get_exception() -> const char*
    auto* getExcTy = llvm::FunctionType::get(i8PtrTy, {}, false);
    runtimeGetException_ = llvm::Function::Create(getExcTy, llvm::Function::ExternalLinkage,
                                                    "chris_get_exception", module_.get());

    // chris_get_jmpbuf(int depth) -> ptr (jmp_buf*)
    auto* getJmpbufTy = llvm::FunctionType::get(i8PtrTy, {i32Ty}, false);
    runtimeGetJmpbuf_ = llvm::Function::Create(getJmpbufTy, llvm::Function::ExternalLinkage,
                                                 "chris_get_jmpbuf", module_.get());

    // setjmp(jmp_buf*) -> int  (C library)
    auto* setjmpTy = llvm::FunctionType::get(i32Ty, {i8PtrTy}, false);
    setjmpFunc_ = llvm::Function::Create(setjmpTy, llvm::Function::ExternalLinkage,
                                          "setjmp", module_.get());

    // chris_array_alloc(i64 elem_size, i64 count) -> ptr
    auto* arrayAllocTy = llvm::FunctionType::get(i8PtrTy, {i64Ty, i64Ty}, false);
    runtimeArrayAlloc_ = llvm::Function::Create(arrayAllocTy, llvm::Function::ExternalLinkage,
                                                 "chris_array_alloc", module_.get());

    // chris_array_bounds_check(i64 index, i64 length) -> void
    auto* arrayBoundsCheckTy = llvm::FunctionType::get(voidTy, {i64Ty, i64Ty}, false);
    runtimeArrayBoundsCheck_ = llvm::Function::Create(arrayBoundsCheckTy, llvm::Function::ExternalLinkage,
                                                       "chris_array_bounds_check", module_.get());

    // chris_str_to_int(const char*) -> i64
    auto* strToIntTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeStrToInt_ = llvm::Function::Create(strToIntTy, llvm::Function::ExternalLinkage,
                                               "chris_str_to_int", module_.get());

    // chris_str_to_float(const char*) -> double
    auto* strToFloatTy = llvm::FunctionType::get(doubleTy, {i8PtrTy}, false);
    runtimeStrToFloat_ = llvm::Function::Create(strToFloatTy, llvm::Function::ExternalLinkage,
                                                 "chris_str_to_float", module_.get());

    // chris_str_len(const char*) -> i64
    auto* strLenTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeStrLen_ = llvm::Function::Create(strLenTy, llvm::Function::ExternalLinkage,
                                             "chris_str_len", module_.get());

    // chris_str_contains(str, substr) -> bool (i1)
    auto* strContainsTy = llvm::FunctionType::get(i1Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeStrContains_ = llvm::Function::Create(strContainsTy, llvm::Function::ExternalLinkage,
                                                  "chris_str_contains", module_.get());

    // chris_str_starts_with(str, prefix) -> bool (i1)
    auto* strStartsWithTy = llvm::FunctionType::get(i1Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeStrStartsWith_ = llvm::Function::Create(strStartsWithTy, llvm::Function::ExternalLinkage,
                                                     "chris_str_starts_with", module_.get());

    // chris_str_ends_with(str, suffix) -> bool (i1)
    auto* strEndsWithTy = llvm::FunctionType::get(i1Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeStrEndsWith_ = llvm::Function::Create(strEndsWithTy, llvm::Function::ExternalLinkage,
                                                   "chris_str_ends_with", module_.get());

    // chris_str_index_of(str, substr) -> i64
    auto* strIndexOfTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeStrIndexOf_ = llvm::Function::Create(strIndexOfTy, llvm::Function::ExternalLinkage,
                                                 "chris_str_index_of", module_.get());

    // chris_str_substring(str, start, end) -> str
    auto* strSubstringTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i64Ty, i64Ty}, false);
    runtimeStrSubstring_ = llvm::Function::Create(strSubstringTy, llvm::Function::ExternalLinkage,
                                                    "chris_str_substring", module_.get());

    // chris_str_replace(str, old, new) -> str
    auto* strReplaceTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i8PtrTy, i8PtrTy}, false);
    runtimeStrReplace_ = llvm::Function::Create(strReplaceTy, llvm::Function::ExternalLinkage,
                                                  "chris_str_replace", module_.get());

    // chris_str_trim(str) -> str
    auto* strTrimTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeStrTrim_ = llvm::Function::Create(strTrimTy, llvm::Function::ExternalLinkage,
                                              "chris_str_trim", module_.get());

    // chris_str_to_upper(str) -> str
    auto* strToUpperTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeStrToUpper_ = llvm::Function::Create(strToUpperTy, llvm::Function::ExternalLinkage,
                                                  "chris_str_to_upper", module_.get());

    // chris_str_to_lower(str) -> str
    auto* strToLowerTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeStrToLower_ = llvm::Function::Create(strToLowerTy, llvm::Function::ExternalLinkage,
                                                  "chris_str_to_lower", module_.get());

    // chris_str_split(str, delim, out_array_ptr) -> void
    // out_array_ptr is a pointer to Array struct {i64, ptr}
    auto* strSplitTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy, i8PtrTy}, false);
    runtimeStrSplit_ = llvm::Function::Create(strSplitTy, llvm::Function::ExternalLinkage,
                                               "chris_str_split", module_.get());

    // chris_str_char_at(str, index) -> str (single char as string)
    auto* strCharAtTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i64Ty}, false);
    runtimeStrCharAt_ = llvm::Function::Create(strCharAtTy, llvm::Function::ExternalLinkage,
                                                "chris_str_char_at", module_.get());

    // Array methods
    // chris_array_push(array_ptr, elem_size, value_i64) -> void
    auto* arrayPushTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i64Ty, i64Ty}, false);
    runtimeArrayPush_ = llvm::Function::Create(arrayPushTy, llvm::Function::ExternalLinkage,
                                                "chris_array_push", module_.get());

    // chris_array_pop(array_ptr, elem_size) -> i64
    auto* arrayPopTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty}, false);
    runtimeArrayPop_ = llvm::Function::Create(arrayPopTy, llvm::Function::ExternalLinkage,
                                               "chris_array_pop", module_.get());

    // chris_array_reverse(array_ptr, elem_size) -> void
    auto* arrayReverseTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i64Ty}, false);
    runtimeArrayReverse_ = llvm::Function::Create(arrayReverseTy, llvm::Function::ExternalLinkage,
                                                    "chris_array_reverse", module_.get());

    // chris_array_join(array_ptr, separator) -> char*
    auto* arrayJoinTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i8PtrTy}, false);
    runtimeArrayJoin_ = llvm::Function::Create(arrayJoinTy, llvm::Function::ExternalLinkage,
                                                "chris_array_join", module_.get());

    // chris_array_map(array_ptr, elem_size, callback, out_array_ptr) -> void
    auto* arrayMapTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i64Ty, i8PtrTy, i8PtrTy}, false);
    runtimeArrayMap_ = llvm::Function::Create(arrayMapTy, llvm::Function::ExternalLinkage,
                                               "chris_array_map", module_.get());

    // chris_array_filter(array_ptr, elem_size, callback, out_array_ptr) -> void
    auto* arrayFilterTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i64Ty, i8PtrTy, i8PtrTy}, false);
    runtimeArrayFilter_ = llvm::Function::Create(arrayFilterTy, llvm::Function::ExternalLinkage,
                                                   "chris_array_filter", module_.get());

    // chris_array_foreach(array_ptr, elem_size, callback) -> void
    auto* arrayForEachTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i64Ty, i8PtrTy}, false);
    runtimeArrayForEach_ = llvm::Function::Create(arrayForEachTy, llvm::Function::ExternalLinkage,
                                                    "chris_array_foreach", module_.get());

    // Array struct type: {i64 length, ptr data}
    arrayStructType_ = llvm::StructType::create(*context_, {i64Ty, i8PtrTy}, "Array");

    // Future struct type (opaque pointer — runtime manages internals)
    futureStructType_ = llvm::StructType::create(*context_, "Future");

    // chris_async_spawn(func_ptr, arg_ptr, kind) -> Future*
    // kind: 0=io, 1=compute
    auto* asyncSpawnTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i8PtrTy, i32Ty}, false);
    runtimeAsyncSpawn_ = llvm::Function::Create(asyncSpawnTy, llvm::Function::ExternalLinkage,
                                                  "chris_async_spawn", module_.get());

    // chris_async_await(Future*) -> i64 (result as i64, caller reinterprets)
    auto* asyncAwaitTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeAsyncAwait_ = llvm::Function::Create(asyncAwaitTy, llvm::Function::ExternalLinkage,
                                                  "chris_async_await", module_.get());

    // chris_async_run_loop() -> void (drain the event loop, called at end of main)
    auto* asyncRunLoopTy = llvm::FunctionType::get(voidTy, {}, false);
    runtimeAsyncRunLoop_ = llvm::Function::Create(asyncRunLoopTy, llvm::Function::ExternalLinkage,
                                                    "chris_async_run_loop", module_.get());

    // Shared class mutex runtime functions
    // chris_mutex_init(ptr) -> void  (ptr points to mutex bytes in struct)
    auto* mutexInitTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeMutexInit_ = llvm::Function::Create(mutexInitTy, llvm::Function::ExternalLinkage,
                                                "chris_mutex_init", module_.get());

    // chris_mutex_lock(ptr) -> void
    auto* mutexLockTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeMutexLock_ = llvm::Function::Create(mutexLockTy, llvm::Function::ExternalLinkage,
                                                "chris_mutex_lock", module_.get());

    // chris_mutex_unlock(ptr) -> void
    auto* mutexUnlockTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeMutexUnlock_ = llvm::Function::Create(mutexUnlockTy, llvm::Function::ExternalLinkage,
                                                  "chris_mutex_unlock", module_.get());

    // chris_mutex_destroy(ptr) -> void
    auto* mutexDestroyTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeMutexDestroy_ = llvm::Function::Create(mutexDestroyTy, llvm::Function::ExternalLinkage,
                                                    "chris_mutex_destroy", module_.get());

    // Reflection runtime: TypeInfo struct { ptr name, i64 num_fields, ptr fields, i64 num_implements, ptr implements }
    typeInfoStructType_ = llvm::StructType::create(*context_, {
        i8PtrTy,  // name (const char*)
        i64Ty,    // num_fields
        i8PtrTy,  // fields (const char**)
        i64Ty,    // num_implements
        i8PtrTy   // implements (const char**)
    }, "ChrisTypeInfo");

    // chris_typeinfo_name(ptr info) -> ptr (string)
    auto* tiNameTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeTypeInfoName_ = llvm::Function::Create(tiNameTy, llvm::Function::ExternalLinkage,
                                                    "chris_typeinfo_name", module_.get());

    // chris_typeinfo_fields(ptr info, ptr out_array) -> void
    auto* tiFieldsTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy}, false);
    runtimeTypeInfoFields_ = llvm::Function::Create(tiFieldsTy, llvm::Function::ExternalLinkage,
                                                      "chris_typeinfo_fields", module_.get());

    // chris_typeinfo_implements(ptr info, ptr out_array) -> void
    auto* tiImplTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy}, false);
    runtimeTypeInfoImplements_ = llvm::Function::Create(tiImplTy, llvm::Function::ExternalLinkage,
                                                          "chris_typeinfo_implements", module_.get());

    // Process execution runtime functions
    // chris_exec(ptr command) -> i64 (exit code)
    auto* execTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeExec_ = llvm::Function::Create(execTy, llvm::Function::ExternalLinkage,
                                           "chris_exec", module_.get());

    // chris_exec_output(ptr command) -> ptr (stdout string)
    auto* execOutputTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeExecOutput_ = llvm::Function::Create(execOutputTy, llvm::Function::ExternalLinkage,
                                                  "chris_exec_output", module_.get());

    // File I/O runtime functions
    // chris_read_file(ptr path) -> ptr (string)
    auto* readFileTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeReadFile_ = llvm::Function::Create(readFileTy, llvm::Function::ExternalLinkage,
                                               "chris_read_file", module_.get());

    // chris_write_file(ptr path, ptr content) -> i32 (1=ok, 0=fail)
    auto* writeFileTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeWriteFile_ = llvm::Function::Create(writeFileTy, llvm::Function::ExternalLinkage,
                                                "chris_write_file", module_.get());

    // chris_append_file(ptr path, ptr content) -> i32 (1=ok, 0=fail)
    auto* appendFileTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeAppendFile_ = llvm::Function::Create(appendFileTy, llvm::Function::ExternalLinkage,
                                                 "chris_append_file", module_.get());

    // chris_file_exists(ptr path) -> i32 (1=exists, 0=not)
    auto* fileExistsTy = llvm::FunctionType::get(i32Ty, {i8PtrTy}, false);
    runtimeFileExists_ = llvm::Function::Create(fileExistsTy, llvm::Function::ExternalLinkage,
                                                  "chris_file_exists", module_.get());

    // Map runtime functions
    // chris_map_create() -> ptr
    auto* mapCreateTy = llvm::FunctionType::get(i8PtrTy, {}, false);
    runtimeMapCreate_ = llvm::Function::Create(mapCreateTy, llvm::Function::ExternalLinkage,
                                                "chris_map_create", module_.get());

    // chris_map_set(ptr map, ptr key, i64 value) -> void
    auto* mapSetTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy, i64Ty}, false);
    runtimeMapSet_ = llvm::Function::Create(mapSetTy, llvm::Function::ExternalLinkage,
                                             "chris_map_set", module_.get());

    // chris_map_get(ptr map, ptr key) -> i64
    auto* mapGetTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeMapGet_ = llvm::Function::Create(mapGetTy, llvm::Function::ExternalLinkage,
                                             "chris_map_get", module_.get());

    // chris_map_has(ptr map, ptr key) -> i32
    auto* mapHasTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeMapHas_ = llvm::Function::Create(mapHasTy, llvm::Function::ExternalLinkage,
                                             "chris_map_has", module_.get());

    // chris_map_delete(ptr map, ptr key) -> i32
    auto* mapDeleteTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeMapDelete_ = llvm::Function::Create(mapDeleteTy, llvm::Function::ExternalLinkage,
                                                "chris_map_delete", module_.get());

    // chris_map_size(ptr map) -> i64
    auto* mapSizeTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeMapSize_ = llvm::Function::Create(mapSizeTy, llvm::Function::ExternalLinkage,
                                              "chris_map_size", module_.get());

    // chris_map_destroy(ptr map) -> void
    auto* mapDestroyTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeMapDestroy_ = llvm::Function::Create(mapDestroyTy, llvm::Function::ExternalLinkage,
                                                 "chris_map_destroy", module_.get());

    // chris_map_keys(ptr map, ptr out_array) -> void
    auto* mapKeysTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy}, false);
    runtimeMapKeys_ = llvm::Function::Create(mapKeysTy, llvm::Function::ExternalLinkage,
                                              "chris_map_keys", module_.get());

    // Networking runtime functions (TCP)
    // chris_tcp_connect(ptr host, i64 port) -> i64
    auto* tcpConnectTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i64Ty}, false);
    runtimeTcpConnect_ = llvm::Function::Create(tcpConnectTy, llvm::Function::ExternalLinkage,
                                                  "chris_tcp_connect", module_.get());

    // chris_tcp_listen(i64 port) -> i64
    auto* tcpListenTy = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    runtimeTcpListen_ = llvm::Function::Create(tcpListenTy, llvm::Function::ExternalLinkage,
                                                 "chris_tcp_listen", module_.get());

    // chris_tcp_accept(i64 serverFd) -> i64
    auto* tcpAcceptTy = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    runtimeTcpAccept_ = llvm::Function::Create(tcpAcceptTy, llvm::Function::ExternalLinkage,
                                                 "chris_tcp_accept", module_.get());

    // chris_tcp_send(i64 fd, ptr data) -> i64
    auto* tcpSendTy = llvm::FunctionType::get(i64Ty, {i64Ty, i8PtrTy}, false);
    runtimeTcpSend_ = llvm::Function::Create(tcpSendTy, llvm::Function::ExternalLinkage,
                                               "chris_tcp_send", module_.get());

    // chris_tcp_recv(i64 fd, i64 maxBytes) -> ptr
    auto* tcpRecvTy = llvm::FunctionType::get(i8PtrTy, {i64Ty, i64Ty}, false);
    runtimeTcpRecv_ = llvm::Function::Create(tcpRecvTy, llvm::Function::ExternalLinkage,
                                               "chris_tcp_recv", module_.get());

    // chris_tcp_close(i64 fd) -> void
    auto* tcpCloseTy = llvm::FunctionType::get(voidTy, {i64Ty}, false);
    runtimeTcpClose_ = llvm::Function::Create(tcpCloseTy, llvm::Function::ExternalLinkage,
                                                "chris_tcp_close", module_.get());

    // Networking runtime functions (UDP)
    // chris_udp_create() -> i64
    auto* udpCreateTy = llvm::FunctionType::get(i64Ty, {}, false);
    runtimeUdpCreate_ = llvm::Function::Create(udpCreateTy, llvm::Function::ExternalLinkage,
                                                 "chris_udp_create", module_.get());

    // chris_udp_bind(i64 fd, i64 port) -> i64
    auto* udpBindTy = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty}, false);
    runtimeUdpBind_ = llvm::Function::Create(udpBindTy, llvm::Function::ExternalLinkage,
                                               "chris_udp_bind", module_.get());

    // chris_udp_send_to(i64 fd, ptr host, i64 port, ptr data) -> i64
    auto* udpSendToTy = llvm::FunctionType::get(i64Ty, {i64Ty, i8PtrTy, i64Ty, i8PtrTy}, false);
    runtimeUdpSendTo_ = llvm::Function::Create(udpSendToTy, llvm::Function::ExternalLinkage,
                                                  "chris_udp_send_to", module_.get());

    // chris_udp_recv_from(i64 fd, i64 maxBytes) -> ptr
    auto* udpRecvFromTy = llvm::FunctionType::get(i8PtrTy, {i64Ty, i64Ty}, false);
    runtimeUdpRecvFrom_ = llvm::Function::Create(udpRecvFromTy, llvm::Function::ExternalLinkage,
                                                    "chris_udp_recv_from", module_.get());

    // chris_udp_close(i64 fd) -> void
    auto* udpCloseTy = llvm::FunctionType::get(voidTy, {i64Ty}, false);
    runtimeUdpClose_ = llvm::Function::Create(udpCloseTy, llvm::Function::ExternalLinkage,
                                                "chris_udp_close", module_.get());

    // DNS runtime function
    // chris_dns_lookup(ptr hostname) -> ptr
    auto* dnsLookupTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeDnsLookup_ = llvm::Function::Create(dnsLookupTy, llvm::Function::ExternalLinkage,
                                                 "chris_dns_lookup", module_.get());

    // ConcurrentMap runtime functions
    // chris_cmap_create() -> ptr
    auto* cmapCreateTy = llvm::FunctionType::get(i8PtrTy, {}, false);
    runtimeCmapCreate_ = llvm::Function::Create(cmapCreateTy, llvm::Function::ExternalLinkage,
                                                  "chris_cmap_create", module_.get());

    // chris_cmap_set(ptr handle, ptr key, i64 value) -> void
    auto* cmapSetTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy, i64Ty}, false);
    runtimeCmapSet_ = llvm::Function::Create(cmapSetTy, llvm::Function::ExternalLinkage,
                                               "chris_cmap_set", module_.get());

    // chris_cmap_get(ptr handle, ptr key) -> i64
    auto* cmapGetTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeCmapGet_ = llvm::Function::Create(cmapGetTy, llvm::Function::ExternalLinkage,
                                               "chris_cmap_get", module_.get());

    // chris_cmap_has(ptr handle, ptr key) -> i64
    auto* cmapHasTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeCmapHas_ = llvm::Function::Create(cmapHasTy, llvm::Function::ExternalLinkage,
                                               "chris_cmap_has", module_.get());

    // chris_cmap_delete(ptr handle, ptr key) -> i64
    auto* cmapDeleteTy = llvm::FunctionType::get(i64Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeCmapDelete_ = llvm::Function::Create(cmapDeleteTy, llvm::Function::ExternalLinkage,
                                                  "chris_cmap_delete", module_.get());

    // chris_cmap_size(ptr handle) -> i64
    auto* cmapSizeTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeCmapSize_ = llvm::Function::Create(cmapSizeTy, llvm::Function::ExternalLinkage,
                                                "chris_cmap_size", module_.get());

    // chris_cmap_destroy(ptr handle) -> void
    auto* cmapDestroyTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeCmapDestroy_ = llvm::Function::Create(cmapDestroyTy, llvm::Function::ExternalLinkage,
                                                   "chris_cmap_destroy", module_.get());

    // ConcurrentQueue runtime functions
    // chris_cqueue_create() -> ptr
    auto* cqueueCreateTy = llvm::FunctionType::get(i8PtrTy, {}, false);
    runtimeCqueueCreate_ = llvm::Function::Create(cqueueCreateTy, llvm::Function::ExternalLinkage,
                                                    "chris_cqueue_create", module_.get());

    // chris_cqueue_enqueue(ptr handle, i64 value) -> void
    auto* cqueueEnqueueTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i64Ty}, false);
    runtimeCqueueEnqueue_ = llvm::Function::Create(cqueueEnqueueTy, llvm::Function::ExternalLinkage,
                                                     "chris_cqueue_enqueue", module_.get());

    // chris_cqueue_dequeue(ptr handle) -> i64
    auto* cqueueDequeueTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeCqueueDequeue_ = llvm::Function::Create(cqueueDequeueTy, llvm::Function::ExternalLinkage,
                                                     "chris_cqueue_dequeue", module_.get());

    // chris_cqueue_size(ptr handle) -> i64
    auto* cqueueSizeTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeCqueueSize_ = llvm::Function::Create(cqueueSizeTy, llvm::Function::ExternalLinkage,
                                                  "chris_cqueue_size", module_.get());

    // chris_cqueue_is_empty(ptr handle) -> i64
    auto* cqueueIsEmptyTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeCqueueIsEmpty_ = llvm::Function::Create(cqueueIsEmptyTy, llvm::Function::ExternalLinkage,
                                                     "chris_cqueue_is_empty", module_.get());

    // chris_cqueue_destroy(ptr handle) -> void
    auto* cqueueDestroyTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeCqueueDestroy_ = llvm::Function::Create(cqueueDestroyTy, llvm::Function::ExternalLinkage,
                                                     "chris_cqueue_destroy", module_.get());

    // Atomic runtime functions
    // chris_atomic_create(i64 initial) -> i64
    auto* atomicCreateTy = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    runtimeAtomicCreate_ = llvm::Function::Create(atomicCreateTy, llvm::Function::ExternalLinkage,
                                                    "chris_atomic_create", module_.get());

    // chris_atomic_load(i64 handle) -> i64
    auto* atomicLoadTy = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    runtimeAtomicLoad_ = llvm::Function::Create(atomicLoadTy, llvm::Function::ExternalLinkage,
                                                  "chris_atomic_load", module_.get());

    // chris_atomic_store(i64 handle, i64 value) -> void
    auto* atomicStoreTy = llvm::FunctionType::get(voidTy, {i64Ty, i64Ty}, false);
    runtimeAtomicStore_ = llvm::Function::Create(atomicStoreTy, llvm::Function::ExternalLinkage,
                                                   "chris_atomic_store", module_.get());

    // chris_atomic_add(i64 handle, i64 delta) -> i64
    auto* atomicAddTy = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty}, false);
    runtimeAtomicAdd_ = llvm::Function::Create(atomicAddTy, llvm::Function::ExternalLinkage,
                                                 "chris_atomic_add", module_.get());

    // chris_atomic_sub(i64 handle, i64 delta) -> i64
    auto* atomicSubTy = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty}, false);
    runtimeAtomicSub_ = llvm::Function::Create(atomicSubTy, llvm::Function::ExternalLinkage,
                                                 "chris_atomic_sub", module_.get());

    // chris_atomic_compare_swap(i64 handle, i64 expected, i64 desired) -> i64
    auto* atomicCasTy = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty, i64Ty}, false);
    runtimeAtomicCompareSwap_ = llvm::Function::Create(atomicCasTy, llvm::Function::ExternalLinkage,
                                                         "chris_atomic_compare_swap", module_.get());

    // chris_atomic_destroy(i64 handle) -> void
    auto* atomicDestroyTy = llvm::FunctionType::get(voidTy, {i64Ty}, false);
    runtimeAtomicDestroy_ = llvm::Function::Create(atomicDestroyTy, llvm::Function::ExternalLinkage,
                                                     "chris_atomic_destroy", module_.get());

    // HTTP runtime functions (client)
    // chris_http_get(ptr url) -> ptr
    auto* httpGetTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy}, false);
    runtimeHttpGet_ = llvm::Function::Create(httpGetTy, llvm::Function::ExternalLinkage,
                                               "chris_http_get", module_.get());

    // chris_http_post(ptr url, ptr body, ptr contentType) -> ptr
    auto* httpPostTy = llvm::FunctionType::get(i8PtrTy, {i8PtrTy, i8PtrTy, i8PtrTy}, false);
    runtimeHttpPost_ = llvm::Function::Create(httpPostTy, llvm::Function::ExternalLinkage,
                                                "chris_http_post", module_.get());

    // HTTP runtime functions (server)
    // chris_http_server_create(i64 port) -> i64
    auto* httpServerCreateTy = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    runtimeHttpServerCreate_ = llvm::Function::Create(httpServerCreateTy, llvm::Function::ExternalLinkage,
                                                        "chris_http_server_create", module_.get());

    // chris_http_server_accept(i64 serverFd) -> i64
    auto* httpServerAcceptTy = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    runtimeHttpServerAccept_ = llvm::Function::Create(httpServerAcceptTy, llvm::Function::ExternalLinkage,
                                                        "chris_http_server_accept", module_.get());

    // chris_http_request_method(i64 handle) -> ptr
    auto* httpReqMethodTy = llvm::FunctionType::get(i8PtrTy, {i64Ty}, false);
    runtimeHttpRequestMethod_ = llvm::Function::Create(httpReqMethodTy, llvm::Function::ExternalLinkage,
                                                         "chris_http_request_method", module_.get());

    // chris_http_request_path(i64 handle) -> ptr
    auto* httpReqPathTy = llvm::FunctionType::get(i8PtrTy, {i64Ty}, false);
    runtimeHttpRequestPath_ = llvm::Function::Create(httpReqPathTy, llvm::Function::ExternalLinkage,
                                                       "chris_http_request_path", module_.get());

    // chris_http_request_body(i64 handle) -> ptr
    auto* httpReqBodyTy = llvm::FunctionType::get(i8PtrTy, {i64Ty}, false);
    runtimeHttpRequestBody_ = llvm::Function::Create(httpReqBodyTy, llvm::Function::ExternalLinkage,
                                                       "chris_http_request_body", module_.get());

    // chris_http_respond(i64 handle, i64 status, ptr body) -> void
    auto* httpRespondTy = llvm::FunctionType::get(voidTy, {i64Ty, i64Ty, i8PtrTy}, false);
    runtimeHttpRespond_ = llvm::Function::Create(httpRespondTy, llvm::Function::ExternalLinkage,
                                                   "chris_http_respond", module_.get());

    // chris_http_server_close(i64 serverFd) -> void
    auto* httpServerCloseTy = llvm::FunctionType::get(voidTy, {i64Ty}, false);
    runtimeHttpServerClose_ = llvm::Function::Create(httpServerCloseTy, llvm::Function::ExternalLinkage,
                                                       "chris_http_server_close", module_.get());

    // Set runtime functions
    // chris_set_create() -> ptr
    auto* setCreateTy = llvm::FunctionType::get(i8PtrTy, {}, false);
    runtimeSetCreate_ = llvm::Function::Create(setCreateTy, llvm::Function::ExternalLinkage,
                                                "chris_set_create", module_.get());

    // chris_set_add(ptr set, ptr value) -> void
    auto* setAddTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy}, false);
    runtimeSetAdd_ = llvm::Function::Create(setAddTy, llvm::Function::ExternalLinkage,
                                             "chris_set_add", module_.get());

    // chris_set_has(ptr set, ptr value) -> i32
    auto* setHasTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeSetHas_ = llvm::Function::Create(setHasTy, llvm::Function::ExternalLinkage,
                                             "chris_set_has", module_.get());

    // chris_set_remove(ptr set, ptr value) -> i32
    auto* setRemoveTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeSetRemove_ = llvm::Function::Create(setRemoveTy, llvm::Function::ExternalLinkage,
                                                "chris_set_remove", module_.get());

    // chris_set_size(ptr set) -> i64
    auto* setSizeTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeSetSize_ = llvm::Function::Create(setSizeTy, llvm::Function::ExternalLinkage,
                                              "chris_set_size", module_.get());

    // chris_set_clear(ptr set) -> void
    auto* setClearTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeSetClear_ = llvm::Function::Create(setClearTy, llvm::Function::ExternalLinkage,
                                               "chris_set_clear", module_.get());

    // chris_set_values(ptr set, ptr out_array) -> void
    auto* setValuesTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy}, false);
    runtimeSetValues_ = llvm::Function::Create(setValuesTy, llvm::Function::ExternalLinkage,
                                                "chris_set_values", module_.get());

    // chris_set_destroy(ptr set) -> void
    auto* setDestroyTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeSetDestroy_ = llvm::Function::Create(setDestroyTy, llvm::Function::ExternalLinkage,
                                                 "chris_set_destroy", module_.get());

    // Channel runtime functions
    // chris_channel_create(i64 capacity) -> ptr
    auto* chanCreateTy = llvm::FunctionType::get(i8PtrTy, {i64Ty}, false);
    runtimeChannelCreate_ = llvm::Function::Create(chanCreateTy, llvm::Function::ExternalLinkage,
                                                    "chris_channel_create", module_.get());

    // chris_channel_send(ptr ch, i64 value) -> i32 (1=ok, 0=closed)
    auto* chanSendTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i64Ty}, false);
    runtimeChannelSend_ = llvm::Function::Create(chanSendTy, llvm::Function::ExternalLinkage,
                                                  "chris_channel_send", module_.get());

    // chris_channel_recv(ptr ch, ptr out) -> i32 (1=ok, 0=closed+empty)
    auto* chanRecvTy = llvm::FunctionType::get(i32Ty, {i8PtrTy, i8PtrTy}, false);
    runtimeChannelRecv_ = llvm::Function::Create(chanRecvTy, llvm::Function::ExternalLinkage,
                                                  "chris_channel_recv", module_.get());

    // chris_channel_close(ptr ch) -> void
    auto* chanCloseTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeChannelClose_ = llvm::Function::Create(chanCloseTy, llvm::Function::ExternalLinkage,
                                                   "chris_channel_close", module_.get());

    // chris_channel_destroy(ptr ch) -> void
    auto* chanDestroyTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeChannelDestroy_ = llvm::Function::Create(chanDestroyTy, llvm::Function::ExternalLinkage,
                                                     "chris_channel_destroy", module_.get());

    // Math runtime functions (Int)
    auto* i64_i64Ty = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    auto* i64_i64_i64Ty = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty}, false);
    runtimeMathAbs_ = llvm::Function::Create(i64_i64Ty, llvm::Function::ExternalLinkage,
                                              "chris_math_abs", module_.get());
    runtimeMathMin_ = llvm::Function::Create(i64_i64_i64Ty, llvm::Function::ExternalLinkage,
                                              "chris_math_min", module_.get());
    runtimeMathMax_ = llvm::Function::Create(i64_i64_i64Ty, llvm::Function::ExternalLinkage,
                                              "chris_math_max", module_.get());
    runtimeMathRandom_ = llvm::Function::Create(i64_i64_i64Ty, llvm::Function::ExternalLinkage,
                                                  "chris_math_random", module_.get());

    // Math runtime functions (Float)
    auto* d_dTy = llvm::FunctionType::get(doubleTy, {doubleTy}, false);
    auto* d_d_dTy = llvm::FunctionType::get(doubleTy, {doubleTy, doubleTy}, false);
    runtimeMathSqrt_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                               "chris_math_sqrt", module_.get());
    runtimeMathPow_ = llvm::Function::Create(d_d_dTy, llvm::Function::ExternalLinkage,
                                              "chris_math_pow", module_.get());
    runtimeMathFloor_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                                "chris_math_floor", module_.get());
    runtimeMathCeil_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                               "chris_math_ceil", module_.get());
    runtimeMathRound_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                                "chris_math_round", module_.get());
    runtimeMathLog_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                              "chris_math_log", module_.get());
    runtimeMathSin_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                              "chris_math_sin", module_.get());
    runtimeMathCos_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                              "chris_math_cos", module_.get());
    runtimeMathTan_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                              "chris_math_tan", module_.get());
    runtimeMathFabs_ = llvm::Function::Create(d_dTy, llvm::Function::ExternalLinkage,
                                               "chris_math_fabs", module_.get());
    runtimeMathFmin_ = llvm::Function::Create(d_d_dTy, llvm::Function::ExternalLinkage,
                                               "chris_math_fmin", module_.get());
    runtimeMathFmax_ = llvm::Function::Create(d_d_dTy, llvm::Function::ExternalLinkage,
                                               "chris_math_fmax", module_.get());

    // Stdin: chris_read_line() -> ptr
    auto* readLineTy = llvm::FunctionType::get(i8PtrTy, {}, false);
    runtimeReadLine_ = llvm::Function::Create(readLineTy, llvm::Function::ExternalLinkage,
                                               "chris_read_line", module_.get());

    // Test framework
    // chris_assert(i64 cond, ptr msg, ptr file, i64 line) -> void
    auto* assertTy = llvm::FunctionType::get(voidTy, {i64Ty, i8PtrTy, i8PtrTy, i64Ty}, false);
    runtimeAssert_ = llvm::Function::Create(assertTy, llvm::Function::ExternalLinkage,
                                             "chris_assert", module_.get());

    // chris_assert_equal_int(i64 expected, i64 actual, ptr msg, i64 line) -> void
    auto* assertEqIntTy = llvm::FunctionType::get(voidTy, {i64Ty, i64Ty, i8PtrTy, i64Ty}, false);
    runtimeAssertEqualInt_ = llvm::Function::Create(assertEqIntTy, llvm::Function::ExternalLinkage,
                                                      "chris_assert_equal_int", module_.get());

    // chris_assert_equal_str(ptr expected, ptr actual, ptr msg, i64 line) -> void
    auto* assertEqStrTy = llvm::FunctionType::get(voidTy, {i8PtrTy, i8PtrTy, i8PtrTy, i64Ty}, false);
    runtimeAssertEqualStr_ = llvm::Function::Create(assertEqStrTy, llvm::Function::ExternalLinkage,
                                                      "chris_assert_equal_str", module_.get());

    // JSON runtime functions
    // chris_json_parse(ptr str) -> i64
    auto* jsonParseTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
    runtimeJsonParse_ = llvm::Function::Create(jsonParseTy, llvm::Function::ExternalLinkage,
                                                "chris_json_parse", module_.get());

    // chris_json_get(i64 handle, ptr key) -> ptr
    auto* jsonGetTy = llvm::FunctionType::get(i8PtrTy, {i64Ty, i8PtrTy}, false);
    runtimeJsonGet_ = llvm::Function::Create(jsonGetTy, llvm::Function::ExternalLinkage,
                                              "chris_json_get", module_.get());

    // chris_json_get_int(i64 handle, ptr key) -> i64
    auto* jsonGetIntTy = llvm::FunctionType::get(i64Ty, {i64Ty, i8PtrTy}, false);
    runtimeJsonGetInt_ = llvm::Function::Create(jsonGetIntTy, llvm::Function::ExternalLinkage,
                                                  "chris_json_get_int", module_.get());

    // chris_json_get_bool(i64 handle, ptr key) -> i64
    auto* jsonGetBoolTy = llvm::FunctionType::get(i64Ty, {i64Ty, i8PtrTy}, false);
    runtimeJsonGetBool_ = llvm::Function::Create(jsonGetBoolTy, llvm::Function::ExternalLinkage,
                                                    "chris_json_get_bool", module_.get());

    // chris_json_get_float(i64 handle, ptr key) -> double
    auto* jsonGetFloatTy = llvm::FunctionType::get(doubleTy, {i64Ty, i8PtrTy}, false);
    runtimeJsonGetFloat_ = llvm::Function::Create(jsonGetFloatTy, llvm::Function::ExternalLinkage,
                                                     "chris_json_get_float", module_.get());

    // chris_json_get_array(i64 handle, ptr key) -> i64
    auto* jsonGetArrTy = llvm::FunctionType::get(i64Ty, {i64Ty, i8PtrTy}, false);
    runtimeJsonGetArray_ = llvm::Function::Create(jsonGetArrTy, llvm::Function::ExternalLinkage,
                                                     "chris_json_get_array", module_.get());

    // chris_json_get_object(i64 handle, ptr key) -> i64
    auto* jsonGetObjTy = llvm::FunctionType::get(i64Ty, {i64Ty, i8PtrTy}, false);
    runtimeJsonGetObject_ = llvm::Function::Create(jsonGetObjTy, llvm::Function::ExternalLinkage,
                                                      "chris_json_get_object", module_.get());

    // chris_json_array_length(i64 handle) -> i64
    auto* jsonArrLenTy = llvm::FunctionType::get(i64Ty, {i64Ty}, false);
    runtimeJsonArrayLength_ = llvm::Function::Create(jsonArrLenTy, llvm::Function::ExternalLinkage,
                                                        "chris_json_array_length", module_.get());

    // chris_json_array_get(i64 handle, i64 index) -> i64
    auto* jsonArrGetTy = llvm::FunctionType::get(i64Ty, {i64Ty, i64Ty}, false);
    runtimeJsonArrayGet_ = llvm::Function::Create(jsonArrGetTy, llvm::Function::ExternalLinkage,
                                                     "chris_json_array_get", module_.get());

    // chris_json_stringify(i64 handle) -> ptr
    auto* jsonStringifyTy = llvm::FunctionType::get(i8PtrTy, {i64Ty}, false);
    runtimeJsonStringify_ = llvm::Function::Create(jsonStringifyTy, llvm::Function::ExternalLinkage,
                                                      "chris_json_stringify", module_.get());

    // chris_assert_summary() -> void
    auto* testSummaryTy = llvm::FunctionType::get(voidTy, {}, false);
    runtimeTestSummary_ = llvm::Function::Create(testSummaryTy, llvm::Function::ExternalLinkage,
                                                   "chris_assert_summary", module_.get());

    // chris_assert_exit_code() -> i64
    auto* testExitCodeTy = llvm::FunctionType::get(i64Ty, {}, false);
    runtimeTestExitCode_ = llvm::Function::Create(testExitCodeTy, llvm::Function::ExternalLinkage,
                                                    "chris_assert_exit_code", module_.get());

    // --- GC runtime functions ---

    // chris_gc_init() -> void
    auto* gcInitTy = llvm::FunctionType::get(voidTy, {}, false);
    runtimeGcInit_ = llvm::Function::Create(gcInitTy, llvm::Function::ExternalLinkage,
                                             "chris_gc_init", module_.get());

    // chris_gc_alloc(i64 size, i8 type) -> ptr
    auto* gcAllocTy = llvm::FunctionType::get(i8PtrTy, {i64Ty, llvm::Type::getInt8Ty(*context_)}, false);
    runtimeGcAlloc_ = llvm::Function::Create(gcAllocTy, llvm::Function::ExternalLinkage,
                                              "chris_gc_alloc", module_.get());

    // chris_gc_alloc_with_finalizer(i64 size, i8 type, ptr finalizer) -> ptr
    auto* gcAllocFinTy = llvm::FunctionType::get(i8PtrTy, {i64Ty, llvm::Type::getInt8Ty(*context_), i8PtrTy}, false);
    runtimeGcAllocFinalizer_ = llvm::Function::Create(gcAllocFinTy, llvm::Function::ExternalLinkage,
                                                       "chris_gc_alloc_with_finalizer", module_.get());

    // chris_gc_set_num_pointers(ptr, i16 num_pointers) -> void
    auto* gcSetNumPtrsTy = llvm::FunctionType::get(voidTy, {i8PtrTy, llvm::Type::getInt16Ty(*context_)}, false);
    runtimeGcSetNumPointers_ = llvm::Function::Create(gcSetNumPtrsTy, llvm::Function::ExternalLinkage,
                                                       "chris_gc_set_num_pointers", module_.get());

    // chris_gc_collect() -> void
    auto* gcCollectTy = llvm::FunctionType::get(voidTy, {}, false);
    runtimeGcCollect_ = llvm::Function::Create(gcCollectTy, llvm::Function::ExternalLinkage,
                                                "chris_gc_collect", module_.get());

    // chris_gc_shutdown() -> void
    auto* gcShutdownTy = llvm::FunctionType::get(voidTy, {}, false);
    runtimeGcShutdown_ = llvm::Function::Create(gcShutdownTy, llvm::Function::ExternalLinkage,
                                                  "chris_gc_shutdown", module_.get());

    // chris_gc_push_root(ptr* root) -> void
    auto* gcPushRootTy = llvm::FunctionType::get(voidTy, {i8PtrTy}, false);
    runtimeGcPushRoot_ = llvm::Function::Create(gcPushRootTy, llvm::Function::ExternalLinkage,
                                                  "chris_gc_push_root", module_.get());

    // chris_gc_pop_root() -> void
    auto* gcPopRootTy = llvm::FunctionType::get(voidTy, {}, false);
    runtimeGcPopRoot_ = llvm::Function::Create(gcPopRootTy, llvm::Function::ExternalLinkage,
                                                "chris_gc_pop_root", module_.get());

    // chris_gc_pop_roots(i64 n) -> void
    auto* gcPopRootsTy = llvm::FunctionType::get(voidTy, {i64Ty}, false);
    runtimeGcPopRoots_ = llvm::Function::Create(gcPopRootsTy, llvm::Function::ExternalLinkage,
                                                  "chris_gc_pop_roots", module_.get());
}

bool CodeGen::generate(Program& program,
                       const std::vector<GenericInstantiation>& genericInstantiations) {
    // Pass 0: register class struct types and enum types
    for (auto& decl : program.declarations) {
        if (auto* cls = dynamic_cast<ClassDecl*>(decl.get())) {
            if (!cls->typeParams.empty()) {
                // Generic template — skip struct creation, store for later instantiation
                genericClassDecls_[cls->name] = cls;
                continue;
            }
            ClassInfo info;
            info.structType = llvm::StructType::create(*context_, cls->name);
            info.isShared = cls->isShared;
            classInfos_[cls->name] = info;
        } else if (auto* enm = dynamic_cast<EnumDecl*>(decl.get())) {
            EnumInfo info;
            info.name = enm->name;
            info.cases = enm->cases;
            // Check if any variant has associated values
            for (auto& variant : enm->variants) {
                if (variant.associatedType) {
                    info.hasAssociatedValues = true;
                    info.variantTypeNames[variant.name] = variant.associatedType->toString();
                }
            }
            if (info.hasAssociatedValues) {
                auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                info.structType = llvm::StructType::create(*context_, {i64Ty, i64Ty}, enm->name + "_Enum");
            }
            enumInfos_[enm->name] = info;
        }
    }
    // Then set struct bodies with inherited fields first
    for (auto& decl : program.declarations) {
        if (auto* cls = dynamic_cast<ClassDecl*>(decl.get())) {
            if (!cls->typeParams.empty()) continue; // skip generic templates
            auto& info = classInfos_[cls->name];
            std::vector<llvm::Type*> fieldTypes;

            // Shared classes get a mutex as the first field (64 bytes for pthread_mutex_t)
            if (cls->isShared) {
                auto* i8Ty = llvm::Type::getInt8Ty(*context_);
                auto* mutexTy = llvm::ArrayType::get(i8Ty, 64);
                fieldTypes.push_back(mutexTy);
                // Mutex occupies field index 0; user fields start at index 1
            }

            // Include parent class fields first
            if (!cls->baseClass.empty()) {
                info.parentClass = cls->baseClass;
                auto parentIt = classInfos_.find(cls->baseClass);
                if (parentIt != classInfos_.end()) {
                    for (auto& parentFieldName : parentIt->second.fieldNames) {
                        info.fieldNames.push_back(parentFieldName);
                    }
                    // Copy parent field types
                    for (unsigned i = 0; i < parentIt->second.structType->getNumElements(); i++) {
                        fieldTypes.push_back(parentIt->second.structType->getElementType(i));
                    }
                }
            }

            // Then own fields
            for (auto& field : cls->fields) {
                fieldTypes.push_back(getLLVMType(field->typeAnnotation.get()));
                info.fieldNames.push_back(field->name);
            }
            info.structType->setBody(fieldTypes);

            // Emit global TypeInfo for reflection
            {
                auto* i8PtrTy = llvm::PointerType::getUnqual(*context_);
                auto* i64Ty = llvm::Type::getInt64Ty(*context_);

                // Helper lambda to create a global string constant
                auto makeGlobalStr = [&](const std::string& str, const std::string& name) -> llvm::Constant* {
                    auto* strConst = llvm::ConstantDataArray::getString(*context_, str);
                    auto* strGlobal = new llvm::GlobalVariable(*module_,
                        strConst->getType(), true, llvm::GlobalValue::PrivateLinkage,
                        strConst, name);
                    strGlobal->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
                    return llvm::ConstantExpr::getInBoundsGetElementPtr(
                        strConst->getType(), strGlobal,
                        llvm::ArrayRef<llvm::Constant*>{
                            llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0),
                            llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0)
                        });
                };

                // Create global string for class name
                auto* nameStr = makeGlobalStr(cls->name, "typeinfo.name." + cls->name);

                // Create global array of field name strings
                std::vector<llvm::Constant*> fieldNamePtrs;
                for (auto& fname : info.fieldNames) {
                    fieldNamePtrs.push_back(makeGlobalStr(fname, "typeinfo.field." + cls->name + "." + fname));
                }
                llvm::Constant* fieldsArray = nullptr;
                if (fieldNamePtrs.empty()) {
                    fieldsArray = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8PtrTy));
                } else {
                    auto* arrTy = llvm::ArrayType::get(i8PtrTy, fieldNamePtrs.size());
                    auto* fieldsGlobal = new llvm::GlobalVariable(*module_, arrTy, true,
                        llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantArray::get(arrTy, fieldNamePtrs),
                        "typeinfo.fields." + cls->name);
                    fieldsArray = llvm::ConstantExpr::getBitCast(fieldsGlobal, i8PtrTy);
                }

                // Create global array of interface name strings
                std::vector<llvm::Constant*> ifaceNamePtrs;
                for (auto& iname : cls->interfaces) {
                    ifaceNamePtrs.push_back(makeGlobalStr(iname, "typeinfo.iface." + cls->name + "." + iname));
                }
                llvm::Constant* implArray = nullptr;
                if (ifaceNamePtrs.empty()) {
                    implArray = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8PtrTy));
                } else {
                    auto* arrTy = llvm::ArrayType::get(i8PtrTy, ifaceNamePtrs.size());
                    auto* implGlobal = new llvm::GlobalVariable(*module_, arrTy, true,
                        llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantArray::get(arrTy, ifaceNamePtrs),
                        "typeinfo.impls." + cls->name);
                    implArray = llvm::ConstantExpr::getBitCast(implGlobal, i8PtrTy);
                }

                // Build the TypeInfo struct constant
                auto* tiConst = llvm::ConstantStruct::get(typeInfoStructType_, {
                    nameStr,
                    llvm::ConstantInt::get(i64Ty, info.fieldNames.size()),
                    fieldsArray,
                    llvm::ConstantInt::get(i64Ty, cls->interfaces.size()),
                    implArray
                });

                auto* tiGlobal = new llvm::GlobalVariable(*module_, typeInfoStructType_, true,
                    llvm::GlobalValue::PrivateLinkage, tiConst,
                    "typeinfo." + cls->name);
                typeInfoGlobals_[cls->name] = tiGlobal;
            }
        }
    }

    // First pass: declare all functions (including class methods)
    for (auto& decl : program.declarations) {
        if (auto* func = dynamic_cast<FuncDecl*>(decl.get())) {
            // Build parameter types
            std::vector<llvm::Type*> paramTypes;
            for (auto& param : func->parameters) {
                paramTypes.push_back(getLLVMType(param.type.get()));
            }

            llvm::Type* retType = func->returnType
                ? getLLVMType(func->returnType.get())
                : llvm::Type::getVoidTy(*context_);

            // main() must return i32 for the OS
            if (func->name == "main" && !func->returnType) {
                retType = llvm::Type::getInt32Ty(*context_);
            }

            // Async functions return a Future* (i8 pointer)
            if (func->isAsync) {
                retType = llvm::PointerType::getUnqual(*context_);
            }

            // Functions returning Array<T> return the struct by value
            if (!func->isAsync && func->returnType) {
                auto* named = dynamic_cast<NamedType*>(func->returnType.get());
                if (named && named->name == "Array") {
                    retType = arrayStructType_;
                }
            }

            auto* funcType = llvm::FunctionType::get(retType, paramTypes, false);
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                   func->name, module_.get());
        } else if (auto* ext = dynamic_cast<ExternFuncDecl*>(decl.get())) {
            // Declare extern C function
            std::vector<llvm::Type*> paramTypes;
            for (auto& param : ext->parameters) {
                paramTypes.push_back(getLLVMType(param.type.get()));
            }
            llvm::Type* retType = ext->returnType
                ? getLLVMType(ext->returnType.get())
                : llvm::Type::getVoidTy(*context_);
            auto* funcType = llvm::FunctionType::get(retType, paramTypes, ext->isVariadic);
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                   ext->name, module_.get());
        } else if (auto* cls = dynamic_cast<ClassDecl*>(decl.get())) {
            if (!cls->typeParams.empty()) continue; // skip generic templates
            // Declare class methods as ClassName_methodName with 'this' pointer as first param
            auto it = classInfos_.find(cls->name);
            if (it == classInfos_.end()) continue;
            auto* structPtrTy = llvm::PointerType::getUnqual(*context_);

            // Inherit parent method names (for dispatch)
            if (!cls->baseClass.empty()) {
                auto parentIt = classInfos_.find(cls->baseClass);
                if (parentIt != classInfos_.end()) {
                    for (auto& parentMethod : parentIt->second.methodNames) {
                        // Only inherit if not overridden
                        bool overridden = false;
                        for (auto& method : cls->methods) {
                            if (method->name == parentMethod) { overridden = true; break; }
                        }
                        if (!overridden) {
                            it->second.methodNames.push_back(parentMethod);
                        }
                    }
                }
            }

            for (auto& method : cls->methods) {
                std::vector<llvm::Type*> paramTypes;
                paramTypes.push_back(structPtrTy); // 'this' pointer
                for (auto& param : method->parameters) {
                    paramTypes.push_back(getLLVMType(param.type.get()));
                }

                llvm::Type* retType = method->returnType
                    ? getLLVMType(method->returnType.get())
                    : llvm::Type::getVoidTy(*context_);

                // Constructor returning ClassName -> return ptr to struct
                if (method->name == "new" && method->returnType) {
                    auto* named = dynamic_cast<NamedType*>(method->returnType.get());
                    if (named && classInfos_.count(named->name)) {
                        retType = llvm::PointerType::getUnqual(*context_);
                    }
                }

                std::string mangledName = cls->name + "_" + method->name;
                auto* funcType = llvm::FunctionType::get(retType, paramTypes, false);
                llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                       mangledName, module_.get());
                it->second.methodNames.push_back(method->name);
            }
        }
    }

    // Pass 1.5: emit generic class instantiations (must be before function body emission)
    for (auto& inst : genericInstantiations) {
        auto it = genericClassDecls_.find(inst.templateName);
        if (it == genericClassDecls_.end()) continue;
        emitGenericClassInstance(*it->second, inst.mangledName,
                                 inst.typeParams, inst.typeArgs);
    }

    // Second pass: emit function bodies and class method bodies
    for (auto& decl : program.declarations) {
        if (auto* func = dynamic_cast<FuncDecl*>(decl.get())) {
            emitFuncDecl(*func);
        } else if (auto* cls = dynamic_cast<ClassDecl*>(decl.get())) {
            if (!cls->typeParams.empty()) continue; // skip generic templates
            emitClassDecl(*cls);
        }
    }

    // Verify module
    std::string errStr;
    llvm::raw_string_ostream errStream(errStr);
    if (llvm::verifyModule(*module_, &errStream)) {
        diagnostics_.error("E4001", "LLVM IR verification failed: " + errStr,
                           SourceLocation("<codegen>", 0, 0));
        return false;
    }

    return true;
}

void CodeGen::emitFuncDecl(FuncDecl& func) {
    llvm::Function* llvmFunc = module_->getFunction(func.name);
    if (!llvmFunc) return;

    if (func.isAsync) {
        // --- Async function: create a thunk and spawn it ---
        auto* i8PtrTy = llvm::PointerType::getUnqual(*context_);
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        auto* i32Ty = llvm::Type::getInt32Ty(*context_);

        // 1. Create the thunk function: __async_<name>(void* args) -> i64
        std::string thunkName = "__async_" + func.name;
        auto* thunkFnTy = llvm::FunctionType::get(i64Ty, {i8PtrTy}, false);
        auto* thunkFunc = llvm::Function::Create(thunkFnTy, llvm::Function::InternalLinkage,
                                                   thunkName, module_.get());

        // 2. Emit the thunk body (contains the actual async function logic)
        auto* thunkBB = llvm::BasicBlock::Create(*context_, "entry", thunkFunc);
        builder_->SetInsertPoint(thunkBB);

        auto oldNamedValues = namedValues_;
        namedValues_.clear();

        // Unpack parameters from the args struct (i64* array)
        llvm::Value* argsPtr = &*thunkFunc->arg_begin();
        for (size_t i = 0; i < func.parameters.size(); i++) {
            auto* paramGEP = builder_->CreateGEP(i64Ty, argsPtr,
                llvm::ConstantInt::get(i64Ty, i), "arg.ptr." + func.parameters[i].name);
            auto* rawVal = builder_->CreateLoad(i64Ty, paramGEP, "arg.raw." + func.parameters[i].name);

            // Convert i64 back to the parameter's LLVM type
            llvm::Type* paramTy = getLLVMType(func.parameters[i].type.get());
            llvm::Value* paramVal = rawVal;
            if (paramTy->isPointerTy()) {
                paramVal = builder_->CreateIntToPtr(rawVal, paramTy, "arg." + func.parameters[i].name);
            } else if (paramTy->isDoubleTy()) {
                paramVal = builder_->CreateBitCast(rawVal, paramTy, "arg." + func.parameters[i].name);
            } else if (paramTy->isIntegerTy() && paramTy->getIntegerBitWidth() < 64) {
                paramVal = builder_->CreateTrunc(rawVal, paramTy, "arg." + func.parameters[i].name);
            }

            auto* alloca = createEntryBlockAlloca(thunkFunc, func.parameters[i].name, paramTy);
            builder_->CreateStore(paramVal, alloca);
            namedValues_[func.parameters[i].name] = alloca;
        }

        // Emit function body statements
        for (auto& stmt : func.body->statements) {
            emitStmt(*stmt);
        }

        // Default return if no explicit return
        auto* curBlock = builder_->GetInsertBlock();
        if (curBlock && !curBlock->getTerminator()) {
            builder_->CreateRet(llvm::ConstantInt::get(i64Ty, 0));
        }

        namedValues_ = oldNamedValues;

        // 3. Emit the public async function that spawns the thunk
        auto* entryBB = llvm::BasicBlock::Create(*context_, "entry", llvmFunc);
        builder_->SetInsertPoint(entryBB);

        // Pack arguments into an i64 array on the heap (GC-managed)
        llvm::Value* argsPack = nullptr;
        if (!func.parameters.empty()) {
            auto* allocSize = llvm::ConstantInt::get(i64Ty, func.parameters.size() * 8);
            auto* typeTag = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context_), 2); // GC_ARRAY
            auto* rawMem = builder_->CreateCall(runtimeGcAlloc_, {allocSize, typeTag}, "args.mem");
            argsPack = rawMem;

            size_t idx = 0;
            for (auto& arg : llvmFunc->args()) {
                if (idx >= func.parameters.size()) break;
                llvm::Value* argVal = &arg;
                // Convert to i64 for uniform storage
                llvm::Value* stored;
                if (argVal->getType()->isPointerTy()) {
                    stored = builder_->CreatePtrToInt(argVal, i64Ty, "arg.pack");
                } else if (argVal->getType()->isDoubleTy()) {
                    stored = builder_->CreateBitCast(argVal, i64Ty, "arg.pack");
                } else if (argVal->getType()->isIntegerTy() && argVal->getType()->getIntegerBitWidth() < 64) {
                    stored = builder_->CreateSExt(argVal, i64Ty, "arg.pack");
                } else {
                    stored = argVal;
                }
                auto* slotPtr = builder_->CreateGEP(i64Ty, argsPack,
                    llvm::ConstantInt::get(i64Ty, idx), "arg.slot");
                builder_->CreateStore(stored, slotPtr);
                idx++;
            }
        } else {
            argsPack = llvm::ConstantPointerNull::get(
                llvm::PointerType::getUnqual(*context_));
        }

        // Determine async kind: 0=io, 1=compute
        int asyncKindVal = (func.asyncKind == AsyncKind::Compute) ? 1 : 0;
        auto* kindConst = llvm::ConstantInt::get(i32Ty, asyncKindVal);

        // Spawn: chris_async_spawn(thunk_ptr, args_ptr, kind) -> Future*
        auto* thunkPtr = builder_->CreateBitCast(thunkFunc, i8PtrTy, "thunk.ptr");
        auto* futurePtr = builder_->CreateCall(runtimeAsyncSpawn_,
            {thunkPtr, argsPack, kindConst}, "future");

        builder_->CreateRet(futurePtr);
        return;
    }

    // --- Regular (non-async) function ---
    auto* bb = llvm::BasicBlock::Create(*context_, "entry", llvmFunc);
    builder_->SetInsertPoint(bb);

    // If this is main(), initialize the GC
    bool isMain = (func.name == "main");
    if (isMain) {
        builder_->CreateCall(runtimeGcInit_, {});
    }

    // Save old named values and create new scope
    auto oldNamedValues = namedValues_;
    namedValues_.clear();

    // Create allocas for parameters
    size_t idx = 0;
    for (auto& arg : llvmFunc->args()) {
        if (idx < func.parameters.size()) {
            arg.setName(func.parameters[idx].name);
            // For array params (pointer to array struct), create an alloca of the
            // array struct type and store the struct contents there so that
            // array indexing/length code works uniformly
            bool isArrayParam = false;
            if (func.parameters[idx].type) {
                auto* named = dynamic_cast<NamedType*>(func.parameters[idx].type.get());
                if (named && named->name == "Array") isArrayParam = true;
            }
            if (isArrayParam) {
                auto* arrAlloca = createEntryBlockAlloca(llvmFunc, func.parameters[idx].name, arrayStructType_);
                auto* structVal = builder_->CreateLoad(arrayStructType_, &arg, "arr.load");
                builder_->CreateStore(structVal, arrAlloca);
                namedValues_[func.parameters[idx].name] = arrAlloca;
            } else {
                auto* alloca = createEntryBlockAlloca(llvmFunc, func.parameters[idx].name, arg.getType());
                builder_->CreateStore(&arg, alloca);
                namedValues_[func.parameters[idx].name] = alloca;
            }
        }
        idx++;
    }

    // Emit body
    for (auto& stmt : func.body->statements) {
        emitStmt(*stmt);
    }

    // If the function returns void and the last block doesn't have a terminator, add ret void
    auto* currentBlock = builder_->GetInsertBlock();
    if (currentBlock && !currentBlock->getTerminator()) {
        if (isMain) {
            builder_->CreateCall(runtimeGcShutdown_, {});
        }
        if (llvmFunc->getReturnType()->isVoidTy()) {
            builder_->CreateRetVoid();
        } else {
            // Return default value
            builder_->CreateRet(llvm::Constant::getNullValue(llvmFunc->getReturnType()));
        }
    }

    namedValues_ = oldNamedValues;
}

void CodeGen::emitClassDecl(ClassDecl& cls) {
    auto it = classInfos_.find(cls.name);
    if (it == classInfos_.end()) return;

    auto& info = it->second;

    for (auto& method : cls.methods) {
        std::string mangledName = cls.name + "_" + method->name;
        llvm::Function* llvmFunc = module_->getFunction(mangledName);
        if (!llvmFunc) continue;

        auto* bb = llvm::BasicBlock::Create(*context_, "entry", llvmFunc);
        builder_->SetInsertPoint(bb);

        auto oldNamedValues = namedValues_;
        auto oldThisPtr = thisPtr_;
        auto oldClassName = currentClassName_;
        namedValues_.clear();
        currentClassName_ = cls.name;

        // First arg is 'this' pointer
        auto argIt = llvmFunc->arg_begin();
        argIt->setName("this");
        auto* thisAlloca = createEntryBlockAlloca(llvmFunc, "this.addr",
            llvm::PointerType::getUnqual(*context_));
        builder_->CreateStore(&*argIt, thisAlloca);
        thisPtr_ = thisAlloca;
        ++argIt;

        // Remaining args are method parameters
        size_t idx = 0;
        for (; argIt != llvmFunc->arg_end(); ++argIt, ++idx) {
            if (idx < method->parameters.size()) {
                argIt->setName(method->parameters[idx].name);
                auto* alloca = createEntryBlockAlloca(llvmFunc,
                    method->parameters[idx].name, argIt->getType());
                builder_->CreateStore(&*argIt, alloca);
                namedValues_[method->parameters[idx].name] = alloca;
            }
        }

        // Emit method body
        for (auto& stmt : method->body->statements) {
            emitStmt(*stmt);
        }

        // Add default terminator if needed
        auto* currentBlock = builder_->GetInsertBlock();
        if (currentBlock && !currentBlock->getTerminator()) {
            if (llvmFunc->getReturnType()->isVoidTy()) {
                builder_->CreateRetVoid();
            } else {
                builder_->CreateRet(llvm::Constant::getNullValue(llvmFunc->getReturnType()));
            }
        }

        namedValues_ = oldNamedValues;
        thisPtr_ = oldThisPtr;
        currentClassName_ = oldClassName;
    }
}

// --- Statements ---

void CodeGen::emitStmt(Stmt& stmt) {
    // Don't emit into a block that already has a terminator
    if (builder_->GetInsertBlock() && builder_->GetInsertBlock()->getTerminator()) {
        return;
    }

    if (auto* var = dynamic_cast<VarDecl*>(&stmt)) {
        emitVarDecl(*var);
    } else if (auto* ifStmt = dynamic_cast<IfStmt*>(&stmt)) {
        emitIfStmt(*ifStmt);
    } else if (auto* whileStmt = dynamic_cast<WhileStmt*>(&stmt)) {
        emitWhileStmt(*whileStmt);
    } else if (auto* forStmt = dynamic_cast<ForStmt*>(&stmt)) {
        emitForStmt(*forStmt);
    } else if (auto* retStmt = dynamic_cast<ReturnStmt*>(&stmt)) {
        emitReturnStmt(*retStmt);
    } else if (auto* retStmt2 = dynamic_cast<BreakStmt*>(&stmt)) {
        if (!breakTargets_.empty()) {
            builder_->CreateBr(breakTargets_.back());
        }
    } else if (auto* contStmt = dynamic_cast<ContinueStmt*>(&stmt)) {
        if (!continueTargets_.empty()) {
            builder_->CreateBr(continueTargets_.back());
        }
    } else if (auto* throwStmt = dynamic_cast<ThrowStmt*>(&stmt)) {
        emitThrowStmt(*throwStmt);
    } else if (auto* tryCatch = dynamic_cast<TryCatchStmt*>(&stmt)) {
        emitTryCatchStmt(*tryCatch);
    } else if (auto* exprStmt = dynamic_cast<ExprStmt*>(&stmt)) {
        emitExprStmt(*exprStmt);
    } else if (auto* unsafeBlock = dynamic_cast<UnsafeBlock*>(&stmt)) {
        emitBlock(*unsafeBlock->body);
    } else if (auto* block = dynamic_cast<Block*>(&stmt)) {
        emitBlock(*block);
    }
}

void CodeGen::emitBlock(Block& block) {
    for (auto& stmt : block.statements) {
        emitStmt(*stmt);
    }
}

void CodeGen::emitVarDecl(VarDecl& decl) {
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::Value* initVal = nullptr;

    if (decl.initializer) {
        initVal = emitExpr(*decl.initializer);
    }

    if (!initVal) return;

    // Convert init value to declared type if needed (e.g. i64 -> i8 for Int8)
    if (decl.typeAnnotation) {
        llvm::Type* declTy = getLLVMType(decl.typeAnnotation.get());
        if (declTy != initVal->getType() && !declTy->isVoidTy()) {
            // Integer truncation/extension
            if (declTy->isIntegerTy() && initVal->getType()->isIntegerTy()) {
                unsigned declBits = declTy->getIntegerBitWidth();
                unsigned valBits = initVal->getType()->getIntegerBitWidth();
                if (declBits < valBits) {
                    initVal = builder_->CreateTrunc(initVal, declTy, "trunc");
                } else if (declBits > valBits) {
                    initVal = builder_->CreateSExt(initVal, declTy, "sext");
                }
            }
            // Float64 -> Float32
            if (declTy->isFloatTy() && initVal->getType()->isDoubleTy()) {
                initVal = builder_->CreateFPTrunc(initVal, declTy, "fptrunc");
            }
            // Float32 -> Float64
            if (declTy->isDoubleTy() && initVal->getType()->isFloatTy()) {
                initVal = builder_->CreateFPExt(initVal, declTy, "fpext");
            }
        }
    }

    // If initializer is an array (pointer to Array struct), alias directly
    if (auto* allocaInit = llvm::dyn_cast<llvm::AllocaInst>(initVal)) {
        if (allocaInit->getAllocatedType() == arrayStructType_) {
            namedValues_[decl.name] = allocaInit;

            // Track element type from the array literal or method call
            if (auto* arrLit = dynamic_cast<ArrayLiteralExpr*>(decl.initializer.get())) {
                if (!arrLit->elements.empty()) {
                    llvm::Type* elemTy = llvm::Type::getInt64Ty(*context_);
                    auto& firstElem = arrLit->elements[0];
                    if (dynamic_cast<FloatLiteralExpr*>(firstElem.get())) {
                        elemTy = llvm::Type::getDoubleTy(*context_);
                    } else if (dynamic_cast<StringLiteralExpr*>(firstElem.get()) ||
                               dynamic_cast<StringInterpolationExpr*>(firstElem.get())) {
                        elemTy = llvm::PointerType::getUnqual(*context_);
                    } else if (dynamic_cast<BoolLiteralExpr*>(firstElem.get())) {
                        elemTy = llvm::Type::getInt1Ty(*context_);
                    }
                    varArrayElemType_[decl.name] = elemTy;
                }
            } else if (auto* callExpr = dynamic_cast<CallExpr*>(decl.initializer.get())) {
                // Detect split() -> array of strings, map/filter -> same element type as source
                if (auto* memberCallee = dynamic_cast<MemberExpr*>(callExpr->callee.get())) {
                    if (memberCallee->member == "split") {
                        varArrayElemType_[decl.name] = llvm::PointerType::getUnqual(*context_);
                    } else if (memberCallee->member == "map" || memberCallee->member == "filter") {
                        // Inherit element type from source array
                        if (auto* srcIdent = dynamic_cast<IdentifierExpr*>(memberCallee->object.get())) {
                            auto eit = varArrayElemType_.find(srcIdent->name);
                            if (eit != varArrayElemType_.end()) {
                                varArrayElemType_[decl.name] = eit->second;
                            }
                        }
                    }
                }
            }
        } else {
            auto* alloca = createEntryBlockAlloca(func, decl.name, initVal->getType());
            builder_->CreateStore(initVal, alloca);
            namedValues_[decl.name] = alloca;
        }
    } else {
        auto* alloca = createEntryBlockAlloca(func, decl.name, initVal->getType());
        builder_->CreateStore(initVal, alloca);
        namedValues_[decl.name] = alloca;
    }

    // Track class type for variable (needed for generic method dispatch and typeof)
    if (decl.typeAnnotation) {
        auto* named = dynamic_cast<NamedType*>(decl.typeAnnotation.get());
        if (named) {
            if (!named->typeArgs.empty()) {
                // Generic type: build mangled name e.g. Box<Int>
                std::string mangledName = named->name + "<";
                for (size_t i = 0; i < named->typeArgs.size(); i++) {
                    if (i > 0) mangledName += ", ";
                    mangledName += named->typeArgs[i]->toString();
                }
                mangledName += ">";
                varClassMap_[decl.name] = mangledName;
            } else if (classInfos_.count(named->name)) {
                varClassMap_[decl.name] = named->name;
            }
        }
    }
    // Also infer class from ConstructExpr initializer (e.g. var u = User { ... })
    if (decl.initializer) {
        if (auto* construct = dynamic_cast<ConstructExpr*>(decl.initializer.get())) {
            if (classInfos_.count(construct->className)) {
                varClassMap_[decl.name] = construct->className;
            }
        }
    }

    // Track Set variables (for disambiguating .size from Map.size)
    if (decl.initializer) {
        if (auto* call = dynamic_cast<CallExpr*>(decl.initializer.get())) {
            if (auto* ident = dynamic_cast<IdentifierExpr*>(call->callee.get())) {
                if (ident->name == "Set") {
                    varSetNames_.insert(decl.name);
                }
            }
        }
    }
}

void CodeGen::emitIfStmt(IfStmt& stmt) {
    llvm::Value* condVal = emitExpr(*stmt.condition);
    if (!condVal) return;

    // Convert to i1 if needed
    if (!condVal->getType()->isIntegerTy(1)) {
        condVal = builder_->CreateICmpNE(condVal,
            llvm::Constant::getNullValue(condVal->getType()), "ifcond");
    }

    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    auto* thenBB = llvm::BasicBlock::Create(*context_, "then", func);
    auto* elseBB = llvm::BasicBlock::Create(*context_, "else", func);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "ifcont", func);

    builder_->CreateCondBr(condVal, thenBB, elseBB);

    // Then block
    builder_->SetInsertPoint(thenBB);
    for (auto& s : stmt.thenBlock->statements) {
        emitStmt(*s);
    }
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(mergeBB);
    }

    // Else block
    builder_->SetInsertPoint(elseBB);
    if (stmt.elseBlock) {
        emitStmt(*stmt.elseBlock);
    }
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(mergeBB);
    }

    builder_->SetInsertPoint(mergeBB);
}

void CodeGen::emitWhileStmt(WhileStmt& stmt) {
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    auto* condBB = llvm::BasicBlock::Create(*context_, "whilecond", func);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "whilebody", func);
    auto* afterBB = llvm::BasicBlock::Create(*context_, "whileend", func);

    breakTargets_.push_back(afterBB);
    continueTargets_.push_back(condBB);

    builder_->CreateBr(condBB);

    // Condition
    builder_->SetInsertPoint(condBB);
    llvm::Value* condVal = emitExpr(*stmt.condition);
    if (condVal && !condVal->getType()->isIntegerTy(1)) {
        condVal = builder_->CreateICmpNE(condVal,
            llvm::Constant::getNullValue(condVal->getType()), "whilecond");
    }
    builder_->CreateCondBr(condVal, bodyBB, afterBB);

    // Body
    builder_->SetInsertPoint(bodyBB);
    for (auto& s : stmt.body->statements) {
        emitStmt(*s);
    }
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(condBB);
    }

    breakTargets_.pop_back();
    continueTargets_.pop_back();

    builder_->SetInsertPoint(afterBB);
}

void CodeGen::emitForStmt(ForStmt& stmt) {
    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    // Check if iterating over an array
    auto* rangeExpr = dynamic_cast<RangeExpr*>(stmt.iterable.get());
    if (!rangeExpr) {
        // Array iteration: for elem in arr { ... }
        llvm::Value* arrVal = emitExpr(*stmt.iterable);
        if (!arrVal) return;

        // arrVal is a pointer to Array struct {i64 length, ptr data}
        auto* lenPtr = builder_->CreateStructGEP(arrayStructType_, arrVal, 0, "arr.len.ptr");
        auto* length = builder_->CreateLoad(i64Ty, lenPtr, "arr.len");
        auto* dataFieldPtr = builder_->CreateStructGEP(arrayStructType_, arrVal, 1, "arr.data.ptr");
        auto* dataPtr = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), dataFieldPtr, "arr.data");

        // Determine element type
        llvm::Type* elemType = i64Ty;
        if (auto* ident = dynamic_cast<IdentifierExpr*>(stmt.iterable.get())) {
            auto it = varArrayElemType_.find(ident->name);
            if (it != varArrayElemType_.end()) {
                elemType = it->second;
            }
        }

        // Index counter
        auto* idxVar = createEntryBlockAlloca(func, "__idx", i64Ty);
        builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), idxVar);

        // Loop variable (element value)
        auto* loopVar = createEntryBlockAlloca(func, stmt.variable, elemType);
        namedValues_[stmt.variable] = loopVar;

        auto* condBB = llvm::BasicBlock::Create(*context_, "forcond", func);
        auto* bodyBB = llvm::BasicBlock::Create(*context_, "forbody", func);
        auto* incBB = llvm::BasicBlock::Create(*context_, "forinc", func);
        auto* afterBB = llvm::BasicBlock::Create(*context_, "forend", func);

        breakTargets_.push_back(afterBB);
        continueTargets_.push_back(incBB);

        builder_->CreateBr(condBB);

        // Condition: idx < length
        builder_->SetInsertPoint(condBB);
        auto* curIdx = builder_->CreateLoad(i64Ty, idxVar, "__idx");
        auto* cond = builder_->CreateICmpSLT(curIdx, length, "forcond");
        builder_->CreateCondBr(cond, bodyBB, afterBB);

        // Body: load element, then execute body
        builder_->SetInsertPoint(bodyBB);
        auto* idx = builder_->CreateLoad(i64Ty, idxVar, "__idx");
        auto* elemPtr = builder_->CreateGEP(elemType, dataPtr, idx, "elem.ptr");
        auto* elemVal = builder_->CreateLoad(elemType, elemPtr, "elem");
        builder_->CreateStore(elemVal, loopVar);

        for (auto& s : stmt.body->statements) {
            emitStmt(*s);
        }
        if (!builder_->GetInsertBlock()->getTerminator()) {
            builder_->CreateBr(incBB);
        }

        // Increment index
        builder_->SetInsertPoint(incBB);
        auto* nextIdx = builder_->CreateAdd(
            builder_->CreateLoad(i64Ty, idxVar, "__idx"),
            llvm::ConstantInt::get(i64Ty, 1), "nextidx");
        builder_->CreateStore(nextIdx, idxVar);
        builder_->CreateBr(condBB);

        breakTargets_.pop_back();
        continueTargets_.pop_back();

        builder_->SetInsertPoint(afterBB);
        return;
    }

    // Range-based for loop: for i in start..end { ... }
    llvm::Value* startVal = emitExpr(*rangeExpr->start);
    llvm::Value* endVal = emitExpr(*rangeExpr->end);
    if (!startVal || !endVal) return;

    // Create loop variable alloca
    auto* loopVar = createEntryBlockAlloca(func, stmt.variable, i64Ty);
    builder_->CreateStore(startVal, loopVar);
    namedValues_[stmt.variable] = loopVar;

    auto* condBB = llvm::BasicBlock::Create(*context_, "forcond", func);
    auto* bodyBB = llvm::BasicBlock::Create(*context_, "forbody", func);
    auto* incBB = llvm::BasicBlock::Create(*context_, "forinc", func);
    auto* afterBB = llvm::BasicBlock::Create(*context_, "forend", func);

    breakTargets_.push_back(afterBB);
    continueTargets_.push_back(incBB);

    builder_->CreateBr(condBB);

    // Condition: i < end
    builder_->SetInsertPoint(condBB);
    llvm::Value* curVal = builder_->CreateLoad(i64Ty, loopVar, stmt.variable);
    llvm::Value* cond = builder_->CreateICmpSLT(curVal, endVal, "forcond");
    builder_->CreateCondBr(cond, bodyBB, afterBB);

    // Body
    builder_->SetInsertPoint(bodyBB);
    for (auto& s : stmt.body->statements) {
        emitStmt(*s);
    }
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(incBB);
    }

    // Increment: i = i + 1
    builder_->SetInsertPoint(incBB);
    llvm::Value* nextVal = builder_->CreateAdd(
        builder_->CreateLoad(i64Ty, loopVar, stmt.variable),
        llvm::ConstantInt::get(i64Ty, 1), "nextval");
    builder_->CreateStore(nextVal, loopVar);
    builder_->CreateBr(condBB);

    breakTargets_.pop_back();
    continueTargets_.pop_back();

    builder_->SetInsertPoint(afterBB);
}

void CodeGen::emitReturnStmt(ReturnStmt& stmt) {
    if (stmt.value) {
        llvm::Value* retVal = emitExpr(*stmt.value);
        if (retVal) {
            // Coerce return value to match function return type
            llvm::Type* retTy = builder_->GetInsertBlock()->getParent()->getReturnType();
            if (retTy != retVal->getType() && !retTy->isVoidTy()) {
                if (retTy->isIntegerTy() && retVal->getType()->isIntegerTy()) {
                    unsigned rBits = retTy->getIntegerBitWidth();
                    unsigned vBits = retVal->getType()->getIntegerBitWidth();
                    if (rBits < vBits) retVal = builder_->CreateTrunc(retVal, retTy, "ret.trunc");
                    else if (rBits > vBits) retVal = builder_->CreateSExt(retVal, retTy, "ret.sext");
                } else if (retTy->isFloatTy() && retVal->getType()->isDoubleTy()) {
                    retVal = builder_->CreateFPTrunc(retVal, retTy, "ret.fptrunc");
                } else if (retTy->isDoubleTy() && retVal->getType()->isFloatTy()) {
                    retVal = builder_->CreateFPExt(retVal, retTy, "ret.fpext");
                } else if (retTy == arrayStructType_ && retVal->getType()->isPointerTy()) {
                    // Return array struct by value — load from alloca pointer
                    retVal = builder_->CreateLoad(arrayStructType_, retVal, "ret.arr");
                }
            }
            builder_->CreateRet(retVal);
        }
    } else {
        builder_->CreateRetVoid();
    }
}

void CodeGen::emitExprStmt(ExprStmt& stmt) {
    emitExpr(*stmt.expression);
}

// --- Expressions ---

llvm::Value* CodeGen::emitExpr(Expr& expr) {
    if (auto* e = dynamic_cast<IntLiteralExpr*>(&expr))              return emitIntLiteral(*e);
    if (auto* e = dynamic_cast<FloatLiteralExpr*>(&expr))            return emitFloatLiteral(*e);
    if (auto* e = dynamic_cast<StringLiteralExpr*>(&expr))           return emitStringLiteral(*e);
    if (auto* e = dynamic_cast<CharLiteralExpr*>(&expr))             return llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context_), e->value);
    if (auto* e = dynamic_cast<BoolLiteralExpr*>(&expr))             return emitBoolLiteral(*e);
    if (auto* e = dynamic_cast<NilLiteralExpr*>(&expr))              return emitNilLiteral(*e);
    if (auto* e = dynamic_cast<IdentifierExpr*>(&expr))              return emitIdentifier(*e);
    if (auto* e = dynamic_cast<BinaryExpr*>(&expr))                  return emitBinaryExpr(*e);
    if (auto* e = dynamic_cast<UnaryExpr*>(&expr))                   return emitUnaryExpr(*e);
    if (auto* e = dynamic_cast<CallExpr*>(&expr))                    return emitCallExpr(*e);
    if (auto* e = dynamic_cast<AssignExpr*>(&expr))                  return emitAssignExpr(*e);
    if (auto* e = dynamic_cast<RangeExpr*>(&expr))                   return emitRangeExpr(*e);
    if (auto* e = dynamic_cast<MemberExpr*>(&expr))                  return emitMemberExpr(*e);
    if (auto* e = dynamic_cast<ThisExpr*>(&expr))                    return emitThisExpr(*e);
    if (auto* e = dynamic_cast<ConstructExpr*>(&expr))               return emitConstructExpr(*e);
    if (auto* e = dynamic_cast<StringInterpolationExpr*>(&expr))     return emitStringInterpolation(*e);
    if (auto* e = dynamic_cast<NilCoalesceExpr*>(&expr))            return emitNilCoalesceExpr(*e);
    if (auto* e = dynamic_cast<ForceUnwrapExpr*>(&expr))            return emitForceUnwrapExpr(*e);
    if (auto* e = dynamic_cast<OptionalChainExpr*>(&expr))          return emitOptionalChainExpr(*e);
    if (auto* e = dynamic_cast<MatchExpr*>(&expr))                  return emitMatchExpr(*e);
    if (auto* e = dynamic_cast<LambdaExpr*>(&expr))                 return emitLambdaExpr(*e);
    if (auto* e = dynamic_cast<ArrayLiteralExpr*>(&expr))           return emitArrayLiteralExpr(*e);
    if (auto* e = dynamic_cast<IndexExpr*>(&expr))                  return emitIndexExpr(*e);
    if (auto* e = dynamic_cast<IfExpr*>(&expr))                     return emitIfExpr(*e);
    if (auto* e = dynamic_cast<AwaitExpr*>(&expr))                   return emitAwaitExpr(*e);

    return nullptr;
}

llvm::Value* CodeGen::emitIntLiteral(IntLiteralExpr& expr) {
    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), expr.value, true);
}

llvm::Value* CodeGen::emitFloatLiteral(FloatLiteralExpr& expr) {
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), expr.value);
}

llvm::Value* CodeGen::emitStringLiteral(StringLiteralExpr& expr) {
    return createGlobalString(expr.value);
}

llvm::Value* CodeGen::emitBoolLiteral(BoolLiteralExpr& expr) {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), expr.value ? 1 : 0);
}

llvm::Value* CodeGen::emitNilLiteral(NilLiteralExpr& /*expr*/) {
    return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_));
}

llvm::Value* CodeGen::emitIdentifier(IdentifierExpr& expr) {
    auto it = namedValues_.find(expr.name);
    if (it == namedValues_.end()) {
        // Could be a function name
        llvm::Function* func = module_->getFunction(expr.name);
        if (func) return func;
        return nullptr;
    }
    // Array variables: return alloca directly (it's a struct, not a scalar)
    if (it->second->getAllocatedType() == arrayStructType_) {
        return it->second;
    }
    return builder_->CreateLoad(it->second->getAllocatedType(), it->second, expr.name);
}

llvm::Value* CodeGen::emitBinaryExpr(BinaryExpr& expr) {
    llvm::Value* left = emitExpr(*expr.left);
    llvm::Value* right = emitExpr(*expr.right);
    if (!left || !right) return nullptr;

    // Check for operator overloading on class instances
    if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
        // Determine the class of the left operand
        std::string leftClassName;
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.left.get())) {
            auto vit = varClassMap_.find(ident->name);
            if (vit != varClassMap_.end()) {
                leftClassName = vit->second;
            }
        }

        if (!leftClassName.empty()) {
            std::string opMethodName = "operator" + expr.op;
            std::string mangledName = leftClassName + "_" + opMethodName;
            llvm::Function* opFunc = module_->getFunction(mangledName);
            if (opFunc) {
                std::vector<llvm::Value*> args = {left, right};
                if (opFunc->getReturnType()->isVoidTy()) {
                    builder_->CreateCall(opFunc, args);
                    return nullptr;
                }
                return builder_->CreateCall(opFunc, args, "opcall");
            }
        }
    }

    bool isPtr = left->getType()->isPointerTy() && right->getType()->isPointerTy();

    // Promote Float32 to Float64 if mixed
    if (left->getType()->isFloatTy() && right->getType()->isDoubleTy()) {
        left = builder_->CreateFPExt(left, llvm::Type::getDoubleTy(*context_), "fpext");
    } else if (right->getType()->isFloatTy() && left->getType()->isDoubleTy()) {
        right = builder_->CreateFPExt(right, llvm::Type::getDoubleTy(*context_), "fpext");
    }

    bool isFloat = left->getType()->isDoubleTy() || right->getType()->isDoubleTy();
    bool isFloat32 = left->getType()->isFloatTy() && right->getType()->isFloatTy();

    // Promote int to float if mixed
    if (isFloat) {
        if (left->getType()->isIntegerTy()) {
            left = builder_->CreateSIToFP(left, llvm::Type::getDoubleTy(*context_), "intfloat");
        }
        if (right->getType()->isIntegerTy()) {
            right = builder_->CreateSIToFP(right, llvm::Type::getDoubleTy(*context_), "intfloat");
        }
    } else if (isFloat32) {
        // Both float32, no promotion needed
    } else if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
        // Promote smaller integer to larger
        unsigned lBits = left->getType()->getIntegerBitWidth();
        unsigned rBits = right->getType()->getIntegerBitWidth();
        if (lBits < rBits) {
            left = builder_->CreateSExt(left, right->getType(), "sext.binop");
        } else if (rBits < lBits) {
            right = builder_->CreateSExt(right, left->getType(), "sext.binop");
        }
    }

    // String concatenation
    if (isPtr && expr.op == "+") {
        return emitStringConcat(left, right);
    }

    bool anyFloat = isFloat || isFloat32;

    // Arithmetic
    if (expr.op == "+") return anyFloat ? builder_->CreateFAdd(left, right, "addtmp") : builder_->CreateAdd(left, right, "addtmp");
    if (expr.op == "-") return anyFloat ? builder_->CreateFSub(left, right, "subtmp") : builder_->CreateSub(left, right, "subtmp");
    if (expr.op == "*") return anyFloat ? builder_->CreateFMul(left, right, "multmp") : builder_->CreateMul(left, right, "multmp");
    if (expr.op == "/") return anyFloat ? builder_->CreateFDiv(left, right, "divtmp") : builder_->CreateSDiv(left, right, "divtmp");
    if (expr.op == "%") return builder_->CreateSRem(left, right, "modtmp");

    // Comparison
    if (expr.op == "<")  return anyFloat ? builder_->CreateFCmpOLT(left, right, "cmptmp") : builder_->CreateICmpSLT(left, right, "cmptmp");
    if (expr.op == ">")  return anyFloat ? builder_->CreateFCmpOGT(left, right, "cmptmp") : builder_->CreateICmpSGT(left, right, "cmptmp");
    if (expr.op == "<=") return anyFloat ? builder_->CreateFCmpOLE(left, right, "cmptmp") : builder_->CreateICmpSLE(left, right, "cmptmp");
    if (expr.op == ">=") return anyFloat ? builder_->CreateFCmpOGE(left, right, "cmptmp") : builder_->CreateICmpSGE(left, right, "cmptmp");
    if (expr.op == "==") return anyFloat ? builder_->CreateFCmpOEQ(left, right, "cmptmp") : builder_->CreateICmpEQ(left, right, "cmptmp");
    if (expr.op == "!=") return anyFloat ? builder_->CreateFCmpONE(left, right, "cmptmp") : builder_->CreateICmpNE(left, right, "cmptmp");

    // Logical
    if (expr.op == "&&") return builder_->CreateAnd(left, right, "andtmp");
    if (expr.op == "||") return builder_->CreateOr(left, right, "ortmp");

    return nullptr;
}

llvm::Value* CodeGen::emitUnaryExpr(UnaryExpr& expr) {
    llvm::Value* operand = emitExpr(*expr.operand);
    if (!operand) return nullptr;

    if (expr.op == "-") {
        if (operand->getType()->isDoubleTy()) {
            return builder_->CreateFNeg(operand, "negtmp");
        }
        return builder_->CreateNeg(operand, "negtmp");
    }
    if (expr.op == "!") {
        return builder_->CreateNot(operand, "nottmp");
    }

    return nullptr;
}

llvm::Value* CodeGen::emitCallExpr(CallExpr& expr) {
    // Check if this is a method call: obj.method(args) or ClassName.new(args)
    if (auto* memberCallee = dynamic_cast<MemberExpr*>(expr.callee.get())) {
        // Enum variant construction with associated value: Result.Ok(42)
        if (auto* enumIdent = dynamic_cast<IdentifierExpr*>(memberCallee->object.get())) {
            auto eit = enumInfos_.find(enumIdent->name);
            if (eit != enumInfos_.end() && eit->second.hasAssociatedValues && eit->second.structType) {
                auto& enumInfo = eit->second;
                for (size_t i = 0; i < enumInfo.cases.size(); i++) {
                    if (enumInfo.cases[i] == memberCallee->member && expr.arguments.size() >= 1) {
                        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                        auto* alloca = builder_->CreateAlloca(enumInfo.structType, nullptr, "enum.tmp");
                        auto* tagPtr = builder_->CreateStructGEP(enumInfo.structType, alloca, 0, "enum.tag.ptr");
                        builder_->CreateStore(llvm::ConstantInt::get(i64Ty, i), tagPtr);
                        auto* valPtr = builder_->CreateStructGEP(enumInfo.structType, alloca, 1, "enum.val.ptr");
                        llvm::Value* argVal = emitExpr(*expr.arguments[0]);
                        if (!argVal) return nullptr;
                        // Bitcast to i64 for storage
                        llvm::Value* stored;
                        if (argVal->getType()->isDoubleTy()) {
                            stored = builder_->CreateBitCast(argVal, i64Ty);
                        } else if (argVal->getType()->isPointerTy()) {
                            stored = builder_->CreatePtrToInt(argVal, i64Ty);
                        } else {
                            stored = argVal;
                        }
                        builder_->CreateStore(stored, valPtr);
                        return builder_->CreateLoad(enumInfo.structType, alloca, "enum.val");
                    }
                }
            }
        }

        // Array methods: push, pop, reverse, join, map, filter, forEach
        {
            const std::string& method = memberCallee->member;
            if (method == "push" || method == "pop" || method == "reverse" ||
                method == "join" || method == "map" || method == "filter" ||
                method == "forEach") {
                // Get the array alloca pointer (not the loaded value)
                if (auto* arrIdent = dynamic_cast<IdentifierExpr*>(memberCallee->object.get())) {
                    auto it = namedValues_.find(arrIdent->name);
                    if (it != namedValues_.end() && it->second->getAllocatedType() == arrayStructType_) {
                        llvm::Value* arrPtr = it->second;
                        auto* i8PtrTy = llvm::PointerType::getUnqual(*context_);
                        auto* i64Ty = llvm::Type::getInt64Ty(*context_);

                        // Determine element size
                        llvm::Type* elemType = i64Ty; // default
                        auto eit = varArrayElemType_.find(arrIdent->name);
                        if (eit != varArrayElemType_.end()) elemType = eit->second;
                        auto* elemSize = llvm::ConstantInt::get(i64Ty,
                            module_->getDataLayout().getTypeAllocSize(elemType));

                        if (method == "push" && expr.arguments.size() >= 1) {
                            llvm::Value* val = emitExpr(*expr.arguments[0]);
                            if (!val) return nullptr;
                            // Bitcast value to i64 for uniform storage
                            llvm::Value* valI64;
                            if (val->getType()->isDoubleTy()) {
                                valI64 = builder_->CreateBitCast(val, i64Ty);
                            } else if (val->getType()->isPointerTy()) {
                                valI64 = builder_->CreatePtrToInt(val, i64Ty);
                            } else {
                                valI64 = val;
                            }
                            builder_->CreateCall(runtimeArrayPush_, {arrPtr, elemSize, valI64});
                            return nullptr;
                        }
                        if (method == "pop") {
                            auto* rawVal = builder_->CreateCall(runtimeArrayPop_, {arrPtr, elemSize}, "arr.pop");
                            // Cast back to the element type
                            if (elemType->isDoubleTy()) {
                                return builder_->CreateBitCast(rawVal, elemType);
                            } else if (elemType->isPointerTy()) {
                                return builder_->CreateIntToPtr(rawVal, elemType);
                            }
                            return rawVal;
                        }
                        if (method == "reverse") {
                            builder_->CreateCall(runtimeArrayReverse_, {arrPtr, elemSize});
                            return arrPtr; // return same array
                        }
                        if (method == "join" && expr.arguments.size() >= 1) {
                            llvm::Value* sep = emitExpr(*expr.arguments[0]);
                            if (!sep) return nullptr;
                            return builder_->CreateCall(runtimeArrayJoin_, {arrPtr, sep}, "arr.join");
                        }
                        if ((method == "map" || method == "filter" || method == "forEach") &&
                            expr.arguments.size() >= 1) {
                            // Set lambda param type hint from array element type
                            lambdaParamTypeHint_ = elemType;
                            llvm::Value* callback = emitExpr(*expr.arguments[0]);
                            lambdaParamTypeHint_ = nullptr;
                            if (!callback) return nullptr;
                            if (method == "map") {
                                auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "map.arr");
                                builder_->CreateCall(runtimeArrayMap_, {arrPtr, elemSize, callback, outArr});
                                return outArr;
                            }
                            if (method == "filter") {
                                auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "filter.arr");
                                builder_->CreateCall(runtimeArrayFilter_, {arrPtr, elemSize, callback, outArr});
                                return outArr;
                            }
                            if (method == "forEach") {
                                builder_->CreateCall(runtimeArrayForEach_, {arrPtr, elemSize, callback});
                                return nullptr;
                            }
                        }
                    }
                }
            }
        }

        // Map methods: set, get, has, delete, keys
        {
            const std::string& method = memberCallee->member;
            if (method == "set" || method == "get" || method == "has" ||
                method == "delete" || method == "keys") {
                if (auto* mapIdent = dynamic_cast<IdentifierExpr*>(memberCallee->object.get())) {
                    auto it = namedValues_.find(mapIdent->name);
                    if (it != namedValues_.end()) {
                        llvm::Value* mapPtr = builder_->CreateLoad(
                            llvm::PointerType::getUnqual(*context_), it->second, "map.ptr");

                        if (method == "set" && expr.arguments.size() >= 2) {
                            llvm::Value* key = emitExpr(*expr.arguments[0]);
                            llvm::Value* val = emitExpr(*expr.arguments[1]);
                            if (!key || !val) return nullptr;
                            // Convert value to i64 for storage
                            auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                            llvm::Value* valI64;
                            if (val->getType()->isPointerTy()) {
                                valI64 = builder_->CreatePtrToInt(val, i64Ty);
                            } else if (val->getType()->isDoubleTy()) {
                                valI64 = builder_->CreateBitCast(val, i64Ty);
                            } else {
                                valI64 = val;
                            }
                            builder_->CreateCall(runtimeMapSet_, {mapPtr, key, valI64});
                            return nullptr;
                        }
                        if (method == "get" && expr.arguments.size() >= 1) {
                            llvm::Value* key = emitExpr(*expr.arguments[0]);
                            if (!key) return nullptr;
                            return builder_->CreateCall(runtimeMapGet_, {mapPtr, key}, "map.get");
                        }
                        if (method == "has" && expr.arguments.size() >= 1) {
                            llvm::Value* key = emitExpr(*expr.arguments[0]);
                            if (!key) return nullptr;
                            auto* result = builder_->CreateCall(runtimeMapHas_, {mapPtr, key}, "map.has");
                            // Convert i32 to i1 for bool
                            return builder_->CreateICmpNE(result,
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "map.has.bool");
                        }
                        if (method == "delete" && expr.arguments.size() >= 1) {
                            llvm::Value* key = emitExpr(*expr.arguments[0]);
                            if (!key) return nullptr;
                            auto* result = builder_->CreateCall(runtimeMapDelete_, {mapPtr, key}, "map.del");
                            return builder_->CreateICmpNE(result,
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "map.del.bool");
                        }
                        if (method == "keys") {
                            auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "map.keys.arr");
                            builder_->CreateCall(runtimeMapKeys_, {mapPtr, outArr});
                            return outArr;
                        }
                    }
                }
            }
        }

        // Set methods: add, has, remove, size, clear, values
        {
            const std::string& method = memberCallee->member;
            if (method == "add" || method == "has" || method == "remove" ||
                method == "size" || method == "clear" || method == "values") {
                if (auto* setIdent = dynamic_cast<IdentifierExpr*>(memberCallee->object.get())) {
                    auto it = namedValues_.find(setIdent->name);
                    if (it != namedValues_.end()) {
                        llvm::Value* setPtr = builder_->CreateLoad(
                            llvm::PointerType::getUnqual(*context_), it->second, "set.ptr");

                        if (method == "add" && expr.arguments.size() >= 1) {
                            llvm::Value* val = emitExpr(*expr.arguments[0]);
                            if (!val) return nullptr;
                            builder_->CreateCall(runtimeSetAdd_, {setPtr, val});
                            return nullptr;
                        }
                        if (method == "has" && expr.arguments.size() >= 1) {
                            llvm::Value* val = emitExpr(*expr.arguments[0]);
                            if (!val) return nullptr;
                            auto* result = builder_->CreateCall(runtimeSetHas_, {setPtr, val}, "set.has");
                            return builder_->CreateICmpNE(result,
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "set.has.bool");
                        }
                        if (method == "remove" && expr.arguments.size() >= 1) {
                            llvm::Value* val = emitExpr(*expr.arguments[0]);
                            if (!val) return nullptr;
                            auto* result = builder_->CreateCall(runtimeSetRemove_, {setPtr, val}, "set.rm");
                            return builder_->CreateICmpNE(result,
                                llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "set.rm.bool");
                        }
                        if (method == "size") {
                            return builder_->CreateCall(runtimeSetSize_, {setPtr}, "set.size");
                        }
                        if (method == "clear") {
                            builder_->CreateCall(runtimeSetClear_, {setPtr});
                            return nullptr;
                        }
                        if (method == "values") {
                            auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "set.vals.arr");
                            builder_->CreateCall(runtimeSetValues_, {setPtr, outArr});
                            return outArr;
                        }
                    }
                }
            }
        }

        // Primitive type conversion methods and String methods
        {
            const std::string& method = memberCallee->member;
            if (method == "toString" || method == "toInt" || method == "toFloat" || method == "toChar" ||
                method == "length" || method == "contains" || method == "startsWith" ||
                method == "endsWith" || method == "indexOf" || method == "substring" ||
                method == "replace" || method == "trim" || method == "toUpper" ||
                method == "toLower" || method == "split" || method == "charAt") {
                llvm::Value* objVal = emitExpr(*memberCallee->object);
                if (objVal) {
                    llvm::Type* objTy = objVal->getType();

                    // Int methods
                    if (objTy->isIntegerTy(64)) {
                        if (method == "toString") return emitIntToString(objVal);
                        if (method == "toFloat") return builder_->CreateSIToFP(
                            objVal, llvm::Type::getDoubleTy(*context_), "int2float");
                        if (method == "toChar") return builder_->CreateTrunc(
                            objVal, llvm::Type::getInt8Ty(*context_), "int2char");
                    }
                    // Sized integer methods (i16, i32 — excludes i8/Char and i1/Bool)
                    if (objTy->isIntegerTy() && !objTy->isIntegerTy(64) &&
                        !objTy->isIntegerTy(8) && !objTy->isIntegerTy(1)) {
                        if (method == "toString") {
                            auto* ext = builder_->CreateSExt(objVal, llvm::Type::getInt64Ty(*context_), "sext.str");
                            return emitIntToString(ext);
                        }
                        if (method == "toInt") return builder_->CreateSExt(
                            objVal, llvm::Type::getInt64Ty(*context_), "sized2int");
                        if (method == "toFloat") {
                            auto* ext = builder_->CreateSExt(objVal, llvm::Type::getInt64Ty(*context_), "sext.flt");
                            return builder_->CreateSIToFP(ext, llvm::Type::getDoubleTy(*context_), "sized2float");
                        }
                    }
                    // Char methods (i8)
                    if (objTy->isIntegerTy(8)) {
                        if (method == "toString") return builder_->CreateCall(runtimeCharToStr_, {objVal}, "char.str");
                        if (method == "toInt") return builder_->CreateSExt(
                            objVal, llvm::Type::getInt64Ty(*context_), "char2int");
                    }
                    // Float methods
                    if (objTy->isDoubleTy()) {
                        if (method == "toString") return emitFloatToString(objVal);
                        if (method == "toInt") return builder_->CreateFPToSI(
                            objVal, llvm::Type::getInt64Ty(*context_), "float2int");
                    }
                    // Float32 methods
                    if (objTy->isFloatTy()) {
                        if (method == "toString") {
                            auto* ext = builder_->CreateFPExt(objVal, llvm::Type::getDoubleTy(*context_), "fpext.str");
                            return emitFloatToString(ext);
                        }
                        if (method == "toFloat") return builder_->CreateFPExt(
                            objVal, llvm::Type::getDoubleTy(*context_), "f32tof64");
                        if (method == "toInt") {
                            auto* ext = builder_->CreateFPExt(objVal, llvm::Type::getDoubleTy(*context_), "fpext.int");
                            return builder_->CreateFPToSI(ext, llvm::Type::getInt64Ty(*context_), "f32toint");
                        }
                    }
                    // Bool methods
                    if (objTy->isIntegerTy(1)) {
                        if (method == "toString") return emitBoolToString(objVal);
                    }
                    // String methods
                    if (objTy->isPointerTy()) {
                        if (method == "toInt") {
                            return builder_->CreateCall(runtimeStrToInt_, {objVal}, "str2int");
                        }
                        if (method == "toFloat") {
                            return builder_->CreateCall(runtimeStrToFloat_, {objVal}, "str2float");
                        }
                        if (method == "length") {
                            return builder_->CreateCall(runtimeStrLen_, {objVal}, "str.len");
                        }
                        if (method == "trim") {
                            return builder_->CreateCall(runtimeStrTrim_, {objVal}, "str.trim");
                        }
                        if (method == "toUpper") {
                            return builder_->CreateCall(runtimeStrToUpper_, {objVal}, "str.upper");
                        }
                        if (method == "toLower") {
                            return builder_->CreateCall(runtimeStrToLower_, {objVal}, "str.lower");
                        }
                        // Methods with 1 string arg
                        if (method == "contains" || method == "startsWith" ||
                            method == "endsWith" || method == "indexOf") {
                            if (expr.arguments.size() >= 1) {
                                llvm::Value* arg = emitExpr(*expr.arguments[0]);
                                if (!arg) return nullptr;
                                if (method == "contains")
                                    return builder_->CreateCall(runtimeStrContains_, {objVal, arg}, "str.contains");
                                if (method == "startsWith")
                                    return builder_->CreateCall(runtimeStrStartsWith_, {objVal, arg}, "str.startsWith");
                                if (method == "endsWith")
                                    return builder_->CreateCall(runtimeStrEndsWith_, {objVal, arg}, "str.endsWith");
                                if (method == "indexOf")
                                    return builder_->CreateCall(runtimeStrIndexOf_, {objVal, arg}, "str.indexOf");
                            }
                        }
                        // substring(start, end)
                        if (method == "substring" && expr.arguments.size() >= 2) {
                            llvm::Value* start = emitExpr(*expr.arguments[0]);
                            llvm::Value* end = emitExpr(*expr.arguments[1]);
                            if (!start || !end) return nullptr;
                            return builder_->CreateCall(runtimeStrSubstring_, {objVal, start, end}, "str.sub");
                        }
                        // replace(old, new)
                        if (method == "replace" && expr.arguments.size() >= 2) {
                            llvm::Value* oldStr = emitExpr(*expr.arguments[0]);
                            llvm::Value* newStr = emitExpr(*expr.arguments[1]);
                            if (!oldStr || !newStr) return nullptr;
                            return builder_->CreateCall(runtimeStrReplace_, {objVal, oldStr, newStr}, "str.replace");
                        }
                        // charAt(index)
                        if (method == "charAt" && expr.arguments.size() >= 1) {
                            llvm::Value* idx = emitExpr(*expr.arguments[0]);
                            if (!idx) return nullptr;
                            return builder_->CreateCall(runtimeStrCharAt_, {objVal, idx}, "str.charAt");
                        }
                        // split(delim) -> Array struct
                        if (method == "split" && expr.arguments.size() >= 1) {
                            llvm::Value* delim = emitExpr(*expr.arguments[0]);
                            if (!delim) return nullptr;
                            auto* alloca = builder_->CreateAlloca(arrayStructType_, nullptr, "split.arr");
                            builder_->CreateCall(runtimeStrSplit_, {objVal, delim, alloca});
                            return alloca;
                        }
                    }
                }
            }
        }

        // Check for static constructor call: ClassName.new(args)
        auto* identObj = dynamic_cast<IdentifierExpr*>(memberCallee->object.get());
        if (identObj) {
            // Collect candidate class names: exact match, then generic instantiations
            std::vector<std::string> candidates;
            if (classInfos_.count(identObj->name)) {
                candidates.push_back(identObj->name);
            }
            if (genericClassDecls_.count(identObj->name)) {
                for (auto& [name, info] : classInfos_) {
                    if (name.size() > identObj->name.size() &&
                        name.substr(0, identObj->name.size()) == identObj->name &&
                        name[identObj->name.size()] == '<') {
                        candidates.push_back(name);
                    }
                }
            }

            // Evaluate arguments once
            std::vector<llvm::Value*> argVals;
            for (auto& argExpr : expr.arguments) {
                llvm::Value* argVal = emitExpr(*argExpr);
                if (!argVal) return nullptr;
                argVals.push_back(argVal);
            }

            // Try each candidate — match by argument types
            for (auto& candidateName : candidates) {
                std::string mangledName = candidateName + "_" + memberCallee->member;
                llvm::Function* methodFunc = module_->getFunction(mangledName);
                if (!methodFunc) continue;

                // Check argument types match (skip 'this' param at index 0)
                bool typesMatch = true;
                if (methodFunc->arg_size() != argVals.size() + 1) { typesMatch = false; }
                else {
                    for (size_t i = 0; i < argVals.size(); i++) {
                        auto* paramType = methodFunc->getFunctionType()->getParamType(i + 1);
                        if (argVals[i]->getType() != paramType) {
                            typesMatch = false;
                            break;
                        }
                    }
                }
                if (!typesMatch && candidates.size() > 1) continue;

                // For constructors (method named "new"), allocate the object first
                bool isConstructor = (memberCallee->member == "new");
                llvm::Value* thisArg = nullptr;
                if (isConstructor) {
                    auto& info = classInfos_[candidateName];
                    auto* structTy = info.structType;
                    const auto& dataLayout = module_->getDataLayout();
                    uint64_t structSize = dataLayout.getTypeAllocSize(structTy);
                    auto* sizeVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), structSize);
                    auto* typeTag = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context_), 1); // GC_OBJECT
                    thisArg = builder_->CreateCall(runtimeGcAlloc_, {sizeVal, typeTag}, "obj.alloc");
                } else {
                    thisArg = llvm::ConstantPointerNull::get(
                        llvm::PointerType::getUnqual(*context_));
                }

                std::vector<llvm::Value*> args;
                args.push_back(thisArg);
                args.insert(args.end(), argVals.begin(), argVals.end());

                if (isConstructor) {
                    builder_->CreateCall(methodFunc, args);
                    // Constructor returns the allocated object
                    return thisArg;
                }
                if (methodFunc->getReturnType()->isVoidTy()) {
                    builder_->CreateCall(methodFunc, args);
                    return nullptr;
                }
                return builder_->CreateCall(methodFunc, args, "constructcall");
            }
        }

        // Instance method call: obj.method(args)
        llvm::Value* objPtr = emitExpr(*memberCallee->object);
        if (!objPtr) return nullptr;

        // Try to resolve the class from variable-to-class mapping first
        std::string targetClassName;
        if (auto* objIdent = dynamic_cast<IdentifierExpr*>(memberCallee->object.get())) {
            auto vit = varClassMap_.find(objIdent->name);
            if (vit != varClassMap_.end()) {
                targetClassName = vit->second;
            }
        }
        // Also try currentClassName_ for 'this' member access inside methods
        if (targetClassName.empty() && !currentClassName_.empty() &&
            dynamic_cast<ThisExpr*>(memberCallee->object.get())) {
            targetClassName = currentClassName_;
        }

        // If we know the target class, dispatch directly
        if (!targetClassName.empty()) {
            std::string mangledName = targetClassName + "_" + memberCallee->member;
            llvm::Function* methodFunc = module_->getFunction(mangledName);
            if (methodFunc) {
                std::vector<llvm::Value*> args;
                args.push_back(objPtr);
                for (auto& argExpr : expr.arguments) {
                    llvm::Value* argVal = emitExpr(*argExpr);
                    if (!argVal) return nullptr;
                    args.push_back(argVal);
                }
                if (methodFunc->getReturnType()->isVoidTy()) {
                    builder_->CreateCall(methodFunc, args);
                    return nullptr;
                }
                return builder_->CreateCall(methodFunc, args, "methodcall");
            }
        }

        // Fallback: find the class and mangled method name by scanning all classes
        for (auto& [className, info] : classInfos_) {
            for (auto& methodName : info.methodNames) {
                if (methodName == memberCallee->member) {
                    std::string mangledName = className + "_" + methodName;
                    llvm::Function* methodFunc = module_->getFunction(mangledName);
                    if (!methodFunc) continue;

                    std::vector<llvm::Value*> args;
                    args.push_back(objPtr); // 'this' pointer
                    for (auto& argExpr : expr.arguments) {
                        llvm::Value* argVal = emitExpr(*argExpr);
                        if (!argVal) return nullptr;
                        args.push_back(argVal);
                    }

                    if (methodFunc->getReturnType()->isVoidTy()) {
                        builder_->CreateCall(methodFunc, args);
                        return nullptr;
                    }
                    return builder_->CreateCall(methodFunc, args, "methodcall");
                }
            }
        }
        return nullptr;
    }

    // Get the callee
    auto* identCallee = dynamic_cast<IdentifierExpr*>(expr.callee.get());
    if (!identCallee) return nullptr;

    // Special handling for print()
    if (identCallee->name == "print") {
        if (expr.arguments.size() != 1) return nullptr;
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;

        // If the argument is not a string (pointer), convert it
        if (arg->getType()->isIntegerTy(64)) {
            arg = emitIntToString(arg);
        } else if (arg->getType()->isIntegerTy(8)) {
            // Char (i8) — convert to single-character string
            arg = builder_->CreateCall(runtimeCharToStr_, {arg}, "char.str");
        } else if (arg->getType()->isIntegerTy() && !arg->getType()->isIntegerTy(1)) {
            // Sized integers (i16, i32) — extend to i64 then convert
            arg = builder_->CreateSExt(arg, llvm::Type::getInt64Ty(*context_), "sext.print");
            arg = emitIntToString(arg);
        } else if (arg->getType()->isDoubleTy()) {
            arg = emitFloatToString(arg);
        } else if (arg->getType()->isFloatTy()) {
            // Float32 — extend to double then convert
            arg = builder_->CreateFPExt(arg, llvm::Type::getDoubleTy(*context_), "fpext.print");
            arg = emitFloatToString(arg);
        } else if (arg->getType()->isIntegerTy(1)) {
            arg = emitBoolToString(arg);
        }

        builder_->CreateCall(runtimePrint_, {arg});
        return nullptr;
    }

    // Built-in Map() constructor
    if (identCallee->name == "Map") {
        return builder_->CreateCall(runtimeMapCreate_, {}, "map.new");
    }

    // Built-in Set() constructor
    if (identCallee->name == "Set") {
        return builder_->CreateCall(runtimeSetCreate_, {}, "set.new");
    }

    // Built-in typeof() for reflection
    if (identCallee->name == "typeof" && expr.arguments.size() >= 1) {
        // Determine the class name of the argument
        // If the argument is an identifier, look up its class from varClassMap_
        if (auto* argIdent = dynamic_cast<IdentifierExpr*>(expr.arguments[0].get())) {
            auto vcIt = varClassMap_.find(argIdent->name);
            if (vcIt != varClassMap_.end()) {
                auto tiIt = typeInfoGlobals_.find(vcIt->second);
                if (tiIt != typeInfoGlobals_.end()) {
                    return tiIt->second;
                }
            }
        }
        // If it's a ConstructExpr, use the class name directly
        if (auto* construct = dynamic_cast<ConstructExpr*>(expr.arguments[0].get())) {
            auto tiIt = typeInfoGlobals_.find(construct->className);
            if (tiIt != typeInfoGlobals_.end()) {
                // Still emit the construct expression (for side effects), then return TypeInfo
                emitExpr(*expr.arguments[0]);
                return tiIt->second;
            }
        }
        // Fallback: emit argument and return null
        emitExpr(*expr.arguments[0]);
        return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_));
    }

    // Built-in math functions (Int)
    if (identCallee->name == "abs" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathAbs_, {arg}, "math.abs");
    }
    if (identCallee->name == "min" && expr.arguments.size() >= 2) {
        llvm::Value* a = emitExpr(*expr.arguments[0]);
        llvm::Value* b = emitExpr(*expr.arguments[1]);
        if (!a || !b) return nullptr;
        return builder_->CreateCall(runtimeMathMin_, {a, b}, "math.min");
    }
    if (identCallee->name == "max" && expr.arguments.size() >= 2) {
        llvm::Value* a = emitExpr(*expr.arguments[0]);
        llvm::Value* b = emitExpr(*expr.arguments[1]);
        if (!a || !b) return nullptr;
        return builder_->CreateCall(runtimeMathMax_, {a, b}, "math.max");
    }
    if (identCallee->name == "random" && expr.arguments.size() >= 2) {
        llvm::Value* lo = emitExpr(*expr.arguments[0]);
        llvm::Value* hi = emitExpr(*expr.arguments[1]);
        if (!lo || !hi) return nullptr;
        return builder_->CreateCall(runtimeMathRandom_, {lo, hi}, "math.random");
    }

    // Built-in math functions (Float)
    if (identCallee->name == "sqrt" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathSqrt_, {arg}, "math.sqrt");
    }
    if (identCallee->name == "pow" && expr.arguments.size() >= 2) {
        llvm::Value* base = emitExpr(*expr.arguments[0]);
        llvm::Value* exp = emitExpr(*expr.arguments[1]);
        if (!base || !exp) return nullptr;
        return builder_->CreateCall(runtimeMathPow_, {base, exp}, "math.pow");
    }
    if (identCallee->name == "floor" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathFloor_, {arg}, "math.floor");
    }
    if (identCallee->name == "ceil" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathCeil_, {arg}, "math.ceil");
    }
    if (identCallee->name == "round" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathRound_, {arg}, "math.round");
    }
    if (identCallee->name == "log" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathLog_, {arg}, "math.log");
    }
    if (identCallee->name == "sin" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathSin_, {arg}, "math.sin");
    }
    if (identCallee->name == "cos" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathCos_, {arg}, "math.cos");
    }
    if (identCallee->name == "tan" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathTan_, {arg}, "math.tan");
    }
    if (identCallee->name == "fabs" && expr.arguments.size() >= 1) {
        llvm::Value* arg = emitExpr(*expr.arguments[0]);
        if (!arg) return nullptr;
        return builder_->CreateCall(runtimeMathFabs_, {arg}, "math.fabs");
    }
    if (identCallee->name == "fmin" && expr.arguments.size() >= 2) {
        llvm::Value* a = emitExpr(*expr.arguments[0]);
        llvm::Value* b = emitExpr(*expr.arguments[1]);
        if (!a || !b) return nullptr;
        return builder_->CreateCall(runtimeMathFmin_, {a, b}, "math.fmin");
    }
    if (identCallee->name == "fmax" && expr.arguments.size() >= 2) {
        llvm::Value* a = emitExpr(*expr.arguments[0]);
        llvm::Value* b = emitExpr(*expr.arguments[1]);
        if (!a || !b) return nullptr;
        return builder_->CreateCall(runtimeMathFmax_, {a, b}, "math.fmax");
    }

    // Built-in readLine()
    if (identCallee->name == "readLine" && expr.arguments.empty()) {
        return builder_->CreateCall(runtimeReadLine_, {}, "stdin.line");
    }

    // Built-in networking functions (TCP)
    if (identCallee->name == "tcpConnect" && expr.arguments.size() >= 2) {
        llvm::Value* host = emitExpr(*expr.arguments[0]);
        llvm::Value* port = emitExpr(*expr.arguments[1]);
        if (!host || !port) return nullptr;
        return builder_->CreateCall(runtimeTcpConnect_, {host, port}, "tcp.connect");
    }
    if (identCallee->name == "tcpListen" && expr.arguments.size() >= 1) {
        llvm::Value* port = emitExpr(*expr.arguments[0]);
        if (!port) return nullptr;
        return builder_->CreateCall(runtimeTcpListen_, {port}, "tcp.listen");
    }
    if (identCallee->name == "tcpAccept" && expr.arguments.size() >= 1) {
        llvm::Value* serverFd = emitExpr(*expr.arguments[0]);
        if (!serverFd) return nullptr;
        return builder_->CreateCall(runtimeTcpAccept_, {serverFd}, "tcp.accept");
    }
    if (identCallee->name == "tcpSend" && expr.arguments.size() >= 2) {
        llvm::Value* fd = emitExpr(*expr.arguments[0]);
        llvm::Value* data = emitExpr(*expr.arguments[1]);
        if (!fd || !data) return nullptr;
        return builder_->CreateCall(runtimeTcpSend_, {fd, data}, "tcp.send");
    }
    if (identCallee->name == "tcpRecv" && expr.arguments.size() >= 2) {
        llvm::Value* fd = emitExpr(*expr.arguments[0]);
        llvm::Value* maxBytes = emitExpr(*expr.arguments[1]);
        if (!fd || !maxBytes) return nullptr;
        return builder_->CreateCall(runtimeTcpRecv_, {fd, maxBytes}, "tcp.recv");
    }
    if (identCallee->name == "tcpClose" && expr.arguments.size() >= 1) {
        llvm::Value* fd = emitExpr(*expr.arguments[0]);
        if (!fd) return nullptr;
        builder_->CreateCall(runtimeTcpClose_, {fd});
        return nullptr;
    }

    // Built-in networking functions (UDP)
    if (identCallee->name == "udpCreate" && expr.arguments.empty()) {
        return builder_->CreateCall(runtimeUdpCreate_, {}, "udp.create");
    }
    if (identCallee->name == "udpBind" && expr.arguments.size() >= 2) {
        llvm::Value* fd = emitExpr(*expr.arguments[0]);
        llvm::Value* port = emitExpr(*expr.arguments[1]);
        if (!fd || !port) return nullptr;
        return builder_->CreateCall(runtimeUdpBind_, {fd, port}, "udp.bind");
    }
    if (identCallee->name == "udpSendTo" && expr.arguments.size() >= 4) {
        llvm::Value* fd = emitExpr(*expr.arguments[0]);
        llvm::Value* host = emitExpr(*expr.arguments[1]);
        llvm::Value* port = emitExpr(*expr.arguments[2]);
        llvm::Value* data = emitExpr(*expr.arguments[3]);
        if (!fd || !host || !port || !data) return nullptr;
        return builder_->CreateCall(runtimeUdpSendTo_, {fd, host, port, data}, "udp.sendto");
    }
    if (identCallee->name == "udpRecvFrom" && expr.arguments.size() >= 2) {
        llvm::Value* fd = emitExpr(*expr.arguments[0]);
        llvm::Value* maxBytes = emitExpr(*expr.arguments[1]);
        if (!fd || !maxBytes) return nullptr;
        return builder_->CreateCall(runtimeUdpRecvFrom_, {fd, maxBytes}, "udp.recvfrom");
    }
    if (identCallee->name == "udpClose" && expr.arguments.size() >= 1) {
        llvm::Value* fd = emitExpr(*expr.arguments[0]);
        if (!fd) return nullptr;
        builder_->CreateCall(runtimeUdpClose_, {fd});
        return nullptr;
    }

    // Built-in DNS
    if (identCallee->name == "dnsLookup" && expr.arguments.size() >= 1) {
        llvm::Value* hostname = emitExpr(*expr.arguments[0]);
        if (!hostname) return nullptr;
        return builder_->CreateCall(runtimeDnsLookup_, {hostname}, "dns.lookup");
    }

    // Built-in ConcurrentMap functions
    if (identCallee->name == "ConcurrentMap" && expr.arguments.empty()) {
        auto* ptr = builder_->CreateCall(runtimeCmapCreate_, {}, "cmap.new");
        return builder_->CreatePtrToInt(ptr, builder_->getInt64Ty());
    }
    if (identCallee->name == "cmapSet" && expr.arguments.size() >= 3) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        llvm::Value* value = emitExpr(*expr.arguments[2]);
        if (!handle || !key || !value) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        builder_->CreateCall(runtimeCmapSet_, {ptr, key, value});
        return nullptr;
    }
    if (identCallee->name == "cmapGet" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        return builder_->CreateCall(runtimeCmapGet_, {ptr, key}, "cmap.get");
    }
    if (identCallee->name == "cmapHas" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        return builder_->CreateCall(runtimeCmapHas_, {ptr, key}, "cmap.has");
    }
    if (identCallee->name == "cmapDelete" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        return builder_->CreateCall(runtimeCmapDelete_, {ptr, key}, "cmap.del");
    }
    if (identCallee->name == "cmapSize" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        return builder_->CreateCall(runtimeCmapSize_, {ptr}, "cmap.size");
    }
    if (identCallee->name == "cmapDestroy" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        builder_->CreateCall(runtimeCmapDestroy_, {ptr});
        return nullptr;
    }

    // Built-in ConcurrentQueue functions
    if (identCallee->name == "ConcurrentQueue" && expr.arguments.empty()) {
        auto* ptr = builder_->CreateCall(runtimeCqueueCreate_, {}, "cqueue.new");
        return builder_->CreatePtrToInt(ptr, builder_->getInt64Ty());
    }
    if (identCallee->name == "cqueueEnqueue" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* value = emitExpr(*expr.arguments[1]);
        if (!handle || !value) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        builder_->CreateCall(runtimeCqueueEnqueue_, {ptr, value});
        return nullptr;
    }
    if (identCallee->name == "cqueueDequeue" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        return builder_->CreateCall(runtimeCqueueDequeue_, {ptr}, "cqueue.deq");
    }
    if (identCallee->name == "cqueueSize" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        return builder_->CreateCall(runtimeCqueueSize_, {ptr}, "cqueue.size");
    }
    if (identCallee->name == "cqueueIsEmpty" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        return builder_->CreateCall(runtimeCqueueIsEmpty_, {ptr}, "cqueue.empty");
    }
    if (identCallee->name == "cqueueDestroy" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        auto* ptr = builder_->CreateIntToPtr(handle, builder_->getPtrTy());
        builder_->CreateCall(runtimeCqueueDestroy_, {ptr});
        return nullptr;
    }

    // Built-in Atomic functions
    if (identCallee->name == "atomicCreate" && expr.arguments.size() >= 1) {
        llvm::Value* initial = emitExpr(*expr.arguments[0]);
        if (!initial) return nullptr;
        return builder_->CreateCall(runtimeAtomicCreate_, {initial}, "atomic.new");
    }
    if (identCallee->name == "atomicLoad" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        return builder_->CreateCall(runtimeAtomicLoad_, {handle}, "atomic.load");
    }
    if (identCallee->name == "atomicStore" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* value = emitExpr(*expr.arguments[1]);
        if (!handle || !value) return nullptr;
        builder_->CreateCall(runtimeAtomicStore_, {handle, value});
        return nullptr;
    }
    if (identCallee->name == "atomicAdd" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* delta = emitExpr(*expr.arguments[1]);
        if (!handle || !delta) return nullptr;
        return builder_->CreateCall(runtimeAtomicAdd_, {handle, delta}, "atomic.add");
    }
    if (identCallee->name == "atomicSub" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* delta = emitExpr(*expr.arguments[1]);
        if (!handle || !delta) return nullptr;
        return builder_->CreateCall(runtimeAtomicSub_, {handle, delta}, "atomic.sub");
    }
    if (identCallee->name == "atomicCompareSwap" && expr.arguments.size() >= 3) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* expected = emitExpr(*expr.arguments[1]);
        llvm::Value* desired = emitExpr(*expr.arguments[2]);
        if (!handle || !expected || !desired) return nullptr;
        return builder_->CreateCall(runtimeAtomicCompareSwap_, {handle, expected, desired}, "atomic.cas");
    }
    if (identCallee->name == "atomicDestroy" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        builder_->CreateCall(runtimeAtomicDestroy_, {handle});
        return nullptr;
    }

    // Built-in HTTP client functions
    if (identCallee->name == "httpGet" && expr.arguments.size() >= 1) {
        llvm::Value* url = emitExpr(*expr.arguments[0]);
        if (!url) return nullptr;
        return builder_->CreateCall(runtimeHttpGet_, {url}, "http.get");
    }
    if (identCallee->name == "httpPost" && expr.arguments.size() >= 3) {
        llvm::Value* url = emitExpr(*expr.arguments[0]);
        llvm::Value* body = emitExpr(*expr.arguments[1]);
        llvm::Value* ct = emitExpr(*expr.arguments[2]);
        if (!url || !body || !ct) return nullptr;
        return builder_->CreateCall(runtimeHttpPost_, {url, body, ct}, "http.post");
    }

    // Built-in HTTP server functions
    if (identCallee->name == "httpServerCreate" && expr.arguments.size() >= 1) {
        llvm::Value* port = emitExpr(*expr.arguments[0]);
        if (!port) return nullptr;
        return builder_->CreateCall(runtimeHttpServerCreate_, {port}, "http.server");
    }
    if (identCallee->name == "httpServerAccept" && expr.arguments.size() >= 1) {
        llvm::Value* server = emitExpr(*expr.arguments[0]);
        if (!server) return nullptr;
        return builder_->CreateCall(runtimeHttpServerAccept_, {server}, "http.accept");
    }
    if (identCallee->name == "httpRequestMethod" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        return builder_->CreateCall(runtimeHttpRequestMethod_, {handle}, "http.method");
    }
    if (identCallee->name == "httpRequestPath" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        return builder_->CreateCall(runtimeHttpRequestPath_, {handle}, "http.path");
    }
    if (identCallee->name == "httpRequestBody" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        return builder_->CreateCall(runtimeHttpRequestBody_, {handle}, "http.body");
    }
    if (identCallee->name == "httpRespond" && expr.arguments.size() >= 3) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* status = emitExpr(*expr.arguments[1]);
        llvm::Value* body = emitExpr(*expr.arguments[2]);
        if (!handle || !status || !body) return nullptr;
        builder_->CreateCall(runtimeHttpRespond_, {handle, status, body});
        return nullptr;
    }
    if (identCallee->name == "httpServerClose" && expr.arguments.size() >= 1) {
        llvm::Value* server = emitExpr(*expr.arguments[0]);
        if (!server) return nullptr;
        builder_->CreateCall(runtimeHttpServerClose_, {server});
        return nullptr;
    }

    // Built-in JSON functions
    if (identCallee->name == "jsonParse" && expr.arguments.size() >= 1) {
        llvm::Value* str = emitExpr(*expr.arguments[0]);
        if (!str) return nullptr;
        return builder_->CreateCall(runtimeJsonParse_, {str}, "json.parse");
    }
    if (identCallee->name == "jsonGet" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        return builder_->CreateCall(runtimeJsonGet_, {handle, key}, "json.get");
    }
    if (identCallee->name == "jsonGetInt" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        return builder_->CreateCall(runtimeJsonGetInt_, {handle, key}, "json.getint");
    }
    if (identCallee->name == "jsonGetBool" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        auto* result = builder_->CreateCall(runtimeJsonGetBool_, {handle, key}, "json.getbool");
        return builder_->CreateICmpNE(result, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), 0), "json.bool");
    }
    if (identCallee->name == "jsonGetFloat" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        return builder_->CreateCall(runtimeJsonGetFloat_, {handle, key}, "json.getfloat");
    }
    if (identCallee->name == "jsonGetArray" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        return builder_->CreateCall(runtimeJsonGetArray_, {handle, key}, "json.getarr");
    }
    if (identCallee->name == "jsonGetObject" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* key = emitExpr(*expr.arguments[1]);
        if (!handle || !key) return nullptr;
        return builder_->CreateCall(runtimeJsonGetObject_, {handle, key}, "json.getobj");
    }
    if (identCallee->name == "jsonArrayLength" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        return builder_->CreateCall(runtimeJsonArrayLength_, {handle}, "json.arrlen");
    }
    if (identCallee->name == "jsonArrayGet" && expr.arguments.size() >= 2) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        llvm::Value* index = emitExpr(*expr.arguments[1]);
        if (!handle || !index) return nullptr;
        return builder_->CreateCall(runtimeJsonArrayGet_, {handle, index}, "json.arrget");
    }
    if (identCallee->name == "jsonStringify" && expr.arguments.size() >= 1) {
        llvm::Value* handle = emitExpr(*expr.arguments[0]);
        if (!handle) return nullptr;
        return builder_->CreateCall(runtimeJsonStringify_, {handle}, "json.stringify");
    }

    // Built-in test assertions
    if (identCallee->name == "assert" && expr.arguments.size() >= 1) {
        llvm::Value* cond = emitExpr(*expr.arguments[0]);
        if (!cond) return nullptr;
        // Convert bool (i1) to i64
        if (cond->getType()->isIntegerTy(1)) {
            cond = builder_->CreateZExt(cond, llvm::Type::getInt64Ty(*context_));
        }
        llvm::Value* msg = nullptr;
        if (expr.arguments.size() >= 2) {
            msg = emitExpr(*expr.arguments[1]);
        } else {
            msg = builder_->CreateGlobalStringPtr("assertion failed");
        }
        auto* file = builder_->CreateGlobalStringPtr(expr.location.file);
        auto* line = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), expr.location.line);
        builder_->CreateCall(runtimeAssert_, {cond, msg, file, line});
        return nullptr;
    }
    if (identCallee->name == "assertTrue" && expr.arguments.size() >= 1) {
        llvm::Value* cond = emitExpr(*expr.arguments[0]);
        if (!cond) return nullptr;
        if (cond->getType()->isIntegerTy(1)) {
            cond = builder_->CreateZExt(cond, llvm::Type::getInt64Ty(*context_));
        }
        llvm::Value* msg = nullptr;
        if (expr.arguments.size() >= 2) {
            msg = emitExpr(*expr.arguments[1]);
        } else {
            msg = builder_->CreateGlobalStringPtr("assertTrue failed");
        }
        auto* file = builder_->CreateGlobalStringPtr(expr.location.file);
        auto* line = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), expr.location.line);
        builder_->CreateCall(runtimeAssert_, {cond, msg, file, line});
        return nullptr;
    }
    if (identCallee->name == "assertFalse" && expr.arguments.size() >= 1) {
        llvm::Value* cond = emitExpr(*expr.arguments[0]);
        if (!cond) return nullptr;
        if (cond->getType()->isIntegerTy(1)) {
            cond = builder_->CreateZExt(cond, llvm::Type::getInt64Ty(*context_));
        }
        // Negate: assertFalse(x) == assert(!x)
        cond = builder_->CreateICmpEQ(cond, llvm::ConstantInt::get(cond->getType(), 0));
        cond = builder_->CreateZExt(cond, llvm::Type::getInt64Ty(*context_));
        llvm::Value* msg = nullptr;
        if (expr.arguments.size() >= 2) {
            msg = emitExpr(*expr.arguments[1]);
        } else {
            msg = builder_->CreateGlobalStringPtr("assertFalse failed");
        }
        auto* file = builder_->CreateGlobalStringPtr(expr.location.file);
        auto* line = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), expr.location.line);
        builder_->CreateCall(runtimeAssert_, {cond, msg, file, line});
        return nullptr;
    }
    if (identCallee->name == "assertEqual" && expr.arguments.size() >= 2) {
        llvm::Value* expected = emitExpr(*expr.arguments[0]);
        llvm::Value* actual = emitExpr(*expr.arguments[1]);
        if (!expected || !actual) return nullptr;
        llvm::Value* msg = nullptr;
        if (expr.arguments.size() >= 3) {
            msg = emitExpr(*expr.arguments[2]);
        } else {
            msg = builder_->CreateGlobalStringPtr("assertEqual failed");
        }
        auto* line = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), expr.location.line);
        // Choose int or string version based on type
        if (expected->getType()->isIntegerTy()) {
            builder_->CreateCall(runtimeAssertEqualInt_, {expected, actual, msg, line});
        } else {
            builder_->CreateCall(runtimeAssertEqualStr_, {expected, actual, msg, line});
        }
        return nullptr;
    }

    // Built-in process execution functions
    if (identCallee->name == "exec" && expr.arguments.size() >= 1) {
        llvm::Value* cmd = emitExpr(*expr.arguments[0]);
        if (!cmd) return nullptr;
        return builder_->CreateCall(runtimeExec_, {cmd}, "exec.code");
    }
    if (identCallee->name == "execOutput" && expr.arguments.size() >= 1) {
        llvm::Value* cmd = emitExpr(*expr.arguments[0]);
        if (!cmd) return nullptr;
        return builder_->CreateCall(runtimeExecOutput_, {cmd}, "exec.out");
    }

    // Built-in File I/O functions
    if (identCallee->name == "readFile" && expr.arguments.size() >= 1) {
        llvm::Value* path = emitExpr(*expr.arguments[0]);
        if (!path) return nullptr;
        return builder_->CreateCall(runtimeReadFile_, {path}, "file.read");
    }
    if (identCallee->name == "writeFile" && expr.arguments.size() >= 2) {
        llvm::Value* path = emitExpr(*expr.arguments[0]);
        llvm::Value* content = emitExpr(*expr.arguments[1]);
        if (!path || !content) return nullptr;
        auto* result = builder_->CreateCall(runtimeWriteFile_, {path, content}, "file.write");
        return builder_->CreateICmpNE(result,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "file.write.bool");
    }
    if (identCallee->name == "appendFile" && expr.arguments.size() >= 2) {
        llvm::Value* path = emitExpr(*expr.arguments[0]);
        llvm::Value* content = emitExpr(*expr.arguments[1]);
        if (!path || !content) return nullptr;
        auto* result = builder_->CreateCall(runtimeAppendFile_, {path, content}, "file.append");
        return builder_->CreateICmpNE(result,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "file.append.bool");
    }
    if (identCallee->name == "fileExists" && expr.arguments.size() >= 1) {
        llvm::Value* path = emitExpr(*expr.arguments[0]);
        if (!path) return nullptr;
        auto* result = builder_->CreateCall(runtimeFileExists_, {path}, "file.exists");
        return builder_->CreateICmpNE(result,
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "file.exists.bool");
    }

    // Regular function call
    llvm::Function* calleeFunc = module_->getFunction(identCallee->name);

    if (!calleeFunc) {
        // Try indirect call — callee might be a variable holding a function pointer (lambda)
        auto it = namedValues_.find(identCallee->name);
        if (it != namedValues_.end()) {
            llvm::Value* funcPtr = builder_->CreateLoad(
                llvm::PointerType::getUnqual(*context_), it->second, identCallee->name + ".load");

            std::vector<llvm::Value*> args;
            for (auto& argExpr : expr.arguments) {
                llvm::Value* argVal = emitExpr(*argExpr);
                if (!argVal) return nullptr;
                args.push_back(argVal);
            }

            // Build function type from arguments
            std::vector<llvm::Type*> paramTypes;
            for (auto* a : args) paramTypes.push_back(a->getType());
            auto* fnType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(*context_), paramTypes, false);

            return builder_->CreateCall(fnType, funcPtr, args, "lambdacall");
        }
        return nullptr;
    }

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < expr.arguments.size(); i++) {
        llvm::Value* argVal = emitExpr(*expr.arguments[i]);
        if (!argVal) return nullptr;

        // Coerce argument type to match function parameter type
        if (i < calleeFunc->arg_size()) {
            llvm::Type* paramTy = calleeFunc->getArg(i)->getType();
            if (paramTy != argVal->getType()) {
                if (paramTy->isIntegerTy() && argVal->getType()->isIntegerTy()) {
                    unsigned paramBits = paramTy->getIntegerBitWidth();
                    unsigned argBits = argVal->getType()->getIntegerBitWidth();
                    if (paramBits < argBits) {
                        argVal = builder_->CreateTrunc(argVal, paramTy, "arg.trunc");
                    } else if (paramBits > argBits) {
                        argVal = builder_->CreateSExt(argVal, paramTy, "arg.sext");
                    }
                } else if (paramTy->isFloatTy() && argVal->getType()->isDoubleTy()) {
                    argVal = builder_->CreateFPTrunc(argVal, paramTy, "arg.fptrunc");
                } else if (paramTy->isDoubleTy() && argVal->getType()->isFloatTy()) {
                    argVal = builder_->CreateFPExt(argVal, paramTy, "arg.fpext");
                } else if (paramTy->isDoubleTy() && argVal->getType()->isIntegerTy()) {
                    argVal = builder_->CreateSIToFP(argVal, paramTy, "arg.itof");
                } else if (paramTy->isFloatTy() && argVal->getType()->isIntegerTy()) {
                    argVal = builder_->CreateSIToFP(argVal, paramTy, "arg.itof32");
                }
            }
        }
        args.push_back(argVal);
    }

    if (calleeFunc->getReturnType()->isVoidTy()) {
        builder_->CreateCall(calleeFunc, args);
        return nullptr;
    }
    return builder_->CreateCall(calleeFunc, args, "calltmp");
}

llvm::Value* CodeGen::emitAssignExpr(AssignExpr& expr) {
    llvm::Value* val = emitExpr(*expr.value);
    if (!val) return nullptr;

    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.target.get())) {
        auto it = namedValues_.find(ident->name);
        if (it != namedValues_.end()) {
            llvm::Type* targetTy = it->second->getAllocatedType();
            // Coerce value type to match alloca type
            if (targetTy != val->getType()) {
                if (targetTy->isIntegerTy() && val->getType()->isIntegerTy()) {
                    unsigned tBits = targetTy->getIntegerBitWidth();
                    unsigned vBits = val->getType()->getIntegerBitWidth();
                    if (tBits < vBits) val = builder_->CreateTrunc(val, targetTy, "assign.trunc");
                    else if (tBits > vBits) val = builder_->CreateSExt(val, targetTy, "assign.sext");
                } else if (targetTy->isFloatTy() && val->getType()->isDoubleTy()) {
                    val = builder_->CreateFPTrunc(val, targetTy, "assign.fptrunc");
                } else if (targetTy->isDoubleTy() && val->getType()->isFloatTy()) {
                    val = builder_->CreateFPExt(val, targetTy, "assign.fpext");
                }
            }
            builder_->CreateStore(val, it->second);
            return val;
        }
    }

    // Index assignment: arr[i] = value
    if (auto* idx = dynamic_cast<IndexExpr*>(expr.target.get())) {
        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
        llvm::Value* arrVal = emitExpr(*idx->object);
        llvm::Value* idxVal = emitExpr(*idx->index);
        if (!arrVal || !idxVal) return nullptr;

        // Bounds check
        auto* lenPtr = builder_->CreateStructGEP(arrayStructType_, arrVal, 0, "arr.len.ptr");
        auto* length = builder_->CreateLoad(i64Ty, lenPtr, "arr.len");
        builder_->CreateCall(runtimeArrayBoundsCheck_, {idxVal, length});

        // Load data pointer
        auto* dataFieldPtr = builder_->CreateStructGEP(arrayStructType_, arrVal, 1, "arr.data.ptr");
        auto* dataPtr = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), dataFieldPtr, "arr.data");

        // Determine element type
        llvm::Type* elemType = i64Ty;
        if (auto* ident = dynamic_cast<IdentifierExpr*>(idx->object.get())) {
            auto it = varArrayElemType_.find(ident->name);
            if (it != varArrayElemType_.end()) {
                elemType = it->second;
            }
        }

        // GEP + store
        auto* elemPtr = builder_->CreateGEP(elemType, dataPtr, idxVal, "elem.ptr");
        builder_->CreateStore(val, elemPtr);
        return val;
    }

    // Member assignment: obj.field = value
    if (auto* member = dynamic_cast<MemberExpr*>(expr.target.get())) {
        return emitMemberStore(*member, val);
    }

    return nullptr;
}

llvm::Value* CodeGen::emitStringInterpolation(StringInterpolationExpr& expr) {
    // Build the string by concatenating parts and expression results
    llvm::Value* result = nullptr;

    for (size_t i = 0; i < expr.parts.size(); i++) {
        // Add the string part
        if (!expr.parts[i].empty()) {
            llvm::Value* partVal = createGlobalString(expr.parts[i]);
            result = result ? emitStringConcat(result, partVal) : partVal;
        }

        // Add the expression value (converted to string)
        if (i < expr.expressions.size()) {
            llvm::Value* exprVal = emitExpr(*expr.expressions[i]);
            if (exprVal) {
                // Convert to string based on type
                if (exprVal->getType()->isIntegerTy(64)) {
                    exprVal = emitIntToString(exprVal);
                } else if (exprVal->getType()->isIntegerTy(8)) {
                    exprVal = builder_->CreateCall(runtimeCharToStr_, {exprVal}, "char.str");
                } else if (exprVal->getType()->isIntegerTy() && !exprVal->getType()->isIntegerTy(1)) {
                    exprVal = builder_->CreateSExt(exprVal, llvm::Type::getInt64Ty(*context_), "sext.interp");
                    exprVal = emitIntToString(exprVal);
                } else if (exprVal->getType()->isDoubleTy()) {
                    exprVal = emitFloatToString(exprVal);
                } else if (exprVal->getType()->isFloatTy()) {
                    exprVal = builder_->CreateFPExt(exprVal, llvm::Type::getDoubleTy(*context_), "fpext.interp");
                    exprVal = emitFloatToString(exprVal);
                } else if (exprVal->getType()->isIntegerTy(1)) {
                    exprVal = emitBoolToString(exprVal);
                }
                // else assume it's already a string (pointer)

                result = result ? emitStringConcat(result, exprVal) : exprVal;
            }
        }
    }

    if (!result) {
        result = createGlobalString("");
    }

    return result;
}

// --- Class Expression Emitters ---

llvm::Value* CodeGen::emitThisExpr(ThisExpr& /*expr*/) {
    if (!thisPtr_) return nullptr;
    // thisPtr_ is an alloca holding the pointer to the struct
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);
    return builder_->CreateLoad(ptrTy, thisPtr_, "this");
}

llvm::Value* CodeGen::emitConstructExpr(ConstructExpr& expr) {
    auto it = classInfos_.find(expr.className);
    if (it == classInfos_.end()) return nullptr;

    auto& info = it->second;
    auto* structTy = info.structType;

    // Allocate on heap using GC
    const auto& dataLayout = module_->getDataLayout();
    uint64_t structSize = dataLayout.getTypeAllocSize(structTy);

    auto* sizeVal = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), structSize);
    auto* typeTag = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*context_), 1); // GC_OBJECT
    auto* rawPtr = builder_->CreateCall(runtimeGcAlloc_, {sizeVal, typeTag}, "obj");

    // Tell GC how many pointer-typed fields this object has (for mark traversal)
    uint16_t ptrFieldCount = 0;
    for (const auto& fieldName : info.fieldNames) {
        int idx = getFieldIndex(expr.className, fieldName);
        if (idx >= 0) {
            auto* fieldTy = structTy->getElementType(idx);
            if (fieldTy->isPointerTy()) ptrFieldCount++;
        }
    }
    if (ptrFieldCount > 0) {
        builder_->CreateCall(runtimeGcSetNumPointers_, {
            rawPtr,
            llvm::ConstantInt::get(llvm::Type::getInt16Ty(*context_), ptrFieldCount)
        });
    }

    // Initialize mutex for shared classes
    if (info.isShared) {
        auto* mutexPtr = builder_->CreateStructGEP(structTy, rawPtr, 0, "mutex.ptr");
        auto* mutexI8Ptr = builder_->CreateBitCast(mutexPtr,
            llvm::PointerType::getUnqual(*context_), "mutex.i8ptr");
        builder_->CreateCall(runtimeMutexInit_, {mutexI8Ptr});
    }

    // Initialize fields
    for (auto& [fieldName, fieldValue] : expr.fieldInits) {
        int idx = getFieldIndex(expr.className, fieldName);
        if (idx < 0) continue;
        auto* fieldPtr = builder_->CreateStructGEP(structTy, rawPtr, idx, fieldName);
        llvm::Value* val = emitExpr(*fieldValue);
        if (val) {
            builder_->CreateStore(val, fieldPtr);
        }
    }

    return rawPtr;
}

llvm::Value* CodeGen::emitMemberExpr(MemberExpr& expr) {
    // Check for enum variant access: EnumName.CaseName
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
        auto eit = enumInfos_.find(ident->name);
        if (eit != enumInfos_.end()) {
            auto& enumInfo = eit->second;
            for (size_t i = 0; i < enumInfo.cases.size(); i++) {
                if (enumInfo.cases[i] == expr.member) {
                    if (enumInfo.hasAssociatedValues && enumInfo.structType) {
                        // For enums with associated values, simple variants (no args)
                        // create a struct {tag, 0}
                        auto* i64Ty = llvm::Type::getInt64Ty(*context_);
                        auto* alloca = builder_->CreateAlloca(enumInfo.structType, nullptr, "enum.tmp");
                        auto* tagPtr = builder_->CreateStructGEP(enumInfo.structType, alloca, 0, "enum.tag.ptr");
                        builder_->CreateStore(llvm::ConstantInt::get(i64Ty, i), tagPtr);
                        auto* valPtr = builder_->CreateStructGEP(enumInfo.structType, alloca, 1, "enum.val.ptr");
                        builder_->CreateStore(llvm::ConstantInt::get(i64Ty, 0), valPtr);
                        return builder_->CreateLoad(enumInfo.structType, alloca, "enum.val");
                    }
                    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context_), i);
                }
            }
            return nullptr;
        }
    }

    // Array .length access
    if (expr.member == "length") {
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
            auto it = namedValues_.find(ident->name);
            if (it != namedValues_.end() && it->second->getAllocatedType() == arrayStructType_) {
                auto* lenPtr = builder_->CreateStructGEP(arrayStructType_, it->second, 0, "arr.len.ptr");
                return builder_->CreateLoad(llvm::Type::getInt64Ty(*context_), lenPtr, "arr.len");
            }
        }
        // String .length property
        llvm::Value* objVal = emitExpr(*expr.object);
        if (objVal && objVal->getType()->isPointerTy()) {
            return builder_->CreateCall(runtimeStrLen_, {objVal}, "str.len");
        }
    }

    // TypeInfo properties (reflection)
    if (expr.member == "name" || expr.member == "fields" || expr.member == "implements") {
        if (auto* callExpr = dynamic_cast<CallExpr*>(expr.object.get())) {
            if (auto* callIdent = dynamic_cast<IdentifierExpr*>(callExpr->callee.get())) {
                if (callIdent->name == "typeof") {
                    llvm::Value* tiPtr = emitExpr(*expr.object);
                    if (tiPtr) {
                        if (expr.member == "name") {
                            return builder_->CreateCall(runtimeTypeInfoName_, {tiPtr}, "ti.name");
                        }
                        if (expr.member == "fields") {
                            auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "ti.fields.arr");
                            builder_->CreateCall(runtimeTypeInfoFields_, {tiPtr, outArr});
                            return outArr;
                        }
                        if (expr.member == "implements") {
                            auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "ti.impl.arr");
                            builder_->CreateCall(runtimeTypeInfoImplements_, {tiPtr, outArr});
                            return outArr;
                        }
                    }
                }
            }
        }
        // Also handle when typeof result is stored in a variable
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
            auto it = namedValues_.find(ident->name);
            if (it != namedValues_.end()) {
                llvm::Value* tiPtr = builder_->CreateLoad(
                    llvm::PointerType::getUnqual(*context_), it->second, "ti.ptr");
                if (expr.member == "name") {
                    return builder_->CreateCall(runtimeTypeInfoName_, {tiPtr}, "ti.name");
                }
                if (expr.member == "fields") {
                    auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "ti.fields.arr");
                    builder_->CreateCall(runtimeTypeInfoFields_, {tiPtr, outArr});
                    return outArr;
                }
                if (expr.member == "implements") {
                    auto* outArr = builder_->CreateAlloca(arrayStructType_, nullptr, "ti.impl.arr");
                    builder_->CreateCall(runtimeTypeInfoImplements_, {tiPtr, outArr});
                    return outArr;
                }
            }
        }
    }

    // Map/Set .size property
    if (expr.member == "size") {
        if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
            auto it = namedValues_.find(ident->name);
            if (it != namedValues_.end()) {
                llvm::Value* ptr = builder_->CreateLoad(
                    llvm::PointerType::getUnqual(*context_), it->second, "col.ptr");
                if (varSetNames_.count(ident->name)) {
                    return builder_->CreateCall(runtimeSetSize_, {ptr}, "set.size");
                }
                return builder_->CreateCall(runtimeMapSize_, {ptr}, "map.size");
            }
        }
    }

    llvm::Value* objPtr = emitExpr(*expr.object);
    if (!objPtr) return nullptr;

    // Prefer current class context for field resolution (important for generics)
    if (!currentClassName_.empty()) {
        int idx = getFieldIndex(currentClassName_, expr.member);
        if (idx >= 0) {
            auto& info = classInfos_[currentClassName_];
            // Shared class: lock before read, unlock after
            if (info.isShared) {
                auto* mutexPtr = builder_->CreateStructGEP(info.structType, objPtr, 0, "mutex.ptr");
                auto* mutexI8 = builder_->CreateBitCast(mutexPtr, llvm::PointerType::getUnqual(*context_));
                builder_->CreateCall(runtimeMutexLock_, {mutexI8});
            }
            auto* fieldPtr = builder_->CreateStructGEP(info.structType, objPtr, idx, expr.member + ".ptr");
            auto* fieldTy = info.structType->getElementType(idx);
            auto* val = builder_->CreateLoad(fieldTy, fieldPtr, expr.member);
            if (info.isShared) {
                auto* mutexPtr = builder_->CreateStructGEP(info.structType, objPtr, 0, "mutex.ptr2");
                auto* mutexI8 = builder_->CreateBitCast(mutexPtr, llvm::PointerType::getUnqual(*context_));
                builder_->CreateCall(runtimeMutexUnlock_, {mutexI8});
            }
            return val;
        }
    }

    // Fallback: walk through all classInfos_ to find matching field
    for (auto& [className, info] : classInfos_) {
        int idx = getFieldIndex(className, expr.member);
        if (idx >= 0) {
            if (info.isShared) {
                auto* mutexPtr = builder_->CreateStructGEP(info.structType, objPtr, 0, "mutex.ptr");
                auto* mutexI8 = builder_->CreateBitCast(mutexPtr, llvm::PointerType::getUnqual(*context_));
                builder_->CreateCall(runtimeMutexLock_, {mutexI8});
            }
            auto* fieldPtr = builder_->CreateStructGEP(info.structType, objPtr, idx, expr.member + ".ptr");
            auto* fieldTy = info.structType->getElementType(idx);
            auto* val = builder_->CreateLoad(fieldTy, fieldPtr, expr.member);
            if (info.isShared) {
                auto* mutexPtr = builder_->CreateStructGEP(info.structType, objPtr, 0, "mutex.ptr2");
                auto* mutexI8 = builder_->CreateBitCast(mutexPtr, llvm::PointerType::getUnqual(*context_));
                builder_->CreateCall(runtimeMutexUnlock_, {mutexI8});
            }
            return val;
        }
    }

    return nullptr;
}

llvm::Value* CodeGen::emitMemberStore(MemberExpr& member, llvm::Value* value) {
    llvm::Value* objPtr = emitExpr(*member.object);
    if (!objPtr || !value) return nullptr;

    for (auto& [className, info] : classInfos_) {
        int idx = getFieldIndex(className, member.member);
        if (idx >= 0) {
            if (info.isShared) {
                auto* mutexPtr = builder_->CreateStructGEP(info.structType, objPtr, 0, "mutex.ptr");
                auto* mutexI8 = builder_->CreateBitCast(mutexPtr, llvm::PointerType::getUnqual(*context_));
                builder_->CreateCall(runtimeMutexLock_, {mutexI8});
            }
            auto* fieldPtr = builder_->CreateStructGEP(info.structType, objPtr, idx, member.member + ".ptr");
            builder_->CreateStore(value, fieldPtr);
            if (info.isShared) {
                auto* mutexPtr = builder_->CreateStructGEP(info.structType, objPtr, 0, "mutex.ptr2");
                auto* mutexI8 = builder_->CreateBitCast(mutexPtr, llvm::PointerType::getUnqual(*context_));
                builder_->CreateCall(runtimeMutexUnlock_, {mutexI8});
            }
            return value;
        }
    }

    return nullptr;
}

int CodeGen::getFieldIndex(const std::string& className, const std::string& fieldName) {
    auto it = classInfos_.find(className);
    if (it == classInfos_.end()) return -1;
    auto& names = it->second.fieldNames;
    // Shared classes have a mutex at struct index 0, so user fields start at index 1
    int offset = it->second.isShared ? 1 : 0;
    for (size_t i = 0; i < names.size(); i++) {
        if (names[i] == fieldName) return static_cast<int>(i) + offset;
    }
    return -1;
}

llvm::Value* CodeGen::emitLambdaExpr(LambdaExpr& expr) {
    // Generate a unique name for the lambda function
    std::string lambdaName = "__lambda_" + std::to_string(lambdaCounter_++);

    // Build parameter types — default to i64 for untyped params
    std::vector<llvm::Type*> paramTypes;
    for (auto& param : expr.params) {
        if (param.type) {
            paramTypes.push_back(getLLVMType(param.type.get()));
        } else if (lambdaParamTypeHint_) {
            paramTypes.push_back(lambdaParamTypeHint_);
        } else {
            paramTypes.push_back(llvm::Type::getInt64Ty(*context_));
        }
    }

    // Save current insert point
    auto* savedBlock = builder_->GetInsertBlock();
    auto savedNamedValues = namedValues_;

    // First pass: emit body to a temporary function to discover return type
    // Create a temporary function with i64 return to probe the body
    auto* probeRetType = llvm::Type::getInt64Ty(*context_);
    auto* probeFuncType = llvm::FunctionType::get(probeRetType, paramTypes, false);
    auto* probeFunc = llvm::Function::Create(
        probeFuncType, llvm::Function::InternalLinkage, lambdaName + ".probe", module_.get());

    auto* probeBB = llvm::BasicBlock::Create(*context_, "entry", probeFunc);
    builder_->SetInsertPoint(probeBB);

    namedValues_.clear();
    for (auto& [name, val] : savedNamedValues) {
        namedValues_[name] = val;
    }

    size_t i = 0;
    for (auto& arg : probeFunc->args()) {
        arg.setName(expr.params[i].name);
        auto* alloca = builder_->CreateAlloca(arg.getType(), nullptr, expr.params[i].name);
        builder_->CreateStore(&arg, alloca);
        namedValues_[expr.params[i].name] = alloca;
        i++;
    }

    // Emit body to discover the actual return type
    llvm::Type* actualRetType = llvm::Type::getInt64Ty(*context_);
    if (expr.bodyExpr) {
        llvm::Value* result = emitExpr(*expr.bodyExpr);
        if (result) {
            actualRetType = result->getType();
        } else {
            actualRetType = llvm::Type::getVoidTy(*context_);
        }
    }

    // Discard the probe function
    probeFunc->eraseFromParent();

    // Now create the real lambda function with the correct return type
    namedValues_ = savedNamedValues;
    builder_->SetInsertPoint(savedBlock);

    auto* funcType = llvm::FunctionType::get(actualRetType, paramTypes, false);
    auto* lambdaFunc = llvm::Function::Create(
        funcType, llvm::Function::InternalLinkage, lambdaName, module_.get());

    auto* entryBB = llvm::BasicBlock::Create(*context_, "entry", lambdaFunc);
    builder_->SetInsertPoint(entryBB);

    namedValues_.clear();
    for (auto& [name, val] : savedNamedValues) {
        namedValues_[name] = val;
    }

    i = 0;
    for (auto& arg : lambdaFunc->args()) {
        arg.setName(expr.params[i].name);
        auto* alloca = builder_->CreateAlloca(arg.getType(), nullptr, expr.params[i].name);
        builder_->CreateStore(&arg, alloca);
        namedValues_[expr.params[i].name] = alloca;
        i++;
    }

    // Emit body for real
    if (expr.bodyExpr) {
        llvm::Value* result = emitExpr(*expr.bodyExpr);
        if (result) {
            builder_->CreateRet(result);
        } else {
            builder_->CreateRetVoid();
        }
    } else if (expr.bodyBlock) {
        emitBlock(*expr.bodyBlock);
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (actualRetType->isVoidTy()) {
                builder_->CreateRetVoid();
            } else {
                builder_->CreateRet(llvm::ConstantInt::get(actualRetType, 0));
            }
        }
    }

    // Restore insert point and named values
    namedValues_ = savedNamedValues;
    builder_->SetInsertPoint(savedBlock);

    return lambdaFunc;
}

llvm::Value* CodeGen::emitIfExpr(IfExpr& expr) {
    llvm::Value* cond = emitExpr(*expr.condition);
    if (!cond) return nullptr;

    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    auto* thenBB = llvm::BasicBlock::Create(*context_, "ifexpr.then", func);
    auto* elseBB = llvm::BasicBlock::Create(*context_, "ifexpr.else", func);
    auto* mergeBB = llvm::BasicBlock::Create(*context_, "ifexpr.merge", func);

    builder_->CreateCondBr(cond, thenBB, elseBB);

    // Then branch
    builder_->SetInsertPoint(thenBB);
    llvm::Value* thenVal = emitExpr(*expr.thenExpr);
    if (!thenVal) return nullptr;
    auto* thenEndBB = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    // Else branch
    builder_->SetInsertPoint(elseBB);
    llvm::Value* elseVal = emitExpr(*expr.elseExpr);
    if (!elseVal) return nullptr;
    // Coerce else value to match then value type
    if (elseVal->getType() != thenVal->getType()) {
        if (thenVal->getType()->isIntegerTy() && elseVal->getType()->isIntegerTy()) {
            unsigned tBits = thenVal->getType()->getIntegerBitWidth();
            unsigned eBits = elseVal->getType()->getIntegerBitWidth();
            if (tBits > eBits) elseVal = builder_->CreateSExt(elseVal, thenVal->getType(), "ifexpr.sext");
            else if (eBits > tBits) elseVal = builder_->CreateTrunc(elseVal, thenVal->getType(), "ifexpr.trunc");
        }
    }
    auto* elseEndBB = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    // Merge with PHI
    builder_->SetInsertPoint(mergeBB);
    auto* phi = builder_->CreatePHI(thenVal->getType(), 2, "ifexpr.result");
    phi->addIncoming(thenVal, thenEndBB);
    phi->addIncoming(elseVal, elseEndBB);
    return phi;
}

llvm::Value* CodeGen::emitArrayLiteralExpr(ArrayLiteralExpr& expr) {
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);
    auto* ptrTy = llvm::PointerType::getUnqual(*context_);

    int64_t count = static_cast<int64_t>(expr.elements.size());

    // Determine element type from first element
    llvm::Type* elemType = i64Ty; // default
    llvm::Value* firstVal = nullptr;
    if (!expr.elements.empty()) {
        firstVal = emitExpr(*expr.elements[0]);
        if (firstVal) elemType = firstVal->getType();
    }

    // Get element size
    const auto& dataLayout = module_->getDataLayout();
    uint64_t elemSize = dataLayout.getTypeAllocSize(elemType);

    // Allocate data buffer: chris_array_alloc(elemSize, count)
    llvm::Value* dataPtr = builder_->CreateCall(runtimeArrayAlloc_, {
        llvm::ConstantInt::get(i64Ty, elemSize),
        llvm::ConstantInt::get(i64Ty, count)
    }, "arrdata");

    // Store elements
    for (size_t i = 0; i < expr.elements.size(); i++) {
        llvm::Value* val = (i == 0 && firstVal) ? firstVal : emitExpr(*expr.elements[i]);
        if (!val) continue;
        llvm::Value* elemPtr = builder_->CreateGEP(elemType, dataPtr,
            llvm::ConstantInt::get(i64Ty, i), "elem.ptr");
        builder_->CreateStore(val, elemPtr);
    }

    // Create array struct {i64 length, ptr data} on stack
    auto* func = builder_->GetInsertBlock()->getParent();
    auto* arrAlloca = createEntryBlockAlloca(func, "arr", arrayStructType_);

    // Store length
    auto* lenPtr = builder_->CreateStructGEP(arrayStructType_, arrAlloca, 0, "arr.len.ptr");
    builder_->CreateStore(llvm::ConstantInt::get(i64Ty, count), lenPtr);

    // Store data pointer
    auto* dataFieldPtr = builder_->CreateStructGEP(arrayStructType_, arrAlloca, 1, "arr.data.ptr");
    builder_->CreateStore(dataPtr, dataFieldPtr);

    // Return pointer to the array struct
    return arrAlloca;
}

llvm::Value* CodeGen::emitIndexExpr(IndexExpr& expr) {
    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    llvm::Value* arrVal = emitExpr(*expr.object);
    llvm::Value* idxVal = emitExpr(*expr.index);
    if (!arrVal || !idxVal) return nullptr;

    // String indexing: s[i] -> charAt (returns i8)
    if (arrVal->getType()->isPointerTy()) {
        // Check if this is a string variable (not an array alloca)
        bool isString = false;
        if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(arrVal)) {
            isString = (allocaInst->getAllocatedType() != arrayStructType_);
        } else {
            // Non-alloca pointer — likely a string from a load
            isString = true;
        }
        if (isString) {
            // Load the string pointer if it's an alloca
            llvm::Value* strPtr = arrVal;
            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(arrVal)) {
                strPtr = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), arrVal, "str.ptr");
            }
            auto* charPtr = builder_->CreateGEP(llvm::Type::getInt8Ty(*context_), strPtr, idxVal, "char.ptr");
            return builder_->CreateLoad(llvm::Type::getInt8Ty(*context_), charPtr, "char.val");
        }
    }

    // arrVal is a pointer to Array struct {i64, ptr}
    // Load length for bounds check
    auto* lenPtr = builder_->CreateStructGEP(arrayStructType_, arrVal, 0, "arr.len.ptr");
    auto* length = builder_->CreateLoad(i64Ty, lenPtr, "arr.len");

    // Bounds check
    builder_->CreateCall(runtimeArrayBoundsCheck_, {idxVal, length});

    // Load data pointer
    auto* dataFieldPtr = builder_->CreateStructGEP(arrayStructType_, arrVal, 1, "arr.data.ptr");
    auto* dataPtr = builder_->CreateLoad(llvm::PointerType::getUnqual(*context_), dataFieldPtr, "arr.data");

    // Determine element type from varArrayElemType_ if available
    llvm::Type* elemType = i64Ty; // default
    if (auto* ident = dynamic_cast<IdentifierExpr*>(expr.object.get())) {
        auto it = varArrayElemType_.find(ident->name);
        if (it != varArrayElemType_.end()) {
            elemType = it->second;
        }
    }

    // GEP + load
    auto* elemPtr = builder_->CreateGEP(elemType, dataPtr, idxVal, "elem.ptr");
    return builder_->CreateLoad(elemType, elemPtr, "elem");
}

void CodeGen::emitThrowStmt(ThrowStmt& stmt) {
    llvm::Value* msg = emitExpr(*stmt.expression);
    if (!msg) {
        msg = createGlobalString("unknown error");
    }
    // If the expression is an integer, convert to string
    if (msg->getType()->isIntegerTy(64)) {
        msg = emitIntToString(msg);
    } else if (msg->getType()->isDoubleTy()) {
        msg = emitFloatToString(msg);
    }
    builder_->CreateCall(runtimeThrow_, {msg});
    // throw is noreturn in this context, but we add unreachable
    builder_->CreateUnreachable();
}

void CodeGen::emitTryCatchStmt(TryCatchStmt& stmt) {
    llvm::Function* func = builder_->GetInsertBlock()->getParent();

    // 1. Call chris_try_begin() to get the try depth
    llvm::Value* depth = builder_->CreateCall(runtimeTryBegin_, {}, "try.depth");

    // 2. Get the jmp_buf pointer for this depth
    llvm::Value* jmpbuf = builder_->CreateCall(runtimeGetJmpbuf_, {depth}, "try.jmpbuf");

    // 3. Call setjmp — returns 0 on initial call, non-zero when longjmp'd
    llvm::Value* sjResult = builder_->CreateCall(setjmpFunc_, {jmpbuf}, "try.setjmp");

    // 4. Branch: 0 = try body, non-zero = catch
    llvm::Value* isException = builder_->CreateICmpNE(sjResult,
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "try.isexc");

    auto* tryBB = llvm::BasicBlock::Create(*context_, "try.body", func);
    auto* catchBB = llvm::BasicBlock::Create(*context_, "catch.body", func);
    auto* endBB = llvm::BasicBlock::Create(*context_, "try.end", func);

    builder_->CreateCondBr(isException, catchBB, tryBB);

    // --- Try body ---
    builder_->SetInsertPoint(tryBB);
    emitBlock(*stmt.tryBlock);
    // Normal exit: pop the try stack
    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateCall(runtimeTryEnd_, {});
        builder_->CreateBr(endBB);
    }

    // --- Catch body ---
    builder_->SetInsertPoint(catchBB);
    // Get the exception message
    llvm::Value* excMsg = builder_->CreateCall(runtimeGetException_, {}, "exc.msg");

    // For each catch clause, bind the variable and emit the body
    // (simplified: first catch clause handles all exceptions)
    if (!stmt.catchClauses.empty()) {
        auto& clause = stmt.catchClauses[0];
        // Store exception message in a variable
        auto* alloca = builder_->CreateAlloca(
            llvm::PointerType::getUnqual(*context_), nullptr, clause.varName);
        builder_->CreateStore(excMsg, alloca);
        namedValues_[clause.varName] = alloca;

        emitBlock(*clause.body);
    }

    if (!builder_->GetInsertBlock()->getTerminator()) {
        builder_->CreateBr(endBB);
    }

    // --- Finally block (if present) ---
    if (stmt.finallyBlock) {
        // Both try and catch branch to endBB; emit finally there, then continue
        builder_->SetInsertPoint(endBB);
        emitBlock(*stmt.finallyBlock);
        if (!builder_->GetInsertBlock()->getTerminator()) {
            auto* realEndBB = llvm::BasicBlock::Create(*context_, "try.realend", func);
            builder_->CreateBr(realEndBB);
            builder_->SetInsertPoint(realEndBB);
        }
    } else {
        builder_->SetInsertPoint(endBB);
    }
}

void CodeGen::emitEnumDecl(EnumDecl& /*decl*/) {
    // Enums are just integer tags — no codegen needed beyond registration in pass 0
}

llvm::Value* CodeGen::emitMatchExpr(MatchExpr& expr) {
    llvm::Value* subject = emitExpr(*expr.subject);
    if (!subject) return nullptr;

    auto* i64Ty = llvm::Type::getInt64Ty(*context_);

    // Determine if this is an enum with associated values (struct type)
    bool isTaggedEnum = subject->getType()->isStructTy();
    llvm::Value* tagVal = nullptr;
    llvm::Value* assocVal = nullptr;

    if (isTaggedEnum) {
        // Subject is a {i64 tag, i64 value} struct — extract tag for switching
        auto* subjectAlloca = builder_->CreateAlloca(subject->getType(), nullptr, "match.subj");
        builder_->CreateStore(subject, subjectAlloca);
        auto* tagPtr = builder_->CreateStructGEP(subject->getType(), subjectAlloca, 0, "match.tag.ptr");
        tagVal = builder_->CreateLoad(i64Ty, tagPtr, "match.tag");
        auto* valPtr = builder_->CreateStructGEP(subject->getType(), subjectAlloca, 1, "match.val.ptr");
        assocVal = builder_->CreateLoad(i64Ty, valPtr, "match.assoc");
    } else {
        tagVal = subject;
    }

    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context_, "match.end", func);

    // Create a separate default block that branches to merge
    llvm::BasicBlock* defaultBB = llvm::BasicBlock::Create(*context_, "match.default", func);

    // Save the current block (where the switch will be created)
    llvm::BasicBlock* switchBB = builder_->GetInsertBlock();

    // First, emit the default block
    builder_->SetInsertPoint(defaultBB);
    builder_->CreateBr(mergeBB);

    // Go back to the switch block to create the switch instruction
    builder_->SetInsertPoint(switchBB);
    auto* switchInst = builder_->CreateSwitch(tagVal, defaultBB, expr.arms.size());

    // Track arm results for PHI node (match-as-expression)
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> armResults;
    bool isExprMatch = false;

    for (auto& arm : expr.arms) {
        // Find the case index
        int caseIdx = -1;
        std::string enumName = arm.enumName;
        if (enumName.empty()) {
            for (auto& [name, info] : enumInfos_) {
                for (size_t i = 0; i < info.cases.size(); i++) {
                    if (info.cases[i] == arm.caseName) {
                        caseIdx = static_cast<int>(i);
                        break;
                    }
                }
                if (caseIdx >= 0) break;
            }
        } else {
            auto eit = enumInfos_.find(enumName);
            if (eit != enumInfos_.end()) {
                for (size_t i = 0; i < eit->second.cases.size(); i++) {
                    if (eit->second.cases[i] == arm.caseName) {
                        caseIdx = static_cast<int>(i);
                        break;
                    }
                }
            }
        }

        if (caseIdx < 0) continue;

        llvm::BasicBlock* armBB = llvm::BasicBlock::Create(*context_,
            "match." + arm.caseName, func);
        switchInst->addCase(
            llvm::ConstantInt::get(i64Ty, caseIdx),
            armBB);

        builder_->SetInsertPoint(armBB);

        // Bind the associated value if destructuring: Ok(val) => ...
        if (!arm.bindingName.empty() && isTaggedEnum && assocVal) {
            // Determine the associated value's actual type from enum info
            std::string variantTypeName;
            for (auto& [ename, einfo] : enumInfos_) {
                auto vit = einfo.variantTypeNames.find(arm.caseName);
                if (vit != einfo.variantTypeNames.end()) {
                    variantTypeName = vit->second;
                    break;
                }
            }

            if (variantTypeName == "String") {
                // Cast i64 back to pointer for string bindings
                auto* ptrTy = llvm::PointerType::getUnqual(*context_);
                auto* bindAlloca = builder_->CreateAlloca(ptrTy, nullptr, arm.bindingName);
                auto* ptrVal = builder_->CreateIntToPtr(assocVal, ptrTy);
                builder_->CreateStore(ptrVal, bindAlloca);
                namedValues_[arm.bindingName] = bindAlloca;
            } else if (variantTypeName == "Float") {
                auto* dblTy = llvm::Type::getDoubleTy(*context_);
                auto* bindAlloca = builder_->CreateAlloca(dblTy, nullptr, arm.bindingName);
                auto* dblVal = builder_->CreateBitCast(assocVal, dblTy);
                builder_->CreateStore(dblVal, bindAlloca);
                namedValues_[arm.bindingName] = bindAlloca;
            } else {
                // Int or other i64-sized types
                auto* bindAlloca = builder_->CreateAlloca(i64Ty, nullptr, arm.bindingName);
                builder_->CreateStore(assocVal, bindAlloca);
                namedValues_[arm.bindingName] = bindAlloca;
            }
        }

        // Check if arm body is an expression (ExprStmt) — emit value for PHI
        llvm::Value* armVal = nullptr;
        if (arm.body) {
            if (auto* exprStmt = dynamic_cast<ExprStmt*>(arm.body.get())) {
                armVal = emitExpr(*exprStmt->expression);
                if (armVal) isExprMatch = true;
            } else {
                emitStmt(*arm.body);
            }
        }

        // Clean up binding
        if (!arm.bindingName.empty()) {
            namedValues_.erase(arm.bindingName);
        }

        // If the block doesn't already have a terminator, branch to merge
        if (!builder_->GetInsertBlock()->getTerminator()) {
            if (armVal) {
                armResults.push_back({armVal, builder_->GetInsertBlock()});
            }
            builder_->CreateBr(mergeBB);
        }
    }

    builder_->SetInsertPoint(mergeBB);

    // If match was used as expression, create PHI node
    if (isExprMatch && !armResults.empty()) {
        // Add default block with a zero value to the PHI
        auto* phi = builder_->CreatePHI(armResults[0].first->getType(),
            armResults.size() + 1, "match.result");
        // Default case produces zero
        phi->addIncoming(
            llvm::Constant::getNullValue(armResults[0].first->getType()), defaultBB);
        for (auto& [val, bb] : armResults) {
            phi->addIncoming(val, bb);
        }
        return phi;
    }

    return nullptr;
}

llvm::Value* CodeGen::emitNilCoalesceExpr(NilCoalesceExpr& expr) {
    llvm::Value* val = emitExpr(*expr.value);
    if (!val) return nullptr;

    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* isNilBB = llvm::BasicBlock::Create(*context_, "coalesce.nil", func);
    llvm::BasicBlock* notNilBB = llvm::BasicBlock::Create(*context_, "coalesce.notnil", func);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context_, "coalesce.merge", func);

    // Check if value is null
    llvm::Value* isNull;
    if (val->getType()->isPointerTy()) {
        isNull = builder_->CreateICmpEQ(val,
            llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)), "isnull");
    } else {
        // Non-pointer types can't be nil, just use the value
        return val;
    }

    builder_->CreateCondBr(isNull, isNilBB, notNilBB);

    // Nil branch: evaluate default
    builder_->SetInsertPoint(isNilBB);
    llvm::Value* defaultVal = emitExpr(*expr.defaultValue);
    // If default is a non-pointer type but we need a pointer, this is fine for strings/classes
    llvm::BasicBlock* isNilEnd = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    // Not-nil branch: use original value
    builder_->SetInsertPoint(notNilBB);
    llvm::BasicBlock* notNilEnd = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    // Merge
    builder_->SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder_->CreatePHI(val->getType(), 2, "coalesce");
    phi->addIncoming(defaultVal, isNilEnd);
    phi->addIncoming(val, notNilEnd);
    return phi;
}

llvm::Value* CodeGen::emitForceUnwrapExpr(ForceUnwrapExpr& expr) {
    llvm::Value* val = emitExpr(*expr.operand);
    if (!val) return nullptr;
    // Force unwrap just returns the value — runtime trap on nil would be added later
    return val;
}

llvm::Value* CodeGen::emitOptionalChainExpr(OptionalChainExpr& expr) {
    llvm::Value* objPtr = emitExpr(*expr.object);
    if (!objPtr) return nullptr;

    llvm::Function* func = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* isNilBB = llvm::BasicBlock::Create(*context_, "optchain.nil", func);
    llvm::BasicBlock* notNilBB = llvm::BasicBlock::Create(*context_, "optchain.notnil", func);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(*context_, "optchain.merge", func);

    // Check if object is null
    llvm::Value* isNull = builder_->CreateICmpEQ(objPtr,
        llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_)), "isnull");
    builder_->CreateCondBr(isNull, isNilBB, notNilBB);

    // Nil branch: return null
    builder_->SetInsertPoint(isNilBB);
    llvm::Value* nilVal = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_));
    builder_->CreateBr(mergeBB);

    // Not-nil branch: access the member
    builder_->SetInsertPoint(notNilBB);
    // Create a temporary MemberExpr to reuse emitMemberExpr
    MemberExpr tempMember;
    tempMember.location = expr.location;
    // We need to emit the member access using the already-loaded objPtr
    // Find the class and field index
    llvm::Value* memberVal = nullptr;
    for (auto& [className, info] : classInfos_) {
        int idx = getFieldIndex(className, expr.member);
        if (idx >= 0) {
            auto* gep = builder_->CreateStructGEP(info.structType, objPtr, idx, expr.member);
            memberVal = builder_->CreateLoad(info.structType->getElementType(idx), gep, "field");
            break;
        }
    }
    if (!memberVal) {
        memberVal = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*context_));
    }
    llvm::BasicBlock* notNilEnd = builder_->GetInsertBlock();
    builder_->CreateBr(mergeBB);

    // Merge — result is pointer type (nullable)
    builder_->SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder_->CreatePHI(memberVal->getType(), 2, "optchain");
    phi->addIncoming(nilVal, isNilBB);
    phi->addIncoming(memberVal, notNilEnd);
    return phi;
}

// --- Helpers ---

llvm::Value* CodeGen::emitStringConcat(llvm::Value* left, llvm::Value* right) {
    return builder_->CreateCall(runtimeStrcat_, {left, right}, "strcat");
}

llvm::Value* CodeGen::emitIntToString(llvm::Value* val) {
    return builder_->CreateCall(runtimeIntToStr_, {val}, "intstr");
}

llvm::Value* CodeGen::emitFloatToString(llvm::Value* val) {
    return builder_->CreateCall(runtimeFloatToStr_, {val}, "floatstr");
}

llvm::Value* CodeGen::emitBoolToString(llvm::Value* val) {
    return builder_->CreateCall(runtimeBoolToStr_, {val}, "boolstr");
}

llvm::Value* CodeGen::createGlobalString(const std::string& str) {
    return builder_->CreateGlobalString(str, "str");
}

llvm::Value* CodeGen::emitRangeExpr(RangeExpr& /*expr*/) {
    // Range expressions are handled directly by emitForStmt
    // If used outside a for loop, return nullptr
    return nullptr;
}

llvm::Type* CodeGen::getLLVMTypeFromSema(const std::shared_ptr<Type>& type) {
    if (!type) return llvm::Type::getInt64Ty(*context_);
    switch (type->kind()) {
        case TypeKind::Int:     return llvm::Type::getInt64Ty(*context_);
        case TypeKind::Float:   return llvm::Type::getDoubleTy(*context_);
        case TypeKind::Bool:    return llvm::Type::getInt1Ty(*context_);
        case TypeKind::String:  return llvm::PointerType::getUnqual(*context_);
        case TypeKind::Char:    return llvm::Type::getInt8Ty(*context_);
        case TypeKind::Void:    return llvm::Type::getVoidTy(*context_);
        case TypeKind::Nullable: return llvm::PointerType::getUnqual(*context_);
        case TypeKind::Class:   return llvm::PointerType::getUnqual(*context_);
        case TypeKind::TypeParameter: return llvm::Type::getInt64Ty(*context_); // fallback
        default:                return llvm::Type::getInt64Ty(*context_);
    }
}

llvm::Type* CodeGen::getLLVMType(TypeExpr* typeExpr) {
    if (!typeExpr) return llvm::Type::getInt64Ty(*context_); // default to Int
    auto* named = dynamic_cast<NamedType*>(typeExpr);
    if (!named) return llvm::Type::getInt64Ty(*context_);

    // Nullable types are always represented as pointers
    if (named->nullable) {
        return llvm::PointerType::getUnqual(*context_);
    }

    if (named->name == "Int")     return llvm::Type::getInt64Ty(*context_);
    if (named->name == "Int8")    return llvm::Type::getInt16Ty(*context_); // i16 to distinguish from Char(i8)
    if (named->name == "Int16")   return llvm::Type::getInt16Ty(*context_);
    if (named->name == "Int32")   return llvm::Type::getInt32Ty(*context_);
    if (named->name == "UInt")    return llvm::Type::getInt64Ty(*context_);
    if (named->name == "UInt8")   return llvm::Type::getInt16Ty(*context_); // i16 to distinguish from Char(i8)
    if (named->name == "UInt16")  return llvm::Type::getInt16Ty(*context_);
    if (named->name == "UInt32")  return llvm::Type::getInt32Ty(*context_);
    if (named->name == "Float")   return llvm::Type::getDoubleTy(*context_);
    if (named->name == "Float32") return llvm::Type::getFloatTy(*context_);
    if (named->name == "Bool")    return llvm::Type::getInt1Ty(*context_);
    if (named->name == "String")  return llvm::PointerType::getUnqual(*context_);
    if (named->name == "Char")    return llvm::Type::getInt8Ty(*context_);
    if (named->name == "Void")    return llvm::Type::getVoidTy(*context_);

    // Function type: __func convention — represented as a pointer (function pointer)
    if (named->name == "__func")  return llvm::PointerType::getUnqual(*context_);

    // Array type — pointer to Array struct (used for both params and returns)
    if (named->name == "Array")   return llvm::PointerType::getUnqual(arrayStructType_);

    // Check for enum types — simple enums use i64, enums with associated values use struct
    auto eit = enumInfos_.find(named->name);
    if (eit != enumInfos_.end()) {
        if (eit->second.hasAssociatedValues && eit->second.structType) {
            return eit->second.structType;
        }
        return llvm::Type::getInt64Ty(*context_);
    }

    // Check for class types — return pointer to struct
    auto it = classInfos_.find(named->name);
    if (it != classInfos_.end()) {
        return llvm::PointerType::getUnqual(*context_);
    }

    return llvm::Type::getInt64Ty(*context_); // fallback
}

llvm::AllocaInst* CodeGen::createEntryBlockAlloca(llvm::Function* func,
                                                    const std::string& name,
                                                    llvm::Type* type) {
    llvm::IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
    return tmpBuilder.CreateAlloca(type, nullptr, name);
}

void CodeGen::emitGenericClassInstance(ClassDecl& templateDecl,
                                        const std::string& mangledName,
                                        const std::vector<std::string>& typeParams,
                                        const std::vector<std::shared_ptr<Type>>& typeArgs)
{
    // Already emitted?
    if (classInfos_.count(mangledName)) return;

    // Build a substitution map: typeParam name -> sema Type
    // Create LLVM struct type
    ClassInfo info;
    info.structType = llvm::StructType::create(*context_, mangledName);

    // Build field types with substitution
    std::vector<llvm::Type*> fieldTypes;
    for (auto& field : templateDecl.fields) {
        // Resolve the field type through substitution
        llvm::Type* ft = llvm::Type::getInt64Ty(*context_); // default
        if (field->typeAnnotation) {
            auto* named = dynamic_cast<NamedType*>(field->typeAnnotation.get());
            if (named) {
                // Check if it's a type parameter
                bool isParam = false;
                for (size_t i = 0; i < typeParams.size(); i++) {
                    if (named->name == typeParams[i]) {
                        ft = getLLVMTypeFromSema(typeArgs[i]);
                        isParam = true;
                        break;
                    }
                }
                if (!isParam) {
                    ft = getLLVMType(field->typeAnnotation.get());
                }
            }
        }
        fieldTypes.push_back(ft);
        info.fieldNames.push_back(field->name);
    }
    info.structType->setBody(fieldTypes);
    classInfos_[mangledName] = info;

    // Declare and emit methods
    auto* structPtrTy = llvm::PointerType::getUnqual(*context_);

    for (auto& method : templateDecl.methods) {
        std::vector<llvm::Type*> paramTypes;
        paramTypes.push_back(structPtrTy); // 'this' pointer

        for (auto& param : method->parameters) {
            if (param.type) {
                auto* named = dynamic_cast<NamedType*>(param.type.get());
                bool isParam = false;
                if (named) {
                    for (size_t i = 0; i < typeParams.size(); i++) {
                        if (named->name == typeParams[i]) {
                            paramTypes.push_back(getLLVMTypeFromSema(typeArgs[i]));
                            isParam = true;
                            break;
                        }
                    }
                }
                if (!isParam) {
                    paramTypes.push_back(getLLVMType(param.type.get()));
                }
            } else {
                paramTypes.push_back(llvm::Type::getInt64Ty(*context_));
            }
        }

        llvm::Type* retType = llvm::Type::getVoidTy(*context_);
        if (method->returnType) {
            auto* named = dynamic_cast<NamedType*>(method->returnType.get());
            if (named && named->name == templateDecl.name) {
                // Constructor returning the generic class — return ptr
                retType = structPtrTy;
            } else if (named) {
                bool isParam = false;
                for (size_t i = 0; i < typeParams.size(); i++) {
                    if (named->name == typeParams[i]) {
                        retType = getLLVMTypeFromSema(typeArgs[i]);
                        isParam = true;
                        break;
                    }
                }
                if (!isParam) {
                    retType = getLLVMType(method->returnType.get());
                }
            } else {
                retType = getLLVMType(method->returnType.get());
            }
        }

        std::string methodMangledName = mangledName + "_" + method->name;
        auto* funcType = llvm::FunctionType::get(retType, paramTypes, false);
        auto* llvmFunc = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                                 methodMangledName, module_.get());
        classInfos_[mangledName].methodNames.push_back(method->name);

        // Emit method body
        auto* bb = llvm::BasicBlock::Create(*context_, "entry", llvmFunc);
        builder_->SetInsertPoint(bb);

        auto oldNamedValues = namedValues_;
        auto oldThisPtr = thisPtr_;
        auto oldClassName = currentClassName_;
        namedValues_.clear();
        currentClassName_ = mangledName;

        // First arg is 'this'
        auto argIt = llvmFunc->arg_begin();
        argIt->setName("this");
        auto* thisAlloca = createEntryBlockAlloca(llvmFunc, "this.addr",
            llvm::PointerType::getUnqual(*context_));
        builder_->CreateStore(&*argIt, thisAlloca);
        thisPtr_ = thisAlloca;
        ++argIt;

        // Remaining args
        size_t idx = 0;
        for (; argIt != llvmFunc->arg_end(); ++argIt, ++idx) {
            if (idx < method->parameters.size()) {
                argIt->setName(method->parameters[idx].name);
                auto* alloca = createEntryBlockAlloca(llvmFunc,
                    method->parameters[idx].name, argIt->getType());
                builder_->CreateStore(&*argIt, alloca);
                namedValues_[method->parameters[idx].name] = alloca;
            }
        }

        // Emit body — we need to temporarily set classInfos_ so that
        // emitConstructExpr can find our mangled name
        // The ConstructExpr inside the method body uses the template class name,
        // so we need a mapping from template name -> mangled name for the duration
        auto savedClassInfoTemplate = classInfos_.find(templateDecl.name);
        bool hadTemplate = (savedClassInfoTemplate != classInfos_.end());
        ClassInfo savedInfo;
        if (hadTemplate) savedInfo = savedClassInfoTemplate->second;
        classInfos_[templateDecl.name] = classInfos_[mangledName];

        for (auto& stmt : method->body->statements) {
            emitStmt(*stmt);
        }

        // Restore template name mapping
        if (hadTemplate) {
            classInfos_[templateDecl.name] = savedInfo;
        } else {
            classInfos_.erase(templateDecl.name);
        }

        auto* currentBlock = builder_->GetInsertBlock();
        if (currentBlock && !currentBlock->getTerminator()) {
            if (llvmFunc->getReturnType()->isVoidTy()) {
                builder_->CreateRetVoid();
            } else {
                builder_->CreateRet(llvm::Constant::getNullValue(llvmFunc->getReturnType()));
            }
        }

        namedValues_ = oldNamedValues;
        thisPtr_ = oldThisPtr;
        currentClassName_ = oldClassName;
    }
}

llvm::Value* CodeGen::emitAwaitExpr(AwaitExpr& expr) {
    // Emit the operand — should be a Future* (i8*)
    llvm::Value* futurePtr = emitExpr(*expr.operand);
    if (!futurePtr) return nullptr;

    // Call chris_async_await(future_ptr) -> i64
    auto* result = builder_->CreateCall(runtimeAsyncAwait_, {futurePtr}, "await.result");
    return result;
}

std::string CodeGen::getIR() const {
    std::string ir;
    llvm::raw_string_ostream stream(ir);
    module_->print(stream, nullptr);
    return ir;
}

bool CodeGen::emitObjectFile(const std::string& outputPath) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();

    auto targetTripleStr = llvm::sys::getDefaultTargetTriple();
    llvm::Triple targetTriple(targetTripleStr);
    module_->setTargetTriple(targetTriple);

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        diagnostics_.error("E4002", "Could not find target: " + error,
                           SourceLocation("<codegen>", 0, 0));
        return false;
    }

    auto cpu = "generic";
    auto features = "";
    llvm::TargetOptions opt;
    auto targetMachine = target->createTargetMachine(targetTriple, cpu, features, opt,
                                                      llvm::Reloc::PIC_);
    module_->setDataLayout(targetMachine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(outputPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        diagnostics_.error("E4003", "Could not open output file: " + ec.message(),
                           SourceLocation("<codegen>", 0, 0));
        return false;
    }

    llvm::legacy::PassManager pass;
    if (targetMachine->addPassesToEmitFile(pass, dest, nullptr,
                                            llvm::CodeGenFileType::ObjectFile)) {
        diagnostics_.error("E4004", "Target machine cannot emit object file",
                           SourceLocation("<codegen>", 0, 0));
        return false;
    }

    pass.run(*module_);
    dest.flush();
    return true;
}

bool CodeGen::linkExecutable(const std::string& objectPath, const std::string& runtimePath,
                              const std::string& outputPath) {
    std::string cmd = "cc " + objectPath + " " + runtimePath + " -lpthread -o " + outputPath;
    int result = std::system(cmd.c_str());
    if (result != 0) {
        diagnostics_.error("E4005", "Linking failed",
                           SourceLocation("<codegen>", 0, 0));
        return false;
    }
    return true;
}

} // namespace chris
