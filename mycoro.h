#pragma once
#include <functional>

class coro
{
public:
    coro();
    coro(const coro&)=delete;
    coro& operator = (const coro&)=delete;
    coro(coro&&);
    coro& operator = (coro&&)=delete;
    ~coro();

    /// Status:
    /// -1 Invalid
    /// 0 Running
    /// 1 Suspended
    /// 2 Stopped
    int status();

    bool valid();
public:
    struct impl;
    impl* pimpl;

private:
    /// This constructor should only be called by coro_manager.
    coro(impl*);
    friend class coro_manager;
};

class coro_manager
{
public:
    coro_manager();
    coro_manager(const coro_manager& )=delete;
    coro_manager& operator =(const coro_manager&)=delete;
    coro_manager(coro_manager&&)=delete;
    coro_manager& operator =(coro_manager&&)=delete;
    ~coro_manager();

    /// Change the behavior of coro_manager while deconstructing
    /// If flag is true, the coro_manager will wait for all coroutines create by this manager to finish. [Waiting Mode]
    /// If flag is false, the coro_manager will kill all coroutines create by this manager. [Killing Mode] (by default)
    /// Resource will always be cleaned up whether the flag is.
    /// However, in killing mode, deconstructor of local objects may not be called while coroutines are killed, which may cause memory leak.
    void waitAllCoro(bool flag);

    /// Create a new coroutine
    coro create(const std::function<void()>& fn);

    /// Return Value
    /// true: return from another coro. (Resume successfully change the control flow to another fiber)
    /// false: coro& c is not resumable. (maybe invalid or finished.)
    bool resume(coro& c);

    /// Reture Value
    /// true: return from another coro. (Yield successfully change the control flow to another fiber.)
    /// false: Cannot yield. (This fiber is exactly the main fiber)
    bool yield();

private:
    struct impl;
    impl* pimpl;
};
