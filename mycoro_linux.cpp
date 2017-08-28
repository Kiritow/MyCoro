#ifndef _WIN32 /// In Linux

#include "mycoro.h"
#include "ucontext.h"
#include <set>

/// Stack Size: 64KB
#define CORO_STACK 64*1024

/// public
struct coro::impl
{
    ucontext_t lastContext;
    ucontext_t thisContext;
    int status;
    std::function<void()>* pfn;
};

coro::coro() : pimpl(new impl)
{
    pimpl->status=-1;
    pimpl->pfn=nullptr;
}

coro::coro(coro&& cc)
{
    pimpl=cc.pimpl;
    cc.pimpl=nullptr;
}

coro::coro(impl* ptr)
{
    pimpl=ptr;
}

coro::~coro()
{
    if(pimpl)
    {
        if(pimpl->status==2||pimpl->status==-1)
        {
            delete pimpl;
        }
        /// if the coro is still running, then don't clean it.
    }
}

int coro::status()
{
    if(pimpl) return pimpl->status;
    else return -1;
}

bool coro::valid()
{
    return (status()!=-1);
}

/// private
struct coro_manager::impl
{
    coro::impl* pRunning;
    std::set<coro::impl*> coset;
    bool wait_all;
};

coro_manager::coro_manager() : pimpl(new impl)
{
    pimpl->pRunning=nullptr;
    pimpl->wait_all=false;
}

/// static
static void _internal_mycoro_clean_up(coro::impl* ptr)
{
    delete ptr->pfn;
    free(ptr->thisContext.uc_stack.ss_sp);
}

coro_manager::~coro_manager()
{
    if(pimpl)
    {
        if(!pimpl->wait_all)
        {
            /// killing mode
            /// clean up resource

            for(auto iter=pimpl->coset.begin();iter!=pimpl->coset.end();++iter)
            {
                _internal_mycoro_clean_up(*iter);
                delete (*iter);
            }
        }
        else
        {
            /// waiting mode
            while(!pimpl->coset.empty())
            {
                coro c(*(pimpl->coset.begin()));
                resume(c);

                /// If the coroutine is finished , then the resource will be cleaned up by coro_manager::resume(), and the record will be erased. The coro::~coro() will cleaned up pimpl.
                /// If the coroutine is not finished, then coro::~coro() will not clean up pimpl. The record will be kept for next loop.
            }

            /// If coset is empty, then all sub-coroutine is finished.
        }

        delete pimpl;
    }
}

/// static
static void _global_mycoro_fn_wrapper(coro::impl* ptr)
{
    ptr->status=1;
    (*(ptr->pfn))();
    ptr->status=2;

    /// Return to previous context
    setcontext(&(ptr->lastContext));
}

void coro_manager::waitAllCoro(bool flag)
{
    pimpl->wait_all=flag;
}

coro coro_manager::create(const std::function<void()>& fn)
{
    coro c;

    /// Get the current context and modify it to a new stack
    getcontext(&(c.pimpl->thisContext));
    c.pimpl->thisContext.uc_link=0;
    void* nblk=malloc(CORO_STACK);
    c.pimpl->thisContext.uc_stack.ss_sp=nblk;
    c.pimpl->thisContext.uc_stack.ss_size=CORO_STACK;
    c.pimpl->thisContext.uc_stack.ss_flags=0;

    if(c.pimpl->thisContext.uc_stack.ss_sp==nullptr)
    {
        /// Failed to allocate stack.
        /// The status of this coroutine will keep -1.(Invalid)
        return c;
    }

    c.pimpl->pfn=new std::function<void()>(fn);

    /// Inspired by 'BiKeDaMoWang'
    makecontext(&(c.pimpl->thisContext),(void(*)())_global_mycoro_fn_wrapper,1,c.pimpl);

    /// Set status to ready.
    c.pimpl->status=1;

    /// add to record
    pimpl->coset.insert(c.pimpl);
    return c;
}

bool coro_manager::resume(coro& c)
{
    if(c.status()!=1)
    {
        /// Not Resumable.
        return false;
    }

    /// Switch to running
    c.pimpl->status=0;
    coro::impl* oldRunning=pimpl->pRunning;
    pimpl->pRunning=c.pimpl;
    swapcontext(&(c.pimpl->lastContext),&(c.pimpl->thisContext));
    pimpl->pRunning=oldRunning;
    /// Will only return from swapcontext if called yield() or the coro is finished.

    if(c.pimpl->status==2)
    {
        /// coroutine finished.
        /// clean up resource
        _internal_mycoro_clean_up(c.pimpl);
        /// remove from record
        pimpl->coset.erase(c.pimpl);

        /// If the coroutine is finished,
        /// the resource will be cleaned up by coro::~coro()
    }

    return true;
}

bool coro_manager::yield()
{
    if(pimpl->pRunning==nullptr)
    {
        /// Failed to yield
        return false;
    }

    pimpl->pRunning->status=1;
    swapcontext(&(pimpl->pRunning->thisContext),&(pimpl->pRunning->lastContext));
    pimpl->pRunning->status=0;

    return true;
}

#endif /// End of Linux System definition
