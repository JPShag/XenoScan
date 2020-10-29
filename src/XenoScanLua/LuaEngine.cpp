#include "LuaEngine.h"

LuaEngine::LuaEngine(void)
{
	for (auto exp = __luaEngineExports.begin(); exp != __luaEngineExports.end(); exp++)
		this->pushGlobal(exp->first, exp->second());
}

LuaEngine::~LuaEngine(void) {}

void LuaEngine::doThink()
{
	auto currentTime = std::chrono::high_resolution_clock::now();
	std::vector<LuaVariant> args;
	for (auto evt = this->timedEvents.begin(); evt != this->timedEvents.end(); )
	{
		if (evt->executeTime < currentTime)
		{
			if (!this->executeFunction(evt->function, args, 0, evt->function))
			{
				// TODO maybe report the error ?
			}
			evt = this->timedEvents.erase(evt);
		}
		else
			evt++;
	}
}

LuaVariant LuaEngine::createLuaMemoryInformation(const MemoryInformation& meminfo) const
{
	LuaVariant::LuaVariantKTable info;

	info["start"] = LuaVariant((LuaVariant::LuaVariantPointer)meminfo.allocationBase);
	info["end"] = LuaVariant((LuaVariant::LuaVariantPointer)meminfo.allocationEnd);
	info["size"] = LuaVariant((LuaVariant::LuaVariantInt)meminfo.allocationSize);

	info["isModule"] = meminfo.isModule;
	info["isCommitted"] = meminfo.isCommitted;
	info["isMirror"] = meminfo.isMirror;
	info["isWriteable"] = meminfo.isWriteable;
	info["isExecutable"] = meminfo.isExecutable;
	info["isMappedImage"] = meminfo.isMappedImage;
	info["isMapped"] = meminfo.isMapped;
	return info;
}

LuaVariant LuaEngine::createLuaObject(const std::string& typeName, const void* pointer) const
{
	LuaVariant::LuaVariantKTable target;
	target["objectType"] = LuaVariant(typeName);
	target["objectPointer"] = LuaVariant((LuaVariant::LuaVariantPointer)pointer);
	return target;
}

bool LuaEngine::getLuaObject(const LuaVariant& object, const std::string& typeName, void* &pointer) const
{
	LuaVariant::LuaVariantKTable res;
	if (!object.getAsKTable(res)) return false;

	auto it = res.find("objectType");
	if (it == res.end()) return false;

	std::string type;
	if (!it->second.getAsString(type)) return false;
	if (type != typeName) return false;

	it = res.find("objectPointer");
	if (it == res.end()) return false;

	if (!it->second.getAsPointer(pointer)) return false;
	return true;
}

bool LuaEngine::getScannerPair(const LuaVariant& object, ScannerPairList::const_iterator &iterator) const
{
	void* objectPointer;
	if (!this->getLuaObject(object, "ScannerPair", objectPointer)) return false;

	// TODO: put into a map keyed on pointer 
	for (auto isearch = this->scanners.cbegin(); isearch != this->scanners.cend(); isearch++)
	{
		if ((void*)(*isearch).get() == objectPointer)
		{
			iterator = isearch;
			return true;
		}
	}
	return false;
}

LuaEngine::ScannerPairShPtr LuaEngine::getArgAsScannerObject(const std::vector<LuaVariant>& args) const
{
	LuaVariant::LuaVariantKTable _scanner;
	args[0].getAsKTable(_scanner);
	ScannerPairList::const_iterator scanner;
	if (!this->getScannerPair(_scanner, scanner)) return nullptr;
	return *scanner;
}

const ScanVariant LuaEngine::getScanVariantFromLuaVariant(const LuaVariant &variant, const ScanVariant::ScanVariantType &type, bool allowBlank) const
{

	switch (variant.getType())
	{
	case LUA_VARIANT_STRING:
		{
			LuaVariant::LuaVariantString memberValue;
			variant.getAsString(memberValue);
			if (memberValue.length() == 0)
				break;
			return ScanVariant::FromStringTyped(memberValue, type);
		}
	case LUA_VARIANT_INT:
		{
			if (type < ScanVariant::SCAN_VARIANT_NUMERICTYPES_BEGIN ||
				type > ScanVariant::SCAN_VARIANT_NUMERICTYPES_END)
				break;

			LuaVariant::LuaVariantInt memberValueInt;
			variant.getAsInt(memberValueInt);
			return ScanVariant::FromNumberTyped(memberValueInt, type);
		}
	case LUA_VARIANT_KTABLE:
		{
			LuaVariant::LuaVariantKTable value;
			if (!variant.getAsKTable(value))
				break;

			auto itMin = value.find("__min");
			auto itMax = value.find("__max");
			if (itMin != value.end() && itMax != value.end())
			{
				if (type < ScanVariant::SCAN_VARIANT_NUMERICTYPES_BEGIN ||
					type > ScanVariant::SCAN_VARIANT_NUMERICTYPES_END)
					return ScanVariant::MakeNull();

				LuaVariant::LuaVariantInt minValue, maxValue;
				if (!itMin->second.getAsInt(minValue) ||
					!itMax->second.getAsInt(maxValue))
					return ScanVariant::MakeNull();

				auto min = ScanVariant::FromNumberTyped(minValue, type);
				auto max = ScanVariant::FromNumberTyped(maxValue, type);
				return ScanVariant::FromVariantRange(min, max);
			}

			if (value.size() == 0 && allowBlank)
			{
				if (type < ScanVariant::SCAN_VARIANT_NUMERICTYPES_BEGIN ||
					type > ScanVariant::SCAN_VARIANT_NUMERICTYPES_END)
					break;
				return ScanVariant::MakePlaceholder(type);
			}

			break;
		}
	case LUA_VARIANT_ITABLE:
		{
			LuaVariant::LuaVariantITable value;
			if (!variant.getAsITable(value))
				break;

			if (value.size() == 0 && allowBlank)
			{
				if (type < ScanVariant::SCAN_VARIANT_NUMERICTYPES_BEGIN ||
					type > ScanVariant::SCAN_VARIANT_NUMERICTYPES_END)
					break;
				return ScanVariant::MakePlaceholder(type);
			}

			break;
		}
	default:
		break;
	}

	return ScanVariant::MakeNull();
}

LuaVariant LuaEngine::getLuaVariantFromScanVariant(const ScanVariant &variant) const
{
	// this isn't exactly clean, but we do it to keep the code
	// as fast as possible without too much duplication pre-compilation
#define TYPE_TO_LUA_VARIANT(VAR_TYPE, RAW_TYPE) \
	case ScanVariant::VAR_TYPE: \
	{ \
		RAW_TYPE value; \
		if (variant.getValue(value)) \
			return LuaVariant(value); \
		break; \
	}


	auto traits = variant.getTypeTraits();
	if (variant.isComposite())
	{
		auto values = variant.getCompositeValues();
		LuaVariant::LuaVariantITable complex;
		for (auto v = values.begin(); v != values.end(); v++)
			complex.push_back(this->getLuaVariantFromScanVariant(*v));
		return complex;
	}
	else if (traits->isStringType())
	{
		return LuaVariant(variant.toString());
	}
	else if (traits->isNumericType())
	{
		switch (variant.getType())
		{
			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_DOUBLE, double);
			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_FLOAT, float);

			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_INT8, int8_t);
			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_UINT8, uint8_t);

			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_INT16, int16_t);
			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_UINT16, uint16_t);

			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_INT32, int32_t);
			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_UINT32, uint32_t);

			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_INT64, int64_t);
			TYPE_TO_LUA_VARIANT(SCAN_VARIANT_UINT64, uint64_t);
		}
	}

	return LuaVariant();

#undef TYPE_TO_LUA_VARIANT
}