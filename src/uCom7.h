#ifndef UCOM7_H
#define UCOM7_H

#include "uPlatform.h"
#include "uCrc8.h"
#include "uSpookyMacros.h"
#include "uEnumMacros.h"

namespace com7{
    typedef int Tres;
    typedef dtypes::uint8 byte;
   	typedef dtypes::uint8 uint8;

    namespace decryptErrs{
        static const Tres INP_TOO_SHORT = -1;
        static const Tres OUTBUF_SMALL = -2;
        static const Tres EOM_IN_STB = -3;
        static const Tres CS_INVALID = -4;
        static const Tres NO_EOM = -5;
        static const Tres INV_SIZE = -6;        //EOM found prior _inLen
    }

    template<int _pos>
    constexpr uint8 decryptByteAtPos(const void* _in){
    	static_assert(_pos >= 0 && _pos <= 7, "_pos must be between 0 and 7");
    	const uint8* in = static_cast<const uint8*>(_in);
    	uint8 stuffbyte = *in;
    	uint8 b = in[_pos+1];
    	if (stuffbyte & (1<<_pos)){
    		return (b<<1)|1;
    	}
    	else{
    		return b;
    	}
    }

    Tres decrypt(const void* in, uint8 _inLen, void* out, uint8 _outSize){
    		if (_inLen < 3) return decryptErrs::INP_TOO_SHORT;
        if (_outSize < _inLen) return decryptErrs::OUTBUF_SMALL;    //toDo calculate corret size but this one is safe

        uint8* _out = static_cast<uint8*>(out);
        uint8* outStart = _out;
        
        const uint8* _in = static_cast<const uint8*>(in);
        uint8 checksum = 0;
        int stuffbyte = 0;

    		for (auto i = 0; i<_inLen; i++){
          uint8 b = *_in++;

            //load stuffbyte
            if (i%8 == 0){
                if ((b & 0x80) > 0) return decryptErrs::EOM_IN_STB;
                stuffbyte = b;
                continue;
            }

            uint8 encByte = (b << 1) | (stuffbyte & 0x01);
            checksum = crc8::tab[checksum ^ encByte];
            if ((b & 0x80) > 0){
                if (i != _inLen-1) return decryptErrs::INV_SIZE;
                if (checksum==0) return _out-outStart; 
                return decryptErrs::CS_INVALID;
            }
            *_out++ = encByte;
            stuffbyte = stuffbyte >> 1;
        }
        return decryptErrs::NO_EOM;
    }

    namespace encryptErrs{
        static const Tres INP_TOO_SHORT = -1;
        static const Tres OUTBUF_SMALL = -2;
    }

    Tres encrypt(const void* _in, uint8 _inLen, void* _out, uint8 _outSize){
        if (_inLen < 1) return encryptErrs::INP_TOO_SHORT;
        if (_inLen + _inLen/7 + 2 > _outSize) return encryptErrs::OUTBUF_SMALL;

        uint8* out = static_cast<uint8*>(_out);
        const uint8* in = static_cast<const uint8*>(_in);

        uint8* outStart = out;
        uint8* pStuffByte = out;
        uint8 checksum = 0;
        uint8 stuffByte = 0;
        uint8 packLen = 7;
        
        do{
            if (_inLen <= 7) packLen = _inLen;
            pStuffByte = out;
            out++;
            for (auto i=0; i<packLen; i++){
                uint8 b = *in++;
                checksum = crc8::tab[b ^ checksum];
                stuffByte = stuffByte | ((b & 0x01)<<i);
                *out++ = (b>>1);
            }
            *pStuffByte = stuffByte;
            stuffByte = 0;
            _inLen-=packLen;
        } while (_inLen > 0);

        //edge case for msgLength modulo 7, here we have to write the stuffbyte
        //missing in the loop + one stuffbyte containing the lsb of checksum
        if (packLen == 7){
            *out++ = (checksum & 0x01);
        } 
        else{
            *pStuffByte = *pStuffByte | ((checksum & 0x01)<<packLen);
        }
        *out++ = (checksum>>1) | 0x80;

        return out - outStart;
    }

}

#if MARKI_DEBUG_PLATFORM == 1
#include "../test/uCom7_test.h"
#endif

#endif
