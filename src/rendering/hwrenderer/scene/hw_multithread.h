#pragma once


EXTERN_CVAR(Int, gl_multithread)


struct RenderJob
{
	enum
	{
		TerminateJob,	// inserted when all work is done so that the worker can return.
		FlatJob,
		WallJob,
		SpriteJob,
		ParticleJob,
		PortalJob,
	};
	
	int type;
	subsector_t *sub;
	seg_t *seg;
};

struct BufferJob
{
	enum
	{
		TerminateJob,	// inserted when all work is done so that the worker can return.
		FlatVertexJob,
		WallVertexJob,
		FlatLightJob,
		WallLightJob,
		SpriteJob,
	};

	int type;
	int param;
	void* obj;	// can be HWWall, HWFlat or HWSprite
};

// Note: This queue may only have one thread writing to it. Otherwise it would require a mutex for synchronization and lose its effectiveness.
// GZDoom never uses it in a context with more than one writer thread.
template<class Job, class T1, class T2>
class JobQueue
{
	Job pool[300000];	// Way more than ever needed. The largest ever seen on a single viewpoint is around 40000.
	std::atomic<int> readindex{};
	std::atomic<int> writeindex{};
public:
	void AddJob(int type,  T1 v1, T2 v2 = 0)
	{
		// This does not check for array overflows. The pool should be large enough that it never hits the limit.

		pool[writeindex] = { type, v1, v2 };
		writeindex++;
	}

	Job *GetJob()
	{
		if (readindex < writeindex) return &pool[readindex++];
		return nullptr;
	}
	
	void ReleaseAll()
	{
		readindex = 0;
		writeindex = 0;
	}
};

extern JobQueue<RenderJob, subsector_t*, seg_t *> renderJobQueue;	// One static queue is sufficient here. This code will never be called recursively.
extern JobQueue<BufferJob, int, void *> bufferJobQueue;	// One static queue is sufficient here. This code will never be called recursively.
