/**
 * @file llcontrol.h
 * @brief A mechanism for storing "control state" for a program
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLCONTROL_H
#define LL_LLCONTROL_H

#include <vector>

#include "boost/bind.hpp"
#include "boost/signals2.hpp"

#include "llpointer.h"
#include "llpreprocessor.h"
#include "llstring.h"
#include "llrect.h"
#include "llrefcount.h"
#include "llinstancetracker.h"

class LLVector3;
class LLVector3d;
class LLColor4;
class LLColor3;
class LLColor4U;

constexpr bool NO_PERSIST = false;
constexpr bool PERSIST_ALWAYS = true;

typedef enum e_control_type
{
	TYPE_U32 = 0,
	TYPE_S32,
	TYPE_F32,
	TYPE_BOOLEAN,
	TYPE_STRING,
	TYPE_VEC3,
	TYPE_VEC3D,
	TYPE_RECT,
	TYPE_COL4,
	TYPE_COL3,
	TYPE_COL4U,
	TYPE_LLSD,
	TYPE_COUNT
} eControlType;

// Useful combiner for boost signals that return a bool (e.g. validation)
// returns false if any of the callbacks return false.
struct boost_boolean_combiner
{
	typedef bool result_type;
	template<typename InputIterator>
	bool operator()(InputIterator first, InputIterator last) const
	{
		bool res = true;
		while (first != last)
		{
			res &= *first++;
		}
		return res;
	}
};

class LLControlVariable : public LLRefCount
{
	friend class LLControlGroup;

public:
	typedef boost::signals2::signal<bool(LLControlVariable* controlp,
										 const LLSD&),
									boost_boolean_combiner> validate_signal_t;
	typedef boost::signals2::signal<void(LLControlVariable* controlp,
										 const LLSD&)> commit_signal_t;

public:
	LLControlVariable(const char* name, eControlType type,
					  LLSD initial, const std::string& comment,
					  bool persist = true, bool hide_from_user = false);

	LL_INLINE const std::string& getName() const		{ return mName; }
	LL_INLINE const std::string& getComment() const		{ return mComment; }

	LL_INLINE eControlType type()						{ return mType; }
	LL_INLINE bool isType(eControlType tp)				{ return tp == mType; }

	void resetToDefault(bool fire_signal = false);

	LL_INLINE commit_signal_t* getSignal()				{ return &mCommitSignal; }
	LL_INLINE validate_signal_t* getValidateSignal()	{ return &mValidateSignal; }

	LL_INLINE bool isDefault()							{ return mValues.size() == 1; }
	bool isSaveValueDefault();
	LL_INLINE bool isPersisted()						{ return mPersist; }
	LL_INLINE bool isHiddenFromUser()					{ return mHideFromUser; }
	LL_INLINE const LLSD& getValue() const				{ return mValues.back(); }
	LL_INLINE const LLSD& getDefault() const			{ return mValues.front(); }
	LLSD getSaveValue(bool user_value = true) const;

	void setValue(const LLSD& value, bool saved_value = true);
	void setDefaultValue(const LLSD& value);
	void setPersist(bool state);
	void setHiddenFromUser(bool hide);
	void setComment(const std::string& comment);

	LL_INLINE void firePropertyChanged()				{ mCommitSignal(this, mValues.back()); }

private:
	LLSD getComparableValue(const LLSD& value);
	bool llsd_compare(const LLSD& a, const LLSD & b);

private:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variables, counting the 32 bits counter from LLRefCount. HB

	eControlType		mType;
	std::string			mName;
	std::string			mComment;
	commit_signal_t		mCommitSignal;
	validate_signal_t	mValidateSignal;
	std::vector<LLSD>	mValues;
	bool				mPersist;
	bool				mHideFromUser;
};

typedef LLPointer<LLControlVariable> LLControlVariablePtr;

// Helper functions for converting between static types and LLControl values
template <class T>
eControlType get_control_type()
{
	llwarns << "Usupported control type: " << typeid(T).name() << "."
			<< llendl;
	return TYPE_COUNT;
}

template <class T>
LLSD convert_to_llsd(const T& in)
{
	// Default implementation
	return LLSD(in);
}

template <class T>
T convert_from_llsd(const LLSD& sd, eControlType type,
					const char* control_name)
{
	// Needs specialization
	return T(sd);
}

class LLControlGroup : public LLInstanceTracker<LLControlGroup, std::string>
{
protected:
	LOG_CLASS(LLControlGroup);

	eControlType typeStringToEnum(const std::string& typestr);
	std::string typeEnumToString(eControlType typeenum);

public:
	LLControlGroup(const std::string& name);
	~LLControlGroup();
	void cleanup();

	LLControlVariablePtr getControl(const char* name);

	struct ApplyFunctor
	{
		virtual ~ApplyFunctor() {}
		virtual void apply(const std::string& name,
						   LLControlVariable* controlp) = 0;
	};

	void applyToAll(ApplyFunctor* func);

	LLControlVariable* declareControl(const char* name, eControlType type,
									  const LLSD initial_val,
									  const std::string& comment, bool persist,
									  bool hide_from_settings_editor = false);

	LLControlVariable* declareBool(const char* name, bool initial_val,
								   const std::string& comment,
								   bool persist = true);

	LLControlVariable* declareString(const char* name,
									 const std::string& initial_val,
									 const std::string& comment,
									 bool persist = true);

	LLControlVariable* declareColor4(const char* name,
									 const LLColor4& initial_val,
									 const std::string& comment,
									 bool persist = true);

	LLControlVariable* declareColor4U(const char* name,
									  const LLColor4U& initial_val,
									  const std::string& comment,
									  bool persist = true);

	LLControlVariable* declareLLSD(const char* name,
								   const LLSD& initial_val,
								   const std::string& comment,
								   bool persist = true);
#if 0	// Not used
	LLControlVariable* declareU32(const char* name, U32 initial_val,
								  const std::string& comment,
								  bool persist = true);

	LLControlVariable* declareS32(const char* name,
								  S32 initial_val,
								  const std::string& comment,
								  bool persist = true);

	LLControlVariable* declareF32(const char* name, F32 initial_val,
								  const std::string& comment,
								  bool persist = true);

	LLControlVariable* declareVec3(const char* name,
								   const LLVector3& initial_val,
								   const std::string& comment,
								   bool persist = true);

	LLControlVariable* declareVec3d(const char* name,
									const LLVector3d& initial_val,
									const std::string& comment,
									bool persist = true);

	LLControlVariable* declareRect(const char* name,
								   const LLRect& initial_val,
								   const std::string& comment,
								   bool persist = true);

	LLControlVariable* declareColor3(const char* name,
									 const LLColor3& initial_val,
									 const std::string& comment,
									 bool persist = true);
#endif

	std::string getString(const char* name);
	std::string getText(const char* name);
	bool getBool(const char* name);
	S32 getS32(const char* name);
	F32 getF32(const char* name);
	U32 getU32(const char* name);

	LLWString getWString(const char* name);
	LLVector3 getVector3(const char* name);
	LLVector3d getVector3d(const char* name);
	LLRect getRect(const char* name);
	LLSD getLLSD(const char* name);

	LLColor4 getColor(const char* name);
	LLColor4 getColor4(const char* name);
	LLColor4U getColor4U(const char* name);
	LLColor3 getColor3(const char* name);

	// Generic getter
	template<typename T> T get(const char* name)
	{
		LL_DEBUGS("GetControlCalls") << "Requested control: " << name
									 << LL_ENDL;
		LLControlVariable* ctrlp = getControl(name);
		if (!ctrlp)
		{
			llwarns << "Control " << name << " not found." << llendl;
			return T();
		}
		return convert_from_llsd<T>(ctrlp->getValue(), ctrlp->type(), name);
	}

	void setBool(const char* name, bool val);
	void setS32(const char* name, S32 val);
	void setF32(const char* name, F32 val);
	void setU32(const char* name, U32 val);
	void setString(const char*  name, const std::string& val);
	void setVector3(const char* name, const LLVector3& val);
	void setVector3d(const char* name, const LLVector3d& val);
	void setRect(const char* name, const LLRect& val);
	void setColor4(const char* name, const LLColor4& val);
	void setLLSD(const char* name, const LLSD& val);

	// Type agnostic setter that takes LLSD
	void setUntypedValue(const char* name, const LLSD& val);

	// Generic setter
	template<typename T> void set(const char* name, const T& val)
	{
		LLControlVariable* ctrlp = getControl(name);
		if (!ctrlp || !ctrlp->isType(get_control_type<T>()))
		{
			llwarns << "Invalid control " << name << llendl;
			return;
		}
		ctrlp->setValue(convert_to_llsd(val));
	}

	bool controlExists(const char* name);

	// Returns number of controls loaded, 0 if failed
	// If require_declaration is false, will auto-declare controls it finds
	// as the given type.
	U32	loadFromFileLegacy(const std::string& filename,
						   bool require_declaration = true,
						   eControlType declare_as = TYPE_STRING);
 	U32 saveToFile(const std::string& filename, bool nondefault_only = true,
				   bool save_default = false);
 	U32	loadFromFile(const std::string& filename, bool default_values = false,
					 bool save_values = true);
	void resetToDefaults();

	// Ignorable Warnings

	// Add a config variable to be reset on resetWarnings()
	void addWarning(const std::string& name);
	bool getWarning(const std::string& name);
	void setWarning(const std::string& name, bool val);

	// Resets all ignorables
	void resetWarnings();

protected:
	typedef std::map<std::string, LLControlVariablePtr> ctrl_name_table_t;
	ctrl_name_table_t		mNameTable;

	std::set<std::string>	mWarnings;

	std::string				mTypeString[TYPE_COUNT];
};

// Publish/Subscribe object to interact with LLControlGroups.

static const std::string sCachedControlComment = "Cached control";

// Use an LLCachedControl instance to connect to a LLControlVariable without
// having to manually create and bind a listener to a local object.
template <class T>
class LLControlCache : public LLRefCount,
					   public LLInstanceTracker<LLControlCache<T>, std::string>
{
public:
	LLControlCache(LLControlGroup& group, const char* name)
	:	LLInstanceTracker<LLControlCache<T>, std::string >(name)
	{
		bindToControl(group, name);
	}

	LL_INLINE const T& getValue() const					{ return mCachedValue; }

private:
	LL_NO_INLINE void bindToControl(LLControlGroup& group, const char* name)
	{
		if (!group.controlExists(name))
		{
			llerrs << "Control named " << name << " not found." << llendl;
		}
		LLControlVariablePtr ctrlp = group.getControl(name);
		mType = ctrlp->type();
		mCachedValue = convert_from_llsd<T>(ctrlp->getValue(), mType, name);

		// Add a listener to the controls signal...
		mConnection =
			ctrlp->getSignal()->connect(boost::bind(&LLControlCache<T>::handleValueChange,
													this, _2),
										boost::signals2::at_front);
	}

	bool handleValueChange(const LLSD& newvalue)
	{
		mCachedValue = convert_from_llsd<T>(newvalue, mType, "");
		return true;
	}

private:
    T									mCachedValue;
	eControlType						mType;
    boost::signals2::scoped_connection	mConnection;
};

template <typename T>
class LLCachedControl
{
public:
	LLCachedControl(LLControlGroup& group, const char* name)
	{
		mCachedControlPtr = LLControlCache<T>::getNamedInstance(name).get();
		if (mCachedControlPtr.isNull())
		{
			mCachedControlPtr = new LLControlCache<T>(group, name);
		}
	}

	LL_INLINE operator const T&() const
	{
		return mCachedControlPtr->getValue();
	}

	LL_INLINE operator boost::function<const T&()>() const
	{
		return boost::function<const T&()>(*this);
	}

	LL_INLINE const T& operator()()
	{
		return mCachedControlPtr->getValue();
	}

private:
	LLPointer<LLControlCache<T> > mCachedControlPtr;
};

template<>eControlType get_control_type<U32>();
template<>eControlType get_control_type<S32>();
template<>eControlType get_control_type<F32>();
template<>eControlType get_control_type<bool>();
template<>eControlType get_control_type<std::string>();
template<>eControlType get_control_type<LLVector3>();
template<>eControlType get_control_type<LLVector3d>();
template<>eControlType get_control_type<LLRect>();
template<>eControlType get_control_type<LLColor4>();
template<>eControlType get_control_type<LLColor4U>();
template<>eControlType get_control_type<LLColor3>();
template<>eControlType get_control_type<LLSD>();

template<>LLSD convert_to_llsd<U32>(const U32& in);
template<>LLSD convert_to_llsd<LLVector3>(const LLVector3& in);
template<>LLSD convert_to_llsd<LLVector3d>(const LLVector3d& in);
template<>LLSD convert_to_llsd<LLRect>(const LLRect& in);
template<>LLSD convert_to_llsd<LLColor4>(const LLColor4& in);
template<>LLSD convert_to_llsd<LLColor4U>(const LLColor4U& in);
template<>LLSD convert_to_llsd<LLColor3>(const LLColor3& in);

template<>std::string convert_from_llsd<std::string>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLWString convert_from_llsd<LLWString>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLVector3 convert_from_llsd<LLVector3>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLVector3d convert_from_llsd<LLVector3d>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLRect convert_from_llsd<LLRect>(const LLSD& sd, eControlType type, const char* control_name);
template<>bool convert_from_llsd<bool>(const LLSD& sd, eControlType type, const char* control_name);
template<>S32 convert_from_llsd<S32>(const LLSD& sd, eControlType type, const char* control_name);
template<>F32 convert_from_llsd<F32>(const LLSD& sd, eControlType type, const char* control_name);
template<>U32 convert_from_llsd<U32>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLColor3 convert_from_llsd<LLColor3>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLColor4 convert_from_llsd<LLColor4>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLColor4U convert_from_llsd<LLColor4U>(const LLSD& sd, eControlType type, const char* control_name);
template<>LLSD convert_from_llsd<LLSD>(const LLSD& sd, eControlType type, const char* control_name);

#endif
