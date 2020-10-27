#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "Assert.h"
#include "ScanVariantTypeTraits.h"
#include "ScannerTypes.h"

#include "ScanVariantSearchContext.h"

class ScanVariant
{
public:
	typedef uint32_t ScanVariantType;
	enum _ScanVariantType : ScanVariantType
	{
		// REMEMBER : IMPORTANT
		// When we update this enum, we must also
		// update the ScanVariant::UnderlyingTypeTraits
		// array in ScanVariant.cpp

		SCAN_VARIANT_ALLTYPES_BEGIN,

		SCAN_VARIANT_STRINGTYPES_BEGIN = SCAN_VARIANT_ALLTYPES_BEGIN,
			SCAN_VARIANT_ASCII_STRING = SCAN_VARIANT_STRINGTYPES_BEGIN,
			SCAN_VARIANT_WIDE_STRING,
		SCAN_VARIANT_STRINGTYPES_END = SCAN_VARIANT_WIDE_STRING,

		SCAN_VARIANT_NUMERICTYPES_BEGIN,
			SCAN_VARIANT_NUMERICTYPES_INFERABLE_BEGIN = SCAN_VARIANT_NUMERICTYPES_BEGIN,
				SCAN_VARIANT_UINT8 = SCAN_VARIANT_NUMERICTYPES_BEGIN,
				SCAN_VARIANT_INT8,
				SCAN_VARIANT_UINT16,
				SCAN_VARIANT_INT16,
				SCAN_VARIANT_UINT32,
				SCAN_VARIANT_INT32,
				SCAN_VARIANT_UINT64,
				SCAN_VARIANT_INT64,
				SCAN_VARIANT_DOUBLE,
				SCAN_VARIANT_FLOAT,
			SCAN_VARIANT_NUMERICTYPES_INFERABLE_END = SCAN_VARIANT_FLOAT,
			SCAN_VARIANT_FILETIME64,
			SCAN_VARIANT_TICKTIME32,
		SCAN_VARIANT_NUMERICTYPES_END = SCAN_VARIANT_TICKTIME32,

		SCAN_VARIANT_ALLTYPES_END = SCAN_VARIANT_NUMERICTYPES_END,

		// These come beyond the end marker because they are special snowflakes
		SCAN_VARIANT_STRUCTURE,
		SCAN_VARIANT_NULL, // null is the last type with traits defined

		// Need to make sure we always handle these types special (check getUnderlyingType() function)
		SCAN_VARIANT_RANGE_BEGIN,
		SCAN_VARIANT_RANGE_END = (SCAN_VARIANT_RANGE_BEGIN + (SCAN_VARIANT_NUMERICTYPES_END - SCAN_VARIANT_NUMERICTYPES_BEGIN)),

		SCAN_VARIANT_PLACEHOLDER_BEGIN,
		SCAN_VARIANT_PLACEHOLDER_END = (SCAN_VARIANT_PLACEHOLDER_BEGIN + (SCAN_VARIANT_NUMERICTYPES_END - SCAN_VARIANT_NUMERICTYPES_BEGIN)),
	};


	static const ScanVariant MakeNull() { ScanVariant v; v.setSizeAndValue(); return v; } 
	static const ScanVariant MakePlaceholder(const ScanVariantType& type);

	static const ScanVariant FromRawBuffer(const void* buffer,    const size_t& bufferSize, const bool &isLittleEndian, const ScanVariant& reference);
	static const ScanVariant FromRawBuffer(const uint8_t* buffer, const size_t& bufferSize, const bool &isLittleEndian, const ScanVariant& reference);

	static const ScanVariant FromVariantRange(const ScanVariant& min, const ScanVariant& max);
	static const ScanVariant FromMemoryAddress(const MemoryAddress& valueMemoryAddress);
	static const ScanVariant FromNumberTyped(const uint64_t& value, const ScanVariantType& type);

	static const ScanVariant FromStringTyped(const std::string& input,  const ScanVariantType& type);
	static const ScanVariant FromStringTyped(const std::wstring& input, const ScanVariantType& type);

	static const ScanVariant FromTargetMemory(const std::shared_ptr<class ScannerTarget> &target, const MemoryAddress& address, const ScanVariantType& type);


#define SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(TYPENAME, TYPE, VALUENAME, TYPEIDENTIFIER) \
	static const ScanVariant From ## TYPENAME(const TYPE& VALUENAME) \
	{ \
		ScanVariant v;\
		v.VALUENAME = VALUENAME;\
		v.type = TYPEIDENTIFIER;\
		v.setSizeAndValue();\
		return v;\
	}
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, uint8_t,                         valueuint8,        SCAN_VARIANT_UINT8);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, int8_t,                          valueint8,         SCAN_VARIANT_INT8);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, uint16_t,                        valueuint16,       SCAN_VARIANT_UINT16);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, int16_t,                         valueint16,        SCAN_VARIANT_INT16);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, uint32_t,                        valueuint32,       SCAN_VARIANT_UINT32);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, int32_t,                         valueint32,        SCAN_VARIANT_INT32);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, uint64_t,                        valueuint64,       SCAN_VARIANT_UINT64);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, int64_t,                         valueint64,        SCAN_VARIANT_INT64);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, double,                          valueDouble,       SCAN_VARIANT_DOUBLE);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Number, float,                           valueFloat,        SCAN_VARIANT_FLOAT);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(Struct, std::vector<ScanVariant>,        valueStruct,       SCAN_VARIANT_STRUCTURE);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(String, std::string,                     valueAsciiString,  SCAN_VARIANT_ASCII_STRING);
	SCAN_VARIANT_EXPLICIT_CONSTRUCTOR(String, std::wstring,                    valueWideString,   SCAN_VARIANT_WIDE_STRING);


	const bool isCompatibleWith(const ScanVariant& other, const bool strict) const;

	inline const size_t getSize() const
	{
		return this->valueSize;
	}
	inline const ScanVariantType getType() const
	{
		return this->type;
	}
	inline const ScanVariantType ScanVariant::getUnderlyingType() const
	{
		if (this->isRange())
			return (SCAN_VARIANT_NUMERICTYPES_BEGIN + (this->getType() - SCAN_VARIANT_RANGE_BEGIN));
		else if (this->isPlaceholder())
			return (SCAN_VARIANT_NUMERICTYPES_BEGIN + (this->getType() - SCAN_VARIANT_PLACEHOLDER_BEGIN));
		else
			return this->getType();
	}
	inline const ScanVariantUnderlyingTypeTraits* getTypeTraits() const
	{
		return UnderlyingTypeTraits[this->getUnderlyingType()];
	}

	const std::wstring getTypeName() const;
	const std::wstring toString() const;

	const bool isComposite() const;
	const std::vector<ScanVariant>& getCompositeValues() const;

	inline const bool isStructure() const
	{
		return this->getTypeTraits()->isStructureType();
	}
	inline const bool isRange() const
	{
		return (this->type >= SCAN_VARIANT_RANGE_BEGIN && this->type <= SCAN_VARIANT_RANGE_END);
	}
	inline const bool isDynamic() const
	{
		return this->getTypeTraits()->isDynamicType();
	}
	inline const bool isPlaceholder() const
	{
		return (this->type >= SCAN_VARIANT_PLACEHOLDER_BEGIN && this->type <= SCAN_VARIANT_PLACEHOLDER_END);
	}

	const bool isNull() const;
	const bool getValue(std::string &value) const;
	const bool getValue(std::wstring &value) const;
	const bool getValue(uint8_t &value) const;
	const bool getValue(int8_t &value) const;
	const bool getValue(uint16_t &value) const;
	const bool getValue(int16_t &value) const;
	const bool getValue(uint32_t &value) const;
	const bool getValue(int32_t &value) const;
	const bool getValue(uint64_t &value) const;
	const bool getValue(int64_t &value) const;
	const bool getValue(double &value) const;
	const bool getValue(float &value) const;
	const bool getValue(std::vector<ScanVariant> &value) const;

	const bool writeToTarget(const std::shared_ptr<class ScannerTarget> &target, const MemoryAddress& address) const;

	/*
		This is safe IF and ONLY IF the caller takes some precautions:
			1. When comparing a ScanVariant to a raw memory buffer, the caller should ensure
				the memory buffer is the same size of, or greater than the size of, the ScanVariant.
			2. This means that we need to be sure there's no race conditions or any of circumstances
				that can lead to the ScanVariant size changing between allocating the buffer, reading
				the memory, and using one of these.
			Realistically, when possible, we should try not to use ScanVariant::compareTo
	*/
	// TODO: test string scans, test all integer type scans
	inline const CompareTypeFlags compareTo(const uint8_t* memory, const bool &isLittleEndian) const
	{
		auto context = this->searchContex.get();
		ASSERT(context != nullptr); // can happen if it's a null scan variant
		return context->compareToBuffer(this, isLittleEndian, memory);
	}

	void prepareForSearch(const ScannerTarget* const target);

	void searchForMatchesInChunk(
		const uint8_t* chunk,
		const size_t &chunkSize,
		const CompareTypeFlags &compType,
		const MemoryAddress &startAddress,
		const bool &isLittleEndian,
		std::vector<size_t> &locations) const;

private:
	static ScanVariantUnderlyingTypeTraits* UnderlyingTypeTraits[SCAN_VARIANT_NULL + 1];

	ScanVariant() : type(SCAN_VARIANT_NULL) { }

	ScanVariantType type;
	std::string valueAsciiString;
	std::wstring valueWideString;
	std::vector<ScanVariant> valueStruct;
	union
	{
		uint8_t numericValue;

		uint8_t valueuint8;
		int8_t valueint8;
		uint16_t valueuint16;
		int16_t valueint16;
		uint32_t valueuint32;
		int32_t valueint32;
		uint64_t valueuint64;
		int64_t valueint64;
		double valueDouble;
		float valueFloat;
	};

	size_t valueSize;
	std::shared_ptr<ScanVariantSearchContext> searchContex;

	static const CompareTypeFlags compareRangeToBuffer(
		const ScanVariant* const obj,
		const ScanVariantComparator &comparator,
		const size_t &valueSize,
		const bool &isLittleEndian,
		const void* const target);
	static const CompareTypeFlags compareNumericToBuffer(
		const ScanVariant* const obj,
		const ScanVariantComparator &comparator,
		const size_t &valueSize,
		const bool &isLittleEndian,
		const void* const target);
	static const CompareTypeFlags comparePlaceholderToBuffer(
		const ScanVariant* const obj,
		const ScanVariantComparator &comparator,
		const size_t &valueSize,
		const bool &isLittleEndian,
		const void* const target);
	static const CompareTypeFlags compareStructureToBuffer(
		const ScanVariant* const obj,
		const ScanVariantComparator &comparator,
		const size_t &valueSize,
		const bool &isLittleEndian,
		const void* const target);
	static const CompareTypeFlags compareAsciiStringToBuffer(
		const ScanVariant* const obj,
		const ScanVariantComparator &comparator,
		const size_t &valueSize,
		const bool &isLittleEndian,
		const void* const target);
	static const CompareTypeFlags compareWideStringToBuffer(
		const ScanVariant* const obj,
		const ScanVariantComparator &comparator,
		const size_t &valueSize,
		const bool &isLittleEndian,
		const void* const target);

	void setSizeAndValue();
};