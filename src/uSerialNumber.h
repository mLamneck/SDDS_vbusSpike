#ifndef USERIALNUMBER_H_
#define USERIALNUMBER_H_

#include <uPlatform.h>

/**
 * there is no error if mms_serial_number is not declared in linker script. In this case the compiler will
 * create the section somewhere in the memory -> Make sure to declare this section in the linker script
 * at the desired memory addr.B
 */
namespace mms{
	namespace serialNumber{
		constexpr static int MMS_SER_LENGTH = 15;

		const char* defaultSerial(){ return "DEF_SERIAL"; }

		const char* read(){
			#if MARKI_DEBUG_PLATFORM == 1
			return defaultSerial();
			#else
			auto pSerial = (reinterpret_cast<uint8_t*>(0x08008000-(MMS_SER_LENGTH+1)));
			if (pSerial[MMS_SER_LENGTH] != 0)
				return defaultSerial();
			for (int i = 0; i<MMS_SER_LENGTH; i++){
				if ((pSerial[i] < 32) && (pSerial[i] > 126))
					return defaultSerial();
			}
			return reinterpret_cast<const char*>(pSerial);
			#endif
		}
	}
}

#endif //USERIALNUMBER_H_
