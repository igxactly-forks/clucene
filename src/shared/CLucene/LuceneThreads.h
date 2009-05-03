/*------------------------------------------------------------------------------
* Copyright (C) 2003-2006 Ben van Klinken and the CLucene Team
* 
* Distributable under the terms of either the Apache License (Version 2.0) or 
* the GNU Lesser General Public License, as specified in the COPYING file.
------------------------------------------------------------------------------*/
#ifndef _LuceneThreads_h
#define  _LuceneThreads_h


CL_NS_DEF(util)
class CLuceneThreadIdCompare;

#if defined(_CL_DISABLE_MULTITHREADING)
	#define SCOPED_LOCK_MUTEX(theMutex)
	#define DEFINE_MUTEX(x)
	#define DEFINE_MUTABLE_MUTEX(x)
	#define STATIC_DEFINE_MUTEX(x)
	#define CONDITION_WAIT(theMutex, theCondition)
	#define CONDITION_NOTIFYALL(theCondition)
	#define _LUCENE_CURRTHREADID 1
	#define _LUCENE_THREADID_TYPE char
	#define _LUCENE_THREAD_FUNC(name, argName) int name(void* argName)
	#define _LUCENE_THREAD_FUNC_RETURN(val) return (int)val;
	#define _LUCENE_THREAD_CREATE(func, arg) (*func)(arg)
	#define _LUCENE_THREAD_JOIN(value) //nothing to do...

#else
	#if defined(_LUCENE_DONTIMPLEMENT_THREADMUTEX)
		//do nothing
    #else
    	 #if defined(_CL_HAVE_PTHREAD)
          #define _LUCENE_THREADID_TYPE pthread_t
        	#define _LUCENE_THREAD_FUNC(name, argName) void* name(void* argName) //< use this macro to correctly define the thread start routine
        	#define _LUCENE_THREAD_FUNC_RETURN(val) return (int)val;
          typedef void* (luceneThreadStartRoutine)(void* lpThreadParameter );
          class CLUCENE_SHARED_EXPORT mutex_thread
          {
          private:
              struct Internal;
              Internal* _internal;
          public:
          	mutex_thread(const mutex_thread& clone);
          	mutex_thread();
          	~mutex_thread();
          	void lock();
          	void unlock();
          	static _LUCENE_THREADID_TYPE _GetCurrentThreadId();
        		static _LUCENE_THREADID_TYPE CreateThread(luceneThreadStartRoutine* func, void* arg);
        		static void JoinThread(_LUCENE_THREADID_TYPE id);
        		void Wait(mutex_thread* shared_lock);
        		void NotifyAll();
          };
					class CLUCENE_SHARED_EXPORT shared_condition{
        	private:
        		class Internal;
        		Internal* _internal;
        	public:
        		shared_condition();
        		~shared_condition();
						void Wait(mutex_thread* shared_lock);
        		void NotifyAll();
					};
    
    	#elif defined(_CL_HAVE_WIN32_THREADS)
        	#define _LUCENE_THREADID_TYPE uint64_t
    	    #define _LUCENE_THREAD_FUNC(name, argName) void __stdcall name(void* argName) //< use this macro to correctly define the thread start routine
			#define _LUCENE_THREAD_FUNC_RETURN(val) CL_NS(util)::mutex_thread::_exitThread(val)
          typedef void (__stdcall luceneThreadStartRoutine)(void* lpThreadParameter );
          class CLUCENE_SHARED_EXPORT mutex_thread
        	{
        	private:
        		struct Internal;
        		Internal* _internal;
        	public:
        		mutex_thread(const mutex_thread& clone);
        		mutex_thread();
        		~mutex_thread();
        		void lock();
        		void unlock();
						static void _exitThread(int ret);
        		static _LUCENE_THREADID_TYPE _GetCurrentThreadId();
        		static _LUCENE_THREADID_TYPE CreateThread(luceneThreadStartRoutine* func, void* arg);
        		static void JoinThread(_LUCENE_THREADID_TYPE id);
        	};
			class CLUCENE_SHARED_EXPORT shared_condition{
        	private:
        		class Internal;
        		Internal* _internal;
        	public:
        		shared_condition();
        		~shared_condition();
				void Wait(mutex_thread* shared_lock);
        		void NotifyAll();
			};
    	#else
    		#error A valid thread library was not found
    	#endif //mutex types
    	
    	
    	#define _LUCENE_THREAD_CREATE(func, arg) CL_NS(util)::mutex_thread::CreateThread(func,arg)
    	#define _LUCENE_THREAD_JOIN(id) CL_NS(util)::mutex_thread::JoinThread(id)
        #define _LUCENE_CURRTHREADID CL_NS(util)::mutex_thread::_GetCurrentThreadId()
      #define _LUCENE_THREADMUTEX CL_NS(util)::mutex_thread
      #define _LUCENE_THREADCOND CL_NS(util)::shared_condition
    #endif //don't implement
	
	/** @internal */
	class CLUCENE_SHARED_EXPORT mutexGuard
	{
	private:
		_LUCENE_THREADMUTEX* mrMutex;
		mutexGuard(const mutexGuard& clone);
	public:
		mutexGuard( _LUCENE_THREADMUTEX& rMutex );
		~mutexGuard();
	};
	
	#define SCOPED_LOCK_MUTEX(theMutex) 				CL_NS(util)::mutexGuard theMutexGuard(theMutex);
	#define DEFINE_MUTEX(theMutex) 							_LUCENE_THREADMUTEX theMutex;
	#define DEFINE_CONDITION(theCondition) 			_LUCENE_THREADCOND theCondition;
	#define DEFINE_MUTABLE_MUTEX(theMutex)  		mutable _LUCENE_THREADMUTEX theMutex;
	#define STATIC_DEFINE_MUTEX(theMutex) 			static _LUCENE_THREADMUTEX theMutex;
	
	#define CONDITION_WAIT(theMutex, theCondition)	theCondition.Wait(&theMutex);
	#define CONDITION_NOTIFYALL(theCondition)				theCondition.NotifyAll();

#endif //_CL_DISABLE_MULTITHREADING
CL_NS_END

#endif
