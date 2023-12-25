/**
 * @file llinstancetracker.h
 * @brief LLInstanceTracker is a mixin class that automatically tracks object
 *		  instances with or without an associated key
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLINSTANCETRACKER_H
#define LL_LLINSTANCETRACKER_H

#if LL_WINDOWS
# pragma warning (push)
# pragma warning (disable:4265)
#endif
#include <mutex>
#if LL_WINDOWS
# pragma warning (pop)
#endif

#include <memory>
#include <typeinfo>
#include <vector>

#include "boost/iterator/filter_iterator.hpp"
#include "boost/iterator/indirect_iterator.hpp"
#include "boost/iterator/transform_iterator.hpp"

#include "llerror.h"
#include "hbfastmap.h"
#include "hbfastset.h"

namespace LLInstanceTrackerPrivate
{

struct StaticBase
{
	// We need to be able to lock static data while manipulating it.
	std::mutex mMutex;
};

// Instantiate this template to obtain a pointer to the canonical static
// instance of Static while holding a lock on that instance. Use of
// Static::mMutex presumes that Static declares some suitable mMutex.
// NOTE: this template class is defined in a lockstatic.h header in LL's
// sources, but since it is only used by LLInstanceTracker, I moved it here. HB
template <typename Static>
class LockStatic
{
	typedef std::unique_lock<decltype(Static::mMutex)> lock_t;

public:
	LockStatic()
	:	mData(getStatic()),
		mLock(mData->mMutex)
	{
	}

	LL_INLINE Static* get() const			{ return mData; }
	LL_INLINE operator Static*() const		{ return get(); }
	LL_INLINE Static* operator->() const	{ return get(); }

	// Sometimes we must explicitly unlock...
	LL_INLINE void unlock()
	{
		// ... But once we do, access is no longer permitted !
		mData = NULL;
		mLock.unlock();
	}

private:
	Static* getStatic()
	{
		// Static::mMutex must be function-local static rather than class-
		// static. Some of our consumers must function properly (therefore
		// lock properly) even when the containing module's static variables
		// have not yet been runtime-initialized while a mutex requires
		// construction and a static class member might not yet have been
		// constructed.
		// We could store a dumb mutex_t*, notice when it is NULL and allocate
		// a heap mutex, but this is vulnerable to race conditions. And we
		// cannot defend the dumb pointer with another mutex.
		// We could store a std::atomic<mutex_t*> but a default-constructed T !
		// Which means std::atomic, too, requires runtime initialization.
		// A function-local static is guaranteed to be initialized exactly
		// once: the first time control reaches that declaration.
		static Static sData;
		return &sData;
	}

protected:
	Static*		mData;
	lock_t		mLock;
};

}	// End namespace LLInstanceTrackerPrivate

enum EInstanceTrackerAllowKeyCollisions
{
	LLInstanceTrackerErrorOnCollision,
	LLInstanceTrackerReplaceOnCollision
};

// This mix-in class adds support for tracking all instances of the specified
// class parameter T. The (optional) key associates a value of type KEY with a
// given instance of T, for quick lookup. If KEY is not provided, then
// instances are stored into a simple unordered_set.
// NOTE: see explicit specialization below for default KEY==void case
template<typename T, typename KEY = void,
		 EInstanceTrackerAllowKeyCollisions KEY_COLLISION_BEHAVIOR =
			LLInstanceTrackerErrorOnCollision>
class LLInstanceTracker
{
	typedef typename flat_hmap<KEY, std::shared_ptr<T> > InstanceMap;
	struct StaticData : public LLInstanceTrackerPrivate::StaticBase
	{
		InstanceMap mMap;
	};
	typedef LLInstanceTrackerPrivate::LockStatic<StaticData> LockStatic;

public:
	using ptr_t = std::shared_ptr<T>;
	using weak_t = std::weak_ptr<T>;

	// No-copy
	LLInstanceTracker(const LLInstanceTracker&) = delete;
	const LLInstanceTracker& operator=(const LLInstanceTracker&) = delete;

	// Storing a dumb T* somewhere external is a bad idea, since the
	// LLInstanceTracker subclasses are explicitly destroyed rather than
	// managed by smart pointers. It is legal to declare stack instances of an
	// LLInstanceTracker subclass.
	// But it is reasonable to store a std::weak_ptr<T>, which will become
	// invalid when the T instance is destroyed.
	LL_INLINE weak_t getWeak()				{ return mSelf; }

	LL_INLINE static size_t instanceCount()	{ return LockStatic()->mMap.size(); }

	// Snapshot of std::pair<const KEY, std::shared_ptr<T>> pairs
	class snapshot
	{
	private:
		// It is very important that what we store in this snapshot are weak
		// pointers, NOT shared pointers. This is how we discover whether any
		// instance has been deleted during the lifespan of a snapshot.
		typedef std::vector<std::pair<const KEY, weak_t> > VectorType;
		// Dereferencing our iterator produces a std::shared_ptr for each
		// instance that still exists. Since we store weak_ptrs, that involves
		// two chained transformations:
		//  - A transform_iterator to lock the weak_ptr and return a shared_ptr
		//  - A filter_iterator to skip any shared_ptr that has become invalid.
		// It is very important that we filter lazily, that is, during
		// traversal. Any one of our stored weak_ptrs might expire during
		// traversal.
		typedef std::pair<const KEY, ptr_t> strong_pair;
		// Note for future reference: Nat has not yet had any luck (up to Boost
		// 1.67) trying to use boost::transform_iterator with a hand-coded
		// functor, only with actual functions. In my experience, an internal
		// boost::result_of() operation fails, even with an explicit
		// result_type typedef. But this works.
		LL_INLINE static strong_pair strengthen(typename VectorType::value_type& pair)
		{
			return { pair.first, pair.second.lock() };
		}

		LL_INLINE static bool dead_skipper(const strong_pair& pair)
		{
			return bool(pair.second);
		}

	public:
		snapshot()
		// Populate our vector with a snapshot of (locked !) InstanceMap
		// Note: this assigns pair<KEY, shared_ptr> to pair<KEY, weak_ptr>
		:	mData(mLock->mMap.begin(), mLock->mMap.end())
		{
			// Release the lock once we have populated mData
			mLock.unlock();
		}

		// You cannot make a transform_iterator (or anything else) that
		// literally stores a C++ function (decltype(strengthen)), but you can
		// make a transform_iterator based on a _function pointer._
		typedef boost::transform_iterator<decltype(strengthen)*,
										  typename VectorType::iterator> strong_iterator;
		typedef boost::filter_iterator<decltype(dead_skipper)*,
									   strong_iterator> iterator;

		LL_INLINE iterator begin()			{ return make_iterator(mData.begin()); }
		LL_INLINE iterator end()			{ return make_iterator(mData.end()); }

	private:
		iterator make_iterator(typename VectorType::iterator iter)
		{
			// transform_iterator only needs the base iterator and the
			// transform. filter_iterator wants the predicate and both ends of
			// the range.
			return iterator(dead_skipper,
							strong_iterator(iter, strengthen),
							strong_iterator(mData.end(), strengthen));
		}

	private:
		// Lock static data during construction
#if !LL_WINDOWS
		LockStatic mLock;
#else
		// We want to be able to use (e.g.) our instance_snapshot subclass as:
		// for (auto& inst : T::instance_snapshot()) ...
		// But when this snapshot base class directly contains LockStatic, as
		// above, VS2017 requires us to code instead:
		// for (auto& inst : std::move(T::instance_snapshot())) ...
		// Nat thinks this should be unnecessary, as an anonymous class
		// instance is already a temporary. It should not need to be cast to
		// rvalue reference (the role of std::move()). clang evidently agrees,
		// as the short form works fine with Xcode on Mac.
		// To support the succinct usage, instead of directly storing
		// LockStatic, store std::shared_ptr<LockStatic>, which is copyable.
		std::shared_ptr<LockStatic> mLockp{ std::make_shared<LockStatic>() };
		LockStatic& mLock{ *mLockp };
#endif
		VectorType mData;
	};

	// Iterate over this for references to each instance
	class instance_snapshot : public snapshot
	{
	private:
		LL_INLINE static T& instance_getter(typename snapshot::iterator::reference pair)
		{
			return *pair.second;
		}

	public:
		typedef boost::transform_iterator<decltype(instance_getter)*,
										  typename snapshot::iterator> iterator;

		LL_INLINE iterator begin()
		{
			return iterator(snapshot::begin(), instance_getter);
		}

		LL_INLINE iterator end()
		{
			return iterator(snapshot::end(), instance_getter);
		}

		LL_INLINE void deleteAll()
		{
			for (auto it = snapshot::begin(), end = snapshot::end();
				 it != end; ++it)
			{
				delete it->second.get();
			}
		}
	};

	// Iterate over this for each key
	class key_snapshot : public snapshot
	{
	 private:
		LL_INLINE static KEY key_getter(typename snapshot::iterator::reference pair)
		{
			return pair.first;
		}

	public:
		typedef boost::transform_iterator<decltype(key_getter)*,
										  typename snapshot::iterator> iterator;

		LL_INLINE iterator begin()
		{
			return iterator(snapshot::begin(), key_getter);
		}

		LL_INLINE iterator end()
		{
			return iterator(snapshot::end(),   key_getter);
		}
	};

	// Note: renamed from 'getInstance()' since otherwise it conflicts with
	// LLSingleton::getInstance() when the class is both a singleton and a
	// tracked instance... HB
	static ptr_t getNamedInstance(const KEY& k)
	{
		LockStatic lock;
		const InstanceMap& map = lock->mMap;
		typename InstanceMap::const_iterator found = map.find(k);
		return found == map.end() ? NULL : found->second;
	}

	// While iterating over instances, we might want to request the key
	LL_INLINE virtual const KEY& getKey() const	{ return mInstanceKey; }

protected:
	LLInstanceTracker(KEY key)
	{
		// We do not intend to manage the lifespan of this object with
		// shared_ptr, so give it a no-op deleter. We store shared_ptrs in our
		// InstanceMap specifically so snapshot can store weak_ptrs so we can
		// detect deletions during traversals.
		ptr_t ptr((T*)this, [](T*){});
		// Save corresponding weak_ptr for future reference
		mSelf = ptr;
		LockStatic lock;
		add_(lock, key, ptr);
	}

	virtual ~LLInstanceTracker()
	{
		LockStatic lock;
		remove_(lock);
	}

private:
	void add_(LockStatic& lock, const KEY& key, const ptr_t& ptr)
	{
		mInstanceKey = key;
		InstanceMap& map = lock->mMap;
		if (KEY_COLLISION_BEHAVIOR == LLInstanceTrackerErrorOnCollision)
		{
			auto pair = map.emplace(key, ptr);
			if (!pair.second)
			{
				llerrs << "Key " << key
					   << " already exists in instance map for "
					   << LLError::className(typeid(*this)) << llendl;
			}
		}
		else
		{
			map[key] = ptr;
		}
	}

	ptr_t remove_(LockStatic& lock)
	{
		InstanceMap& map = lock->mMap;
		typename InstanceMap::iterator iter = map.find(mInstanceKey);
		if (iter != map.end())
		{
			auto ret = iter->second;
			map.erase(iter);
			return ret;
		}
		return {};
	}

private:
	// Storing a weak_ptr to self is a bit like deriving from
	// std::enable_shared_from_this(), except more explicit.
	weak_t	mSelf;
	KEY		mInstanceKey;
};

// Explicit specialization for default case where KEY is void using an
// unordered set<T*>
template<typename T, EInstanceTrackerAllowKeyCollisions KEY_COLLISION_BEHAVIOR>
class LLInstanceTracker<T, void, KEY_COLLISION_BEHAVIOR>
{
	typedef typename flat_hset<std::shared_ptr<T> > InstanceSet;
	struct StaticData : public LLInstanceTrackerPrivate::StaticBase
	{
		InstanceSet mSet;
	};
	typedef LLInstanceTrackerPrivate::LockStatic<StaticData> LockStatic;

public:
	using ptr_t  = std::shared_ptr<T>;
	using weak_t = std::weak_ptr<T>;

	// Storing a dumb T* somewhere external is a bad idea, since the
	// LLInstanceTracker subclasses are explicitly destroyed rather than
	// managed by smart pointers. It is legal to declare stack instances of an
	// LLInstanceTracker subclass.
	// But it is reasonable to store a std::weak_ptr<T>, which will become
	// invalid when the T instance is destroyed.
	LL_INLINE weak_t getWeak()				{ return mSelf; }

	LL_INLINE static size_t instanceCount()	{ return LockStatic()->mSet.size(); }

	// Snapshot of std::shared_ptr<T> pointers
	class snapshot
	{
	private:
		// It is very important that what we store in this snapshot are weak
		// pointers, NOT shared pointers. This is how we discover whether any
		// instance has been deleted during the lifespan of a snapshot.
		typedef std::vector<weak_t> VectorType;
		// Dereferencing our iterator produces a std::shared_ptr for each
		// instance that still exists. Since we store weak_ptrs, that involves
		// two chained transformations:
		//  - A transform_iterator to lock the weak_ptr and return a shared_ptr
		//  - A filter_iterator to skip any shared_ptr that has become invalid.
		typedef std::shared_ptr<T> strong_ptr;
		LL_INLINE static strong_ptr strengthen(typename VectorType::value_type& ptr)
		{
			return ptr.lock();
		}

		LL_INLINE static bool dead_skipper(const strong_ptr& ptr)
		{
			return bool(ptr);
		}

	public:
		snapshot()
		// Populate our vector with a snapshot of (locked !) InstanceSet
		// Note: this assigns pair<KEY, shared_ptr> to pair<KEY, weak_ptr>
		:	mData(mLock->mSet.begin(), mLock->mSet.end())
		{
			// Release the lock once we have populated mData
			mLock.unlock();
		}

		typedef boost::transform_iterator<decltype(strengthen)*,
										  typename VectorType::iterator> strong_iterator;
		typedef boost::filter_iterator<decltype(dead_skipper)*,
									   strong_iterator> iterator;

		LL_INLINE iterator begin()			{ return make_iterator(mData.begin()); }
		LL_INLINE iterator end()			{ return make_iterator(mData.end()); }

	private:
		iterator make_iterator(typename VectorType::iterator iter)
		{
			// transform_iterator only needs the base iterator and the
			// transform. filter_iterator wants the predicate and both ends of
			// the range.
			return iterator(dead_skipper,
							strong_iterator(iter, strengthen),
							strong_iterator(mData.end(), strengthen));
		}

	private:
		// Lock static data during construction
#if !LL_WINDOWS
		LockStatic mLock;
#else
		// We want to be able to use (e.g.) our instance_snapshot subclass as:
		// for (auto& inst : T::instance_snapshot()) ...
		// But when this snapshot base class directly contains LockStatic, as
		// above, VS2017 requires us to code instead:
		// for (auto& inst : std::move(T::instance_snapshot())) ...
		// Nat thinks this should be unnecessary, as an anonymous class
		// instance is already a temporary. It shouldn't need to be cast to
		// rvalue reference (the role of std::move()). clang evidently agrees,
		// as the short form works fine with Xcode on Mac.
		// To support the succinct usage, instead of directly storing
		// LockStatic, store std::shared_ptr<LockStatic>, which is copyable.
		std::shared_ptr<LockStatic> mLockp{ std::make_shared<LockStatic>() };
		LockStatic& mLock{ *mLockp };
#endif
		VectorType mData;
	};

	// Iterate over this for references to each instance
	class instance_snapshot : public snapshot
	{
	public:
		typedef boost::indirect_iterator<typename snapshot::iterator> iterator;

		LL_INLINE iterator begin()
		{
			return iterator(snapshot::begin());
		}

		LL_INLINE iterator end()
		{
			return iterator(snapshot::end());
		}

		LL_INLINE void deleteAll()
		{
			for (auto it = snapshot::begin(), end = snapshot::end();
				 it != end; ++it)
			{
				delete it->get();
			}
		}
	};

protected:
	LLInstanceTracker()
	{
		// Since we do not intend for this shared_ptr to manage lifespan, give
		// it a no-op deleter.
		std::shared_ptr<T> ptr((T*)this, [](T*){});
		// Save corresponding weak_ptr for future reference
		mSelf = ptr;
		// Also store it in our class-static set to track this instance.
		LockStatic()->mSet.emplace(ptr);
	}

	virtual ~LLInstanceTracker()
	{
		// Convert weak_ptr to shared_ptr because this is what we store in our
		// InstanceSet.
		LockStatic()->mSet.erase(mSelf.lock());
	}

	LLInstanceTracker(const LLInstanceTracker& other)
	:	LLInstanceTracker()
	{
	}

private:
	// Storing a weak_ptr to self is a bit like deriving from
	// std::enable_shared_from_this(), except more explicit.
	weak_t mSelf;
};

#endif
