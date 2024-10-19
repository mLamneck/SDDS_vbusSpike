#include "D:\Dropbox\projects\sdds\SDDS_git\src\uSpookyMacros.h"
#include "D:\Dropbox\projects\sdds\SDDS_git\src\uEnumMacros.h"

#define sdds_enumClass2(_name,...)\
	constexpr static const char _name##_str[] = SP_FOR_EACH_PARAM_CALL_MACRO_WITH_PARAM(SDDS_SM_ENUM_STR,__VA_ARGS__);	\
	enum class _name##_TinternalEnum : int {__VA_ARGS__};\
	class _name : public sdds::TenumClaseBase<_name##_TinternalEnum, SP_COUNT_VARARGS(__VA_ARGS__),_name##_str,sizeof(_name##_str)>{\
		operator _name##_TinternalEnum() const{return Fvalue;}\
	};


#define __sdds_namedEnum11(_name, ...) \
    sdds_enumClass2(_name##_class,__VA_ARGS__);\
    typedef TenumTemplate<_name##_class> _name

