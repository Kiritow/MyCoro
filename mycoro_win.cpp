#ifdef _WIN32 /// Windows

#include "mycoro.h"
#include <set>
#include <windows.h>

/// public (!)
struct coro::impl
{
    LPVOID fiber;
    LPVOID last_fiber;/// fiber to switch back
    int status;
    std::function<void()>* pfn;
};

coro::coro() : pimpl(new impl)
{
    pimpl->status=-1;
    pimpl->fiber=NULL;
    pimpl->last_fiber=NULL;
    pimpl->pfn=nullptr;
}

coro::coro(coro&& cc)
{
    pimpl=cc.pimpl;
    cc.pimpl=nullptr;
}

/// private
coro::coro(impl* ptr)
{
    /// This constructor does not allocate a struct impl.
    /// The constructor should only be called in coro_manager::~coro_manager() for calling coro_manager::resume() cleaning resource use.
    /// After calling coro_manager::resume(), the caller (coro_manager::~coro_manager()) should set it to null.
    pimpl=ptr;
}

coro::~coro()
{
    /// coro::~coro() should only manage allocating and deallocating memory
    /// and struct coro::impl should not have user-defined deconstructor.
    if(pimpl)
    {
        if(pimpl->status==1||pimpl->status==0)
        {
            /// If the coroutine is still running, then the coro class should not release the resource.
            /// And the coro_manager should take responsibility of releasing resource.
        }
        else
        {
            /// status=-1, status=2
            delete pimpl;
        }
    }
}

int coro::status()
{
    return pimpl->status;
}

bool coro::valid()
{
    return (status()!=-1);
}

/// private
struct coro_manager::impl
{
    bool converted;
    bool waitall;

    std::set<coro::impl*> coset;
};

coro_manager::coro_manager() : pimpl(new impl)
{
    pimpl->converted=false;
    pimpl->waitall=false;
}

/// static
static void CALLBACK _global_mycoro_fn_wrapper(void* inptr)
{
    coro::impl* ptr=(coro::impl*)inptr;
    ptr->status=0;
    (*(ptr->pfn))();
    ptr->status=2;

    /// Switch back after function is finished.(will return from coro_manager::resume() call)
    /// and coro_manager::resume() must handle resource cleaning up.
    SwitchToFiber(ptr->last_fiber);
}

void coro_manager::waitAllCoro(bool flag)
{
    pimpl->waitall=flag;
}

coro coro_manager::create(const std::function<void()>& fn)
{
    coro c;
    /// After construction of coro, the status of coro must be -1.(Invalid)

    if(!pimpl->converted)
    {
        if(NULL==ConvertThreadToFiber(NULL))
        {
            /// Failed to convert thread to fiber.
            /// return c as an invalid coro.
            return c;
        }
        else
        {
            pimpl->converted=true;
        }
    }

    if(nullptr!=(c.pimpl->fiber=CreateFiber(0,_global_mycoro_fn_wrapper,c.pimpl)))
    {
        /// Successfully created.
        c.pimpl->status=1;
        c.pimpl->pfn=new std::function<void()>(fn);
        /// Add to record set
        pimpl->coset.insert(c.pimpl);
        return c;
    }
    else
    {
        /// c.pimpl->status will keep as -1.(Invalid)
        return c;
    }
}

//static
/// This is an internal function and should only be called by coro_manager::resume() and coro_manager::~coro_manager()
static void _internal_mycoro_cleaning_up(coro::impl* ptr)
{
    /// Do cleaning up jobs
    DeleteFiber(ptr->fiber);
    ptr->fiber=NULL;
    ptr->last_fiber=NULL;
    delete ptr->pfn;
    ptr->pfn=nullptr;
}

bool coro_manager::resume(coro& c)
{
    if(c.status()!=1)
    {
        /// Cannot resume.
        return false;
    }

    c.pimpl->last_fiber=GetCurrentFiber();
    c.pimpl->status=0;
    SwitchToFiber(c.pimpl->fiber);

    if(c.pimpl->status==2)
    {
        /// return from _global_mycoro_fn_wrapper. Finished

        /// Do cleaning up jobs
        _internal_mycoro_cleaning_up(c.pimpl);
        /// Remove from record set
        pimpl->coset.erase(c.pimpl);

        /// c.pimpl is not cleaned by coro_manager::resume(). It should be cleaned up by coro::~coro().
    }
    /// else c.pimpl->status==1.

    return true;
}

bool coro_manager::yield()
{
    coro::impl* ptr=(coro::impl*)GetFiberData();
    if(ptr==nullptr)
    {
        return false;
    }

    ptr->status=1;
    SwitchToFiber(ptr->last_fiber);
    ptr->status=0;

    return true;
}

coro_manager::~coro_manager()
{
    if(!pimpl->waitall)
    {
        /// Killing Mode
        /// Clean up resource.
        for(auto iter=pimpl->coset.begin();iter!=pimpl->coset.end();++iter)
        {
            /// Do cleaning up jobs
            _internal_mycoro_cleaning_up(*iter);
            /// Delete it. (as the coro has been deconstructed)
            delete (*iter);
        }
    }
    else
    {
        /// Waiting Mode
        /// Wait for all coroutines...
        while(!pimpl->coset.empty())
        {
            /// Special Initialization
            coro c(*(pimpl->coset.begin()));
            resume(c);
            /// If c is finished, then the fiber resource will be cleaned up by coro_manager::resume(). The pimpl pointer will be erased from coset.
            /// In deconstructor of c, if the coro has stopped, then coro::~coro() will clean up the resource itself.
            /// Otherwise, the coro::~coro() will keep the pimpl pointer.
        }
    }

    if(pimpl->converted)
    {
        /// Convert Fiber back to thread.
        ConvertFiberToThread();
    }

    if(pimpl)
        delete pimpl;
}

#endif /// End of Windows System definition.
