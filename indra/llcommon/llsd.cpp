/**
 * @file llsd.cpp
 * @brief LLSD flexible data system
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include <limits>

#include "llsd.h"

#include "llatomic.h"
#include "llcommonmath.h"		// For llisnan()
#include "llsdserialize.h"

#if LL_DEBUG
# define NAME_UNNAMED_NAMESPACE 1
#endif

// Defend against a caller forcibly passing a negative number into an unsigned
// size_t index param
LL_INLINE static bool was_negative(size_t i)
{
	return i > std::numeric_limits<int>::max();
}
#define NEGATIVE_EXIT(i) if (was_negative(i)) return
#define NEGATIVE_RETURN(i, result) if (was_negative(i)) return (result)

#ifdef NAME_UNNAMED_NAMESPACE
namespace LLSDUnnamedNamespace
#else
namespace
#endif
{
	class ImplMap;
	class ImplArray;
}

#ifdef NAME_UNNAMED_NAMESPACE
using namespace LLSDUnnamedNamespace;
#endif

// This class is the abstract base class of the implementation of LLSD. It
// provides the reference counting implementation, and the default
// implementation of most methods for most data types. It also serves as a
// working implementation of the Undefined type.
class LLSD::Impl
{
protected:
	Impl();

	enum StaticAllocationMarker { STATIC_USAGE_COUNT = U32_MAX };

	// This constructor is used for static objects.
	Impl(StaticAllocationMarker);

	virtual ~Impl() = default;

	LL_INLINE bool shared() const
	{
		// Must be placed in a temp variable so that we do not risk seeing
		// the value changed by another thread during the && below. HB
		U32 use_count = mUseCount.get();
		return use_count > 1 && use_count != STATIC_USAGE_COUNT;
	}

public:
	// Safely sets var to refer to the new impl (possibly shared)
	static void reset(Impl*& var, Impl* impl);

	// Since a NULL Impl* is used for undefined, this ensures there is always
	// an object you call virtual member functions on
	static Impl& safe(Impl*);

	// Do make sure var is a modifiable, non-shared map or array
	virtual ImplMap& makeMap(Impl*& var) const;
	virtual ImplArray& makeArray(Impl*& var) const;

	LL_INLINE virtual LLSD::Type type() const				{ return LLSD::TypeUndefined; }

	static void assignUndefined(LLSD::Impl*& var);
	static void assign(LLSD::Impl*& var, const LLSD::Impl* other);

	// If the receiver is the right type and unshared, these are simple data
	// assignments, otherwise the default implementation handles constructing
	// the proper Impl subclass
	virtual void assign(Impl*& var, LLSD::Boolean) const;
	virtual void assign(Impl*& var, LLSD::Integer) const;
	virtual void assign(Impl*& var, LLSD::Real) const;
	virtual void assign(Impl*& var, const LLSD::String&) const;
	virtual void assign(Impl*& var, const LLSD::UUID&) const;
	virtual void assign(Impl*& var, const LLSD::Date&) const;
	virtual void assign(Impl*& var, const LLSD::URI&) const;
	virtual void assign(Impl*& var, const LLSD::Binary&) const;

	LL_INLINE virtual Boolean asBoolean() const				{ return false; }
	LL_INLINE virtual Integer asInteger() const				{ return 0; }
	LL_INLINE virtual Real asReal() const					{ return 0.0; }
	LL_INLINE virtual String asString() const				{ return std::string(); }
	LL_INLINE virtual UUID asUUID() const					{ return LLUUID(); }
	LL_INLINE virtual Date asDate() const					{ return LLDate(); }
	LL_INLINE virtual URI asURI() const						{ return LLURI(); }

	LL_INLINE virtual const Binary& asBinary() const
	{
		static const LLSD::Binary empty;
		return empty;
	}

	LL_INLINE virtual const String& asStringRef() const
	{
		static const std::string empty;
		return empty;
	}

	LL_INLINE virtual bool has(const String&) const			{ return false; }
	LL_INLINE virtual LLSD get(const String&) const			{ return LLSD(); }
	LL_INLINE virtual bool has(const char*) const			{ return false; }
	LL_INLINE virtual LLSD get(const char*) const			{ return LLSD(); }
	LL_INLINE virtual LLSD getKeys() const					{ return LLSD::emptyArray(); }
	LL_INLINE virtual void erase(const String&)				{}
	LL_INLINE virtual const LLSD& ref(const String&) const	{ return undef(); }

	LL_INLINE virtual size_t size() const					{ return 0; }
	LL_INLINE virtual LLSD get(size_t) const				{ return LLSD(); }
	LL_INLINE virtual void erase(size_t)					{}
	LL_INLINE virtual const LLSD& ref(size_t) const			{ return undef(); }

	LL_INLINE virtual LLSD::map_const_iterator beginMap() const
	{
		return sEmptyMap.end();
	}

	LL_INLINE virtual LLSD::map_const_iterator endMap() const
	{
		return sEmptyMap.end();
	}

	LL_INLINE virtual LLSD::map_const_iterator find(const String&) const
	{
		return sEmptyMap.end();
	}

	LL_INLINE virtual LLSD::map_const_iterator find(const char*) const
	{
		return sEmptyMap.end();
	}

	LL_INLINE virtual LLSD::array_const_iterator beginArray() const
	{
		return endArray();
	}

	LL_INLINE virtual LLSD::array_const_iterator endArray() const
	{
		static const std::vector<LLSD> empty;
		return empty.end();
	}

public:
	static const LLSD& undef();

private:
	LLAtomicU32						mUseCount;

	static std::map<String, LLSD>	sEmptyMap;
};

//static
std::map<LLSD::String, LLSD> LLSD::Impl::sEmptyMap;

#ifdef NAME_UNNAMED_NAMESPACE
namespace LLSDUnnamedNamespace
#else
namespace
#endif
{
	// This class handles most of the work for a subclass of Impl for a given
	// simple data type. Subclasses of this provide the conversion functions
	// and a constructor.
	template<LLSD::Type T, class Data, class DataRef = Data>
	class ImplBase : public LLSD::Impl
	{
	public:
		typedef ImplBase Base;

		LL_INLINE ImplBase(DataRef value)
		:	mValue(value)
		{
		}

		LL_INLINE virtual LLSD::Type type() const			{ return T; }

		using LLSD::Impl::assign; // Unhiding base class virtuals...
		LL_INLINE virtual void assign(LLSD::Impl*& var, DataRef value) const
		{
			if (shared())
			{
				Impl::assign(var, value);
			}
			else
			{
				const_cast<ImplBase*>(this)->mValue = value;
			}
		}

	protected:
		Data mValue;
	};

	class ImplBoolean final : public ImplBase<LLSD::TypeBoolean, LLSD::Boolean>
	{
	public:
		LL_INLINE ImplBoolean(LLSD::Boolean v)
		:	Base(v)
		{
		}

		LL_INLINE LLSD::Boolean asBoolean() const override	{ return mValue; }
		LL_INLINE LLSD::Integer asInteger() const override	{ return mValue ? 1 : 0; }
		LL_INLINE LLSD::Real asReal() const override		{ return mValue ? 1 : 0; }
		LLSD::String asString() const override;
	};

	// *NOTE: The reason that false is not converted to "false" is because that
	// would break roundtripping, e.g. LLSD(false).asString().asBoolean().
	// There are many reasons for wanting LLSD("false").asBoolean() == true,
	// such as "everything else seems to work that way".
	LLSD::String ImplBoolean::asString() const				{ return mValue ? "true" : ""; }

	class ImplInteger final : public ImplBase<LLSD::TypeInteger, LLSD::Integer>
	{
	public:
		LL_INLINE ImplInteger(LLSD::Integer v)
		:	Base(v)
		{
		}

		LL_INLINE LLSD::Boolean asBoolean() const override	{ return mValue != 0; }
		LL_INLINE LLSD::Integer asInteger() const override	{ return mValue; }
		LL_INLINE LLSD::Real asReal() const override		{ return mValue; }
		LLSD::String asString() const override;
	};

	LLSD::String ImplInteger::asString() const				{ return llformat("%d", mValue); }

	class ImplReal final : public ImplBase<LLSD::TypeReal, LLSD::Real>
	{
	public:
		LL_INLINE ImplReal(LLSD::Real v)
		:	Base(v)
		{
		}

		LL_INLINE LLSD::Boolean asBoolean() const override
		{
			return !llisnan(mValue) && mValue != 0.0;
		}

		LL_INLINE LLSD::Integer asInteger() const override
		{
			return llisnan(mValue) ? 0 : (LLSD::Integer)mValue;
		}

		LL_INLINE LLSD::Real asReal() const override		{ return mValue; }
		LL_INLINE LLSD::String asString() const override	{ return llformat("%lg", mValue); }
	};

	class ImplString final : public ImplBase<LLSD::TypeString, LLSD::String,
											 const LLSD::String&>
	{
	public:
		LL_INLINE ImplString(const LLSD::String& v)
		:	Base(v)
	 	{
		}

		LL_INLINE LLSD::Boolean	asBoolean() const override	{ return !mValue.empty(); }

		LL_INLINE LLSD::Integer asInteger() const override
		{
			// This must treat "1.23" not as an error, but as a number, which
			// is then truncated down to an integer.  Hence, this code does not
			// call std::istringstream::operator>>(int&), which would not
			// consume the ".23" portion.
			return (int)asReal();
		}

		LL_INLINE LLSD::Real asReal() const override
		{
			F64 v = 0.0;
			std::istringstream i_stream(mValue);
			i_stream >> v;

			// We would probably like to ignore all trailing whitespace as
			// well, but for now, simply eat the next character, and make sure
			// we reached the end of the string.
			return i_stream.get() == EOF ? v : 0.0;
		}

		LL_INLINE LLSD::String	asString() const override	{ return mValue; }
		LL_INLINE LLSD::UUID asUUID() const override		{ return LLUUID(mValue); }
		LL_INLINE LLSD::Date asDate() const override		{ return LLDate(mValue); }
		LL_INLINE LLSD::URI asURI() const override			{ return LLURI(mValue); }
		LL_INLINE size_t size() const override				{ return mValue.size(); }

		LL_INLINE const LLSD::String& asStringRef() const override
		{
			return mValue;
		}
	};

	class ImplUUID final : public ImplBase<LLSD::TypeUUID, LLSD::UUID,
										   const LLSD::UUID&>
	{
	public:
		LL_INLINE ImplUUID(const LLSD::UUID& v)
		:	Base(v)
		{
		}

		LL_INLINE LLSD::String asString() const override	{ return mValue.asString(); }
		LL_INLINE LLSD::UUID asUUID() const override		{ return mValue; }
	};

	class ImplDate final : public ImplBase<LLSD::TypeDate, LLSD::Date,
										   const LLSD::Date&>
	{
	public:
		LL_INLINE ImplDate(const LLSD::Date& v)
		:	ImplBase<LLSD::TypeDate, LLSD::Date, const LLSD::Date&>(v)
		{
		}

		LL_INLINE LLSD::Integer asInteger() const override
		{
			return (LLSD::Integer)(mValue.secondsSinceEpoch());
		}

		LL_INLINE LLSD::Real asReal() const override
		{
			return mValue.secondsSinceEpoch();
		}

		LL_INLINE LLSD::String asString() const override	{ return mValue.asString(); }
		LL_INLINE LLSD::Date asDate() const override		{ return mValue; }
	};

	class ImplURI final : public ImplBase<LLSD::TypeURI, LLSD::URI,
										  const LLSD::URI&>
	{
	public:
		ImplURI(const LLSD::URI& v)
		:	Base(v)
		{
		}

		LL_INLINE LLSD::String asString() const override	{ return mValue.asString(); }
		LL_INLINE LLSD::URI asURI() const override			{ return mValue; }
	};

	class ImplBinary final : public ImplBase<LLSD::TypeBinary, LLSD::Binary,
											 const LLSD::Binary&>
	{
	public:
		ImplBinary(const LLSD::Binary& v)
		:	Base(v)
		{
		}

		LL_INLINE const LLSD::Binary& asBinary() const override
		{
			return mValue;
		}
	};

	class ImplMap final : public LLSD::Impl
	{
	private:
		typedef std::map<LLSD::String, LLSD> DataMap;

	protected:
		LL_INLINE ImplMap(const DataMap& data)
		:	mData(data)
		{
		}

	public:
		ImplMap() = default;

		ImplMap& makeMap(LLSD::Impl*&) const override;

		LL_INLINE LLSD::Type type() const override			{ return LLSD::TypeMap; }

		LL_INLINE LLSD::Boolean asBoolean() const override	{ return !mData.empty(); }

		bool has(const LLSD::String&) const override;
		bool has(const char*) const override;

		using LLSD::Impl::get; // Unhiding get(size_t)
		using LLSD::Impl::erase; // Unhiding erase(size_t)
		using LLSD::Impl::ref; // Unhiding ref(size_t)
		LLSD get(const LLSD::String&) const override;
		LLSD get(const char*) const override;
		LLSD getKeys() const override;
		void insert(const LLSD::String& k, const LLSD& v);
		void erase(const LLSD::String&) override;
		const LLSD& ref(const LLSD::String&) const override;
		LLSD& ref(const LLSD::String&);

		LL_INLINE size_t size() const override				{ return mData.size(); }

		LL_INLINE LLSD::map_iterator beginMap() 			{ return mData.begin(); }
		LL_INLINE LLSD::map_iterator endMap() 				{ return mData.end(); }
		LL_INLINE LLSD::map_const_iterator beginMap() const override
		{
			return mData.begin();
		}

		LL_INLINE LLSD::map_const_iterator endMap() const override
		{
			return mData.end();
		}

		LL_INLINE LLSD::map_const_iterator find(const LLSD::String& k) const override
		{
			return mData.find(k);
		}

		LL_INLINE LLSD::map_const_iterator find(const char* k) const override
		{
			return mData.find(k);
		}

	private:
		DataMap mData;
	};

	ImplMap& ImplMap::makeMap(LLSD::Impl*& var) const
	{
		if (shared())
		{
			ImplMap* i = new ImplMap(mData);
			Impl::assign(var, i);
			return *i;
		}
		// Only we have determined that 'this' is already an ImplMap that is
		// not shared do we cast away its const-ness
		return const_cast<ImplMap&>(*this);
	}

	LL_INLINE bool ImplMap::has(const LLSD::String& k) const
	{
		DataMap::const_iterator i = mData.find(k);
		return i != mData.end();
	}

	LL_INLINE LLSD ImplMap::get(const LLSD::String& k) const
	{
		DataMap::const_iterator i = mData.find(k);
		return i != mData.end() ? i->second : LLSD();
	}

	LL_INLINE bool ImplMap::has(const char* k) const
	{
		DataMap::const_iterator i = mData.find(k);
		return i != mData.end();
	}

	LL_INLINE LLSD ImplMap::get(const char* k) const
	{
		DataMap::const_iterator i = mData.find(k);
		return i != mData.end() ? i->second : LLSD();
	}

	LLSD ImplMap::getKeys() const
	{
		LLSD keys = LLSD::emptyArray();
		for (DataMap::const_iterator iter = mData.begin(), end = mData.end();
			 iter != end; ++iter)
		{
			keys.append(iter->first);
		}
		return keys;
	}

	LL_INLINE void ImplMap::insert(const LLSD::String& k, const LLSD& v)
	{
		mData.insert(DataMap::value_type(k, v));
	}

	LL_INLINE void ImplMap::erase(const LLSD::String& k)
	{
		mData.erase(k);
	}

	LL_INLINE LLSD& ImplMap::ref(const LLSD::String& k)
	{
		return mData[k];
	}

	LL_INLINE const LLSD& ImplMap::ref(const LLSD::String& k) const
	{
		DataMap::const_iterator i = mData.find(k);
		return i != mData.end() ?  i->second : undef();
	}

	class ImplArray final : public LLSD::Impl
	{
	private:
		typedef std::vector<LLSD> DataVector;

		DataVector mData;

	protected:
		ImplArray(const DataVector& data)
		:	mData(data)	
		{
		}

	public:
		ImplArray() = default;

		ImplArray& makeArray(Impl*&) const override;

		LL_INLINE LLSD::Type type() const override			{ return LLSD::TypeArray; }

		LL_INLINE LLSD::Boolean asBoolean() const override	{ return !mData.empty(); }

		using LLSD::Impl::get;		// Unhiding get(LLSD::String)
		using LLSD::Impl::erase;	// Unhiding erase(LLSD::String)
		using LLSD::Impl::ref;		// Unhiding ref(LLSD::String)

		LL_INLINE size_t size() const override				{ return mData.size(); }

		LLSD get(size_t i) const override;
		void set(size_t i, const LLSD& v);

		void insert(size_t i, const LLSD& v);

		LL_INLINE LLSD& append(const LLSD& v)
		{
			mData.push_back(v);
			return mData.back();
		}

		void erase(size_t i) override;

		LLSD& ref(size_t);
		const LLSD& ref(size_t i) const override;

		LL_INLINE LLSD::array_iterator beginArray()
		{
			return mData.begin();
		}

		LL_INLINE LLSD::array_iterator endArray() 			{ return mData.end(); }

		LL_INLINE LLSD::reverse_array_iterator rbeginArray()
		{
			return mData.rbegin();
		}

		LL_INLINE LLSD::reverse_array_iterator rendArray() { return mData.rend(); }

		LL_INLINE LLSD::array_const_iterator beginArray() const override
		{
			return mData.begin();
		}

		LL_INLINE LLSD::array_const_iterator endArray() const override
		{
			return mData.end();
		}
	};

	ImplArray& ImplArray::makeArray(Impl*& var) const
	{
		if (shared())
		{
			ImplArray* i = new ImplArray(mData);
			Impl::assign(var, i);
			return *i;
		}
		// Only once we have determined that 'this' is already an ImplArray
		// that is not shared do we cast away its const-ness
		return const_cast<ImplArray&>(*this);
	}

	LLSD ImplArray::get(size_t i) const
	{
		NEGATIVE_RETURN(i, LLSD());
		return i < mData.size() ? mData[i] : LLSD();
	}

	void ImplArray::set(size_t i, const LLSD& v)
	{
		NEGATIVE_EXIT(i);
		if (i >= mData.size())
		{
			mData.resize(i + 1);
		}
		mData[i] = v;
	}

	void ImplArray::insert(size_t i, const LLSD& v)
	{
		NEGATIVE_EXIT(i);
		if (i >= mData.size())
		{
			mData.resize(i + 1);
		}
		mData.insert(mData.begin() + i, v);
	}

	void ImplArray::erase(size_t i)
	{
		NEGATIVE_EXIT(i);
		if (i < mData.size())
		{
			mData.erase(mData.begin() + i);
		}
	}

	LLSD& ImplArray::ref(size_t i)
	{
		DataVector::size_type index = was_negative(i) ? 0 : i;
		if (index >= mData.size())
		{
			mData.resize(index + 1);
		}
		return mData[index];
	}

	const LLSD& ImplArray::ref(size_t i) const
	{
		NEGATIVE_RETURN(i, undef());
		if (i >= mData.size())
		{
			return undef();
		}
		return mData[i];
	}
}

LLSD::Impl::Impl()
:	mUseCount(0)
{
}

LLSD::Impl::Impl(StaticAllocationMarker)
:	mUseCount(0)
{
}

//static
void LLSD::Impl::reset(Impl*& var, Impl* impl)
{
	if (impl == var)
	{
		return;
	}
	if (impl && impl->mUseCount != STATIC_USAGE_COUNT)
	{
		++impl->mUseCount;
	}
	if (var && var->mUseCount != STATIC_USAGE_COUNT && --var->mUseCount == 0)
	{
		delete var;
	}
	var = impl;
}

//static
LLSD::Impl& LLSD::Impl::safe(Impl* impl)
{
	static Impl theUndefined(STATIC_USAGE_COUNT);
	return impl ? *impl : theUndefined;
}

//virtual
ImplMap& LLSD::Impl::makeMap(Impl*& var) const
{
	ImplMap* im = new ImplMap;
	reset(var, im);
	return *im;
}

//virtual
ImplArray& LLSD::Impl::makeArray(Impl*& var) const
{
	ImplArray* ia = new ImplArray;
	reset(var, ia);
	return *ia;
}

//virtual
void LLSD::Impl::assign(Impl*& var, const Impl* other)
{
	reset(var, const_cast<Impl*>(other));
}

//virtual
void LLSD::Impl::assignUndefined(Impl*& var)
{
	reset(var, 0);
}

//virtual
void LLSD::Impl::assign(Impl*& var, LLSD::Boolean v) const
{
	reset(var, new ImplBoolean(v));
}

//virtual
void LLSD::Impl::assign(Impl*& var, LLSD::Integer v) const
{
	reset(var, new ImplInteger(v));
}

//virtual
void LLSD::Impl::assign(Impl*& var, LLSD::Real v) const
{
	reset(var, new ImplReal(v));
}

//virtual
void LLSD::Impl::assign(Impl*& var, const LLSD::String& v) const
{
	reset(var, new ImplString(v));
}

//virtual
void LLSD::Impl::assign(Impl*& var, const LLSD::UUID& v) const
{
	reset(var, new ImplUUID(v));
}

//virtual
void LLSD::Impl::assign(Impl*& var, const LLSD::Date& v) const
{
	reset(var, new ImplDate(v));
}

//virtual
void LLSD::Impl::assign(Impl*& var, const LLSD::URI& v) const
{
	reset(var, new ImplURI(v));
}

//virtual
void LLSD::Impl::assign(Impl*& var, const LLSD::Binary& v) const
{
	reset(var, new ImplBinary(v));
}

//static
const LLSD& LLSD::Impl::undef()
{
	static const LLSD immutableUndefined;
	return immutableUndefined;
}

#ifdef NAME_UNNAMED_NAMESPACE
namespace LLSDUnnamedNamespace
#else
namespace
#endif
{
	LL_INLINE const LLSD::Impl& safe(LLSD::Impl* impl)
	{
		return LLSD::Impl::safe(impl);
	}

	LL_INLINE ImplMap& makeMap(LLSD::Impl*& var)
	{
		return safe(var).makeMap(var);
	}

	LL_INLINE ImplArray& makeArray(LLSD::Impl*& var)
	{
		return safe(var).makeArray(var);
	}
}

LLSD::LLSD()
:	impl(NULL)
{
}

LLSD::~LLSD()
{
	Impl::reset(impl, NULL);
}

LLSD::LLSD(const LLSD& other)
:	impl(NULL)
{
	assign(other);
}

void LLSD::assign(const LLSD& other)
{
	Impl::assign(impl, other.impl);
}

void LLSD::clear()
{
	Impl::assignUndefined(impl);
}

LLSD::Type LLSD::type() const
{
	return safe(impl).type();
}

// Scalar Constructors

LLSD::LLSD(Boolean v)
:	impl(NULL)
{
	assign(v);
}

LLSD::LLSD(Integer v)
:	impl(NULL)
{
	assign(v);
}

LLSD::LLSD(Real v)
:	impl(NULL)
{
	assign(v);
}

LLSD::LLSD(const UUID& v)
:	impl(NULL)
{
	assign(v);
}

LLSD::LLSD(const String& v)
:	impl(NULL)
{
	assign(v);
}

LLSD::LLSD(const Date& v)
:	impl(NULL)
{
	assign(v);
}

LLSD::LLSD(const URI& v)
:	impl(NULL)
{
	assign(v);
}

LLSD::LLSD(const Binary& v)
:	impl(NULL)
{
	assign(v);
}


// Scalar assignments

void LLSD::assign(Boolean v)
{
	safe(impl).assign(impl, v);
}

void LLSD::assign(Integer v)
{
	safe(impl).assign(impl, v);
}

void LLSD::assign(Real v)
{
	safe(impl).assign(impl, v);
}

void LLSD::assign(const String& v)
{
	safe(impl).assign(impl, v);
}

void LLSD::assign(const UUID& v)
{
	safe(impl).assign(impl, v);
}

void LLSD::assign(const Date& v)
{
	safe(impl).assign(impl, v);
}

void LLSD::assign(const URI& v)
{
	safe(impl).assign(impl, v);
}

void LLSD::assign(const Binary& v)
{
	safe(impl).assign(impl, v);
}

// Scalar Accessors

LLSD::Boolean LLSD::asBoolean() const
{
	return safe(impl).asBoolean();
}

LLSD::Integer LLSD::asInteger() const
{
	return safe(impl).asInteger();
}

LLSD::Real LLSD::asReal() const
{
	return safe(impl).asReal();
}

LLSD::String LLSD::asString() const
{
	return safe(impl).asString();
}

LLSD::UUID LLSD::asUUID() const
{
	return safe(impl).asUUID();
}

LLSD::Date LLSD::asDate() const
{
	return safe(impl).asDate();
}

LLSD::URI LLSD::asURI() const
{
	return safe(impl).asURI();
}

const LLSD::Binary& LLSD::asBinary() const
{
	return safe(impl).asBinary();
}

const LLSD::String& LLSD::asStringRef() const
{
	return safe(impl).asStringRef();
}

// const char* helpers

LLSD::LLSD(const char* v)
:	impl(NULL)
{
	assign(v);
}

void LLSD::assign(const char* v)
{
	if (v)
	{
		assign(std::string(v));
	}
	else
	{
		assign(std::string());
	}
}

LLSD LLSD::emptyMap()
{
	LLSD v;
	makeMap(v.impl);
	return v;
}

bool LLSD::has(const String& k) const
{
	return safe(impl).has(k);
}

LLSD LLSD::get(const String& k) const
{
	return safe(impl).get(k);
}

bool LLSD::has(const char* k) const
{
	return safe(impl).has(k);
}

LLSD LLSD::get(const char* k) const
{
	return safe(impl).get(k);
}

LLSD LLSD::getKeys() const
{
	return safe(impl).getKeys();
}

void LLSD::insert(const String& k, const LLSD& v)
{
	makeMap(impl).insert(k, v);
}

LLSD& LLSD::with(const String& k, const LLSD& v)
{
	makeMap(impl).insert(k, v);
	return *this;
}

void LLSD::erase(const String& k)
{
	makeMap(impl).erase(k);
}

LLSD& LLSD::operator[](const String& k)
{
	return makeMap(impl).ref(k);
}

const LLSD& LLSD::operator[](const String& k) const
{
	return safe(impl).ref(k);
}

LLSD LLSD::emptyArray()
{
	LLSD v;
	makeArray(v.impl);
	return v;
}

size_t LLSD::size() const
{
	return safe(impl).size();
}

LLSD LLSD::get(size_t i) const
{
	return safe(impl).get(i);
}

void LLSD::set(size_t i, const LLSD& v)
{
	makeArray(impl).set(i, v);
}

void LLSD::insert(size_t i, const LLSD& v)
{
	makeArray(impl).insert(i, v);
}

LLSD& LLSD::with(Integer i, const LLSD& v)
{
	makeArray(impl).insert(i, v);
	return *this;
}

LLSD& LLSD::append(const LLSD& v)
{
	return makeArray(impl).append(v);
}

void LLSD::erase(size_t i)
{
	makeArray(impl).erase(i);
}

LLSD& LLSD::operator[](size_t i)
{
	return makeArray(impl).ref(i);
}

const LLSD& LLSD::operator[](size_t i) const
{
	return safe(impl).ref(i);
}

LLSD::map_iterator LLSD::beginMap()
{
	return makeMap(impl).beginMap();
}

LLSD::map_iterator LLSD::endMap()
{
	return makeMap(impl).endMap();
}

LLSD::map_const_iterator LLSD::beginMap() const
{
	return safe(impl).beginMap();
}

LLSD::map_const_iterator LLSD::endMap() const
{
	return safe(impl).endMap();
}

LLSD::map_const_iterator LLSD::find(const String& k) const
{
	return safe(impl).find(k);
}

LLSD::map_const_iterator LLSD::find(const char* k) const
{
	return safe(impl).find(k);
}

LLSD::array_iterator LLSD::beginArray()
{
	return makeArray(impl).beginArray();
}

LLSD::array_iterator LLSD::endArray()
{
	return makeArray(impl).endArray();
}

LLSD::array_const_iterator LLSD::beginArray() const
{
	return safe(impl).beginArray();
}

LLSD::array_const_iterator LLSD::endArray() const
{
	return safe(impl).endArray();
}

LLSD::reverse_array_iterator LLSD::rbeginArray()
{
	return makeArray(impl).rbeginArray();
}

LLSD::reverse_array_iterator LLSD::rendArray()
{
	return makeArray(impl).rendArray();
}
