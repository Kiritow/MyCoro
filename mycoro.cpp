#include <windows.h>
#include <cstdio>
#include <vector>
#include <iostream>

using namespace std;

/** Win32 API Fiber Functions
ConvertThreadToFiber 切换线程为协程
CreateFiber 创建协程/但不运行
SwitchToFiber 切换协程
DeleteFiber 删除协程
*/

typedef void (*VFUNC)();

struct info
{
    /// 0 Running 1 Ready 2 Finished
    int status;
    LPVOID fiber;
};

vector<info*> vec;
int cur_id;
int finished_cnt;

/// API Reference
void Init();
void Add(VFUNC fn);
bool yield();
void StopAndCleanUp();
void WaitAndCleanUp();

#ifdef HC_DEBUG
#define dprintf(fmt,args...) printf("%s: " fmt ,__func__,##args)
#else
#define dprintf(fmt,args...)
#endif

void Init()
{
    info* m=new info;
    m->fiber=ConvertThreadToFiber(NULL);
    m->status=0;
    vec.push_back(m);

    cur_id=0;
    finished_cnt=0;
}

void StopAndCleanUp()
{
    int sz=vec.size();
    for(int i=1;i<sz;i++)
    {
        DeleteFiber(vec[i]->fiber);
        delete vec[i];
        vec[i]=nullptr;
    }

    ConvertFiberToThread();
}

void WaitAndCleanUp()
{
    /// wait all fiber
    while(yield());

    ConvertFiberToThread();
}

struct pack
{
    VFUNC fn;
    info* pinfo;
};

void CALLBACK Wrapper(void* ptr)
{
    pack* ppack=(pack*)ptr;
    ppack->fn();
    dprintf("Task Finished. In %d\n",cur_id);
    ppack->pinfo->status=2;
    delete ppack;
    yield();
}

void Add(VFUNC fn)
{
    info* m=new info;

    pack* ppack=new pack;
    ppack->fn=fn;
    ppack->pinfo=m;

    m->fiber=CreateFiber(0,Wrapper,ppack);
    m->status=1;
    vec.push_back(m);
}

bool yield()
{
    dprintf("call. In %d\n",cur_id);

    if(vec.size()>1)
    {
        if(vec[cur_id]->status==0)
        {
            vec[cur_id]->status=1;

            /// Only try to call DeleteFiber in running fiber.
            if(finished_cnt)
            {
                dprintf("Found %d finished task. Cleanning...\n",finished_cnt);

                int sz=vec.size();
                for(int i=0;i<sz;i++)
                {
                    if(vec[i]&&vec[i]->status==2)
                    {
                        dprintf("Deleting Fiber %d\n",i);

                        DeleteFiber(vec[i]->fiber);
                        delete vec[i];
                        vec[i]=nullptr;
                        --finished_cnt;
                    }
                }
            }
        }
        else if(vec[cur_id]->status==2)
        {
            ++finished_cnt;
        }

        int nextid=-1;

        int sz=vec.size();

        for(int i=cur_id+1;i<sz;i++)
        {
            if(vec[i]&&vec[i]->status==1)
            {
                nextid=i;
                break;
            }
        }

        if(nextid<0)
        {
            for(int i=0;i<cur_id;i++)
            {
                if(vec[i]&&vec[i]->status==1)
                {
                    nextid=i;
                    break;
                }
            }
        }

        if(nextid<0)
        {
            /// Cannot switch
            dprintf("unable to switch. In %d\n",cur_id);
            vec[cur_id]->status=0;
            return false;
        }

        dprintf("switch %d to %d\n",cur_id,nextid);

        cur_id=nextid;

        vec[cur_id]->status=0;
        SwitchToFiber(vec[cur_id]->fiber);

        return true;
    }
    else
    {
        dprintf("skip yield\n");

        return false;
    }
}

#undef dprintf

void workproc()
{
    for(int i=0;i<10;i++)
    {
        cout<<"In WorkProc "<<i<<endl;
        bool ret=yield();
        cout<<"In WorkProc "<<i<<" yield Return "<<ret<<endl;
    }
}

void cpu_heavy_proc()
{
    for(int i=0;i<5;i++)
    {
        cout<<"In CPU Heavy Proc "<<i<<endl;
        for(int c=0;c<1e9+7;++c);
        yield();
    }
}

int main()
{
    Init();
    Add(workproc);
    Add(cpu_heavy_proc);

    for(int i=0;i<5;i++)
    {
        cout<<"In Main "<<i<<endl;
        bool ret=yield();
        cout<<"In Main "<<i<<" yield Return "<<ret<<endl;
    }

    WaitAndCleanUp();

    cout<<"Finished?\n"<<endl;
    return 0;
}
