#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <uMultask.h>
#include "uUartBase.h"
#include "uVbusProtocol.h"
#include "uCrc8.h"
#include "mhal/uFlashProg.h"
#include "uSerialNumber.h"

/**
 *
 */
template <
	//default flash memory layout designed for STM32G4
	dtypes::int32 _BL_SIZE=0x8000,					//32KB
	dtypes::int32 BL_APP_START=_BL_SIZE,			//APP START after BOOTLOADER AREA
	dtypes::int32 BL_APP_END=0x80000-0x1000			//512KB-4Kb for params page
>
class Tbootloader : public Tthread{
	public:
		constexpr static int BL_SIZE = _BL_SIZE;
	private:
		constexpr static dtypes::uint8	BL_VERSION 	= 3;
		constexpr static dtypes::uint8	bls_cIDLE	= 0;
		constexpr static dtypes::uint8	bls_cFLASH 	= 1;

		constexpr static dtypes::uint8	BL_CRC8_0_SV 	= 0xA1;
		constexpr static dtypes::uint8	BL_CRC8_1_SV 	= 0x08;

		typedef Tvbus485ProtStream TprotStream;
		typedef typename TuartBase::TmessageBufferRX TmessageBufferRX;
		typedef TuartBase _Tstream;

		dtypes::uint8 FnBlNotifications = 4;
		struct{
			dtypes::uint8 progMode : 2;
			bool blockRead : 1;
		} Fstatus;

		union Tcrc16{
			dtypes::uint16 crc16;
			struct{
				dtypes::uint8 crc8_0; 
				dtypes::uint8 crc8_1; 
			};
		};
		dtypes::int32 FwriteAnsCs;

		mhal::TflashProg FflashProg;
		dtypes::int32 FcurrPageNum;
		dtypes::uint8 FpageBuffer[4096];

		dtypes::int32 FblockReadAddr;
		dtypes::int32 FblockReadBytesLeft;

		//handshake events with _Tstream
		Tevent FevRx;
		Tevent FevTxIdle;
		Tevent FevTransmit;
		Tevent FevGlobalTimeout;
		_Tstream* Fstream = nullptr;

	protected:
		TprotStream Fps;

		dtypes::int32 getDevId(){
			//3bytes signatures
			dtypes::int32 devId =
			#if MARKI_DEBUG_PLATFORM
				0x112233;
			#else
				HAL_GetDEVID();
			#endif
			return devId;
		}

		const char* mySerial(){
			return mms::serialNumber::read();
		}

		virtual void startToApplication(){
			/*
			ResetClockToHSI();

			constexpr static uint32_t app_addr = mhal::TflashProg::FLASH_START_ADDR + BL_APP_START;
		    HAL_DeInit();
		    __disable_irq();
		    SCB->VTOR = app_addr;
			*/
		    /**
		     * make sure to enable interrupts in the application code...
		     */
		    //__enable_irq();

		    /**
		     * the following code with AppResetHandler doesn't work with code optimization due
		     * to the invalid stack pointer. Assembler magic resolves the issue
		     */
		    //AppResetHandler = (void (*)(void)) (*(volatile uint32_t*) (app_addr + 4));
		    //AppResetHandler();
			/*
		    __asm volatile (
		        "MSR MSP, %0\n"
		        "BX %1"
		        :
		        : "r" (*(volatile uint32_t*)app_addr),
		          "r" (*(volatile uint32_t*)(app_addr + 4))
		    );
		    */
		}

		void buildErrorMsg(const dtypes::uint8 _err){
			Fps.buildErrMsg(_err,0);
			Fps.func(TvbusProtocoll::bl_error);
		}


		/******************************************
		 * read messages
		 ******************************************/

		/**
		 * @brief handle function to change progMode
		 * 
		 */
		void handleProgmode(){
			dtypes::uint8 pm = 0;
			if (!Fps.readVal(pm))
				return;
			
			if (pm == 0)
				return startToApplication();
			if (pm > 1)
				return buildErrorMsg(TvbusProtocoll::bl_err_invProgMode);

			auto serial = Fps.readString();
			if (serial == mySerial()){
				Fstatus.progMode = pm;
				FcurrPageNum = -1;
				Fps.setReturnHeader();
				Fps.writeVal(pm);
				Fps.writeString(mySerial());
				writeInt24(getDevId());
				Fps.setSendPending();
				FevGlobalTimeout.reclaim();
			} else
				startToApplication();
		}

		Tcrc16 calcCrc16(TprotStream _ps){
			Tcrc16 cs;
			cs.crc8_0 = BL_CRC8_0_SV;
			cs.crc8_1 = BL_CRC8_1_SV;
			dtypes::uint8 b = 0;
			while (_ps.bytesAvailableForRead()){
				_ps.readVal(b);
				crc8::calc(cs.crc8_0,b);
				crc8::calc(cs.crc8_1,b);
			}
			return cs;
		}

		union Tint24{
			dtypes::uint32 addr;
			struct{
				dtypes::uint8 addr_0; 
				dtypes::uint8 addr_1; 
				dtypes::uint8 addr_2; 
			};
		};
		
		dtypes::int32 readInt24(){
			Tint24 addr;
			addr.addr = 0;
			Fps.readVal(addr.addr_0);
			Fps.readVal(addr.addr_1);
			Fps.readVal(addr.addr_2);
			return addr.addr;
		}

		void writeInt24(dtypes::int32 _addr){
			Tint24 addr;
			addr.addr = _addr;
			Fps.writeVal(addr.addr_0);
			Fps.writeVal(addr.addr_1);
			Fps.writeVal(addr.addr_2);
		}

		bool progmessageValid(){
			if (Fps.bytesAvailableForRead() < 3){
				buildErrorMsg(TvbusProtocoll::bl_err_invPacketSize);
				return false;
			}

			dtypes::uint8 b = 0;
			crc8::Tcrc cs8 = 0;
			Tcrc16 cs16;
			cs16.crc8_0 = BL_CRC8_0_SV;
			cs16.crc8_1 = BL_CRC8_1_SV;
			while (Fps.bytesAvailableForRead()>1){
				Fps.readVal(b);
				crc8::calc(cs8,b);
				crc8::calc(cs16.crc8_0,b);
				crc8::calc(cs16.crc8_1,b);
			}
			FwriteAnsCs = cs16.crc16;
			Fps.readVal(b);
			if (b == cs8)
				return true;
			buildErrorMsg(TvbusProtocoll::bl_err_invCS);
			return false;
		}

		bool flushCurrentPage(){
			if (FcurrPageNum == -1)
				return true;
			if (!FflashProg.Write(FflashProg.getAbsAddrFromPage(FcurrPageNum),&FpageBuffer[0],FflashProg.FpageSize)){
				buildErrorMsg(TvbusProtocoll::bl_err_progFailed);
				Fps.writeVal(FflashProg.lastError());
				return false;
			};
			FcurrPageNum = -1;
			return true;
		}

		bool addrValidForWrite(dtypes::int32 _addr, dtypes::int32 _size){
			return (_addr >= BL_APP_START && (_addr+_size) < BL_APP_END);
		}

		dtypes::int32 addrToZeroBased(const dtypes::int32 _addr){
			return _addr - BL_APP_START;
		}

		/**
		 * @brief handle function to change progMode
		 * 
		 */
		void handleWriteReq(TmessageBufferRX* _msg){
			if (Fstatus.progMode != bls_cFLASH)
				return;
			if (!progmessageValid())
				return;

			Fps.init(&_msg->data[0],_msg->length-1);
			dtypes::int32 reqAddr = readInt24();
			
			//flush request
			if (reqAddr == 0 && Fps.bytesAvailableForRead() == 0){
				if (!flushCurrentPage())
					return;
			}
			else{
				if (!addrValidForWrite(reqAddr,Fps.bytesAvailableForRead()))
					return buildErrorMsg(TvbusProtocoll::bl_err_invAddr);

				dtypes::int32 bufAddr = reqAddr - FflashProg.getRelAddrFromPage(FflashProg.getPageFromRelAddr(reqAddr));
				dtypes::uint8 data = 0;
				while (Fps.readVal(data)){
					auto page = FflashProg.getPageFromRelAddr(reqAddr);
					if (FcurrPageNum != page){
						if (!flushCurrentPage())
							return;
						FcurrPageNum = page;
						dtypes::int32 pageStartAddr = FflashProg.getAbsAddrFromPage(page);
						for (dtypes::uint32 i=0; i<FflashProg.getPageSize(page)-1; i++)
							FflashProg.readByte(pageStartAddr+i,FpageBuffer[i]);
						bufAddr = reqAddr - FflashProg.getRelAddrFromPage(FflashProg.getPageFromRelAddr(reqAddr));
					}
					FpageBuffer[bufAddr++] = data;
					reqAddr++;
				}
			}

			Fps.setReturnHeader();
			Fps.source(0xFF);
			Tcrc16 cs;
			cs.crc16 = FwriteAnsCs;
			Fps.writeVal(cs.crc8_0);
			Fps.writeVal(cs.crc8_1);
			Fps.setSendPending();
		}

		void handleReadReq(){
			if (Fstatus.progMode != bls_cFLASH)
				return;
			FblockReadAddr = readInt24();
			FblockReadBytesLeft = readInt24();
			dtypes::int32 endAddr = FblockReadAddr + FblockReadBytesLeft;
			if (static_cast<dtypes::uint32>(endAddr) > FflashProg.getFlashSize())
				endAddr = FflashProg.getFlashSize();
			if (FblockReadAddr > endAddr)
				FblockReadAddr = endAddr;
			FblockReadBytesLeft = endAddr - FblockReadAddr;
			Fstatus.blockRead = true;
			FevTransmit.setTimeEvent(10);
		}

		void _readMessage(TmessageBufferRX* _msg){
			Fps.init(&_msg->data[0],_msg->length);
			if (Fps.source() != Fps.ADDR_BROADCAST())
				return;
			switch(Fps.func()){
				case TvbusProtocoll::bl_enterProgModeReq: 
					return handleProgmode();
				case TvbusProtocoll::bl_read_req:
					return handleReadReq();
				case TvbusProtocoll::bl_write_req:
					return handleWriteReq(_msg);
			}
		}

		bool readMessage(TmessageBufferRX* _msg){
			_readMessage(_msg);
			if (Fps.sendPending()){
				sendMessage();
				return true;
			}
			return false;
		}

		void readMessages(){
			auto msg = Fstream->getMessage();
			if (!msg) return;

			while (msg){
				if (readMessage(msg))
					return;
				Fstream->ackMessage(msg);
				msg = Fstream->getMessage();
			}
		}

		/******************************************
		 * send messages
		 ******************************************/

		 bool initFps(TprotStream::t_prot_func _func){
			auto buf = Fstream->getTxBuffer();
			if (!buf) return false;
			Fps.init(buf->data);
			Fps.setHeader(Fps.ADDR_BROADCAST(),0,_func);
			Fps.source(Fps.ADDR_BROADCAST());
			return true;
		}

		void sendMessage(dtypes::uint8* _buf, int len){
			Fstream->write(_buf,len);
			setPriority(2);
		}

		void sendMessage(){
			Fps.source(0xFF);
			auto _buf = Fps.buffer();
			int len = Fps.length();
			sendMessage(_buf,len);
		}

		void sendBlNotification(){
			if (!initFps(TvbusProtocoll::bl_notification)) return;

			Fps.writeByte(BL_VERSION);

			writeInt24(getDevId());

			Fps.writeString(mySerial());
			sendMessage();
		}

		void handleTransmit(){
			if (Fstatus.blockRead){
				if (!initFps(TvbusProtocoll::bl_read)) return;
				
				writeInt24(FblockReadAddr);
				dtypes::int32 nBytes = FblockReadBytesLeft;
				if (nBytes > Fps.spaceAvailableForWrite() - 2){
					nBytes = Fps.spaceAvailableForWrite() - 2;
					FevTransmit.signal();
				}
				FblockReadBytesLeft -= nBytes;
				while (nBytes-- > 0){
					dtypes::uint8 d;
					FflashProg.readByte(FblockReadAddr++ + FflashProg.getFlashStartAddr(),d);
					Fps.writeByte(d);
				}

				TprotStream ps;
				ps.init(Fps.buffer(),Fps.length());
				auto cs = calcCrc16(ps);
				Fps.writeVal(cs.crc16);
				sendMessage();
			}

			else if (Fstatus.progMode == bls_cIDLE) {
				if (FnBlNotifications-- > 0){
					sendBlNotification();
					FevTransmit.setTimeEvent(200);
				} else{
					//FevTransmit.setTimeEvent(200);
					startToApplication();
				}
			}
		}

		void execute(Tevent* _ev) override{
			if (_ev == &FevRx)
				readMessages();
			else if (_ev == &FevTransmit)
				handleTransmit();
			else if (_ev == &FevTxIdle){
				setPriority(0);
				readMessages();
			}
			else if (_ev == &FevGlobalTimeout)
				startToApplication();
			else if (isTaskEvent(_ev)){
				board::TledRed::toggle();
				setTimeEvent(200);
			}
		}
	public:
		Tbootloader(_Tstream* _stream)
			: FevRx(this)
			, FevTxIdle(this,2)
			, FevTransmit(this,1)
			, FevGlobalTimeout(this,3)
			, Fstream(_stream)
		{
			Fstream->begin(&FevRx,&FevTxIdle);
			//toDo: find out why we miss one message if we trigger this immediately
			FevTransmit.setTimeEvent(5);
			FevGlobalTimeout.setTimeEvent(5000);
		}

};

#if MARKI_DEBUG_PLATFORM

class TmmsBootloaderDebug : public Tbootloader{
	private:
		typename TuartBase::TmessageBufferRX FtxBuffer;

		int parseStr(uint8_t* _out, const char* _in){
			auto pEnd = _in + strlen(_in);
			auto pOutStart = _out;
			while (_in < pEnd) {
				//_in += 2;
				dtypes::string byteString(_in,_in+2);
				uint8_t b = static_cast<uint8_t>(
					stoi(byteString, nullptr, 16));
				//Fps.writeByte(b);
				*_out++ = b;
				_in+=3;
			}
			return _out - pOutStart;
		}

		void stimulReadRequest(dtypes::int32 _addr = 0, int len = 50){
			Fps.init(&FtxBuffer.data[0]);
			Fps.setHeader(0xFF,0,TvbusProtocoll::bl_read_req);
			Fps.source(Fps.ADDR_BROADCAST());
			writeInt24(_addr);
			writeInt24(len);
			for (int i=1; i<23; i++)
				Fps.writeByte(i);
			crc8::Tcrc c = crc8::calc(&FtxBuffer.data[5],Fps.length()-5);
			Fps.writeByte(c);
			FtxBuffer.length = Fps.length();
			readMessage(&FtxBuffer);
		}

		Tevent FevStimul;
		int stimStatus = 2;

		void execute(Tevent* _ev) override{
			if (_ev != &FevStimul)
				return Tbootloader::execute(_ev);

			//read request while idle
			if (stimStatus == 0){
				stimulReadRequest();
				FevStimul.setTimeEvent(10);
				stimStatus++;
				return;
			}

			//invalid progmode
			if (stimStatus == 1){
				Fps.init(&FtxBuffer.data[0]);
				Fps.setHeader(0xFF,0,TvbusProtocoll::bl_enterProgModeReq);
				Fps.source(0xFF);
				Fps.writeString(mySerial());
				FtxBuffer.length = Fps.length();
				readMessage(&FtxBuffer);
				FevStimul.setTimeEvent(10);
				stimStatus++;
				return;
			}

			//progmode = flash
			if (stimStatus++ == 2){
				Fps.init(&FtxBuffer.data[0]);
				Fps.setHeader(0xFF,0,TvbusProtocoll::bl_enterProgModeReq);
				Fps.source(0xFF);
				Fps.writeByte(1);
				Fps.writeString(mySerial());
				FtxBuffer.length = Fps.length();
				readMessage(&FtxBuffer);
				FevStimul.setTimeEvent(10);
				stimStatus++;
				return;
			}

			/*
			write to 0x50000 -> 00 ff ff 00 67 00 00 05 30 30 30 30 30 30 a5
			write to 0x60000 -> 00 ff ff 00 67 00 00 06 30 30 30 30 30 30 e2 
			flush-> 00 ff ff 00 67 00 00 00 00 
			*/
			FtxBuffer.length = parseStr(&FtxBuffer.data[0],"00 ff ff 00 67 e2 0f 01 00 bf 70 47 00 bf 05 4b da 6c 02 43 da 64 db 6c 82 b0 03 40 01 93 f5");
			readMessage(&FtxBuffer);
			FtxBuffer.length = parseStr(&FtxBuffer.data[0],"00 ff ff 00 67 f8 0f 01 01 9b 02 b0 70 47 00 bf 00 10 02 40 70 47 70 47 70 47 70 47 70 47 7d ");
			readMessage(&FtxBuffer);
			return;


			stimulReadRequest(0xFFFFFF,1);
			return;

			//01.03.25-20:13:38.473 - TexternalCode: addr = 0x4000; crc16=0x7DED
			FtxBuffer.length = parseStr(&FtxBuffer.data[0],"00 ff ff 00 67 00 40 00 00 00 02 20 d1 44 00 08 35 44 00 08 3d 44 00 08 45 44 00 08 4d 44 df");
			readMessage(&FtxBuffer);
			//01.03.25-20:13:38.473 - TexternalCode: addr = 0x4016; crc16=0xA737
			FtxBuffer.length = parseStr(&FtxBuffer.data[0],"00 ff ff 00 67 16 40 00 00 08 55 44 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05");
			readMessage(&FtxBuffer);
			//01.03.25-20:13:38.473 - TexternalCode: addr = 0x402C; crc16=0x8313
			FtxBuffer.length = parseStr(&FtxBuffer.data[0],"00 ff ff 00 67 2c 40 00 5d 44 00 08 6b 44 00 08 00 00 00 00 79 44 00 08 87 44 00 08 21 45 21");
			readMessage(&FtxBuffer);
			//01.03.25-20:13:38.520 - TexternalCode: addr = 0x57CE; crc16=0x1282
			FtxBuffer.length = parseStr(&FtxBuffer.data[0],"00 ff ff 00 67 ce 57 00 70 47 f8 b5 00 bf f8 bc 08 bc 9e 46 70 47 00 00 00 00 00 00 00 00 b0");
			readMessage(&FtxBuffer);
			
			stimulReadRequest(0x4000,22*3);
			return;

			/*
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x4000; crc16=0x7DED
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 00 40 00 00 00 02 20 d1 44 00 08 35 44 00 08 3d 44 00 08 45 44 00 08 4d 44 df 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x4016; crc16=0xA737
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 16 40 00 00 08 55 44 00 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x402C; crc16=0x8313
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 2c 40 00 5d 44 00 08 6b 44 00 08 00 00 00 00 79 44 00 08 87 44 00 08 21 45 21 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x4042; crc16=0x5BCB
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 42 40 00 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 f9 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x4058; crc16=0xA535
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 58 40 00 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 07 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x406E; crc16=0x1282
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 6e 40 00 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 b0 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x4084; crc16=0xBC2C
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 84 40 00 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 1e 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x409A; crc16=0x1383
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 9a 40 00 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 b1 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x40B0; crc16=0xA9A
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 b0 40 00 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 a8 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x40C6; crc16=0xD040
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 c6 40 00 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 72 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x40DC; crc16=0xC454
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 dc 40 00 21 45 00 08 93 44 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 66 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x40F2; crc16=0x66F6
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 f2 40 00 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 c4 
			01.03.25-20:13:38.473 - TexternalCode: addr = 0x4108; crc16=0x38A8
			01.03.25-20:13:38.473 - TexternalCode: < 00 ff ff 00 67 08 41 00 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 00 08 21 45 9a 			
			...
			01.03.25-20:13:38.520 - TexternalCode: addr = 0x57CE; crc16=0x1282
			01.03.25-20:13:38.520 - TexternalCode: < 00 ff ff 00 67 ce 57 00 70 47 f8 b5 00 bf f8 bc 08 bc 9e 46 70 47 00 00 00 00 00 00 00 00 b0 
			*/

			
			Fps.init(&FtxBuffer.data[0]);
		}
	public:
		TmmsBootloaderDebug(TuartBase* _stream) 
			: Tbootloader(_stream)
			,FevStimul(this,2)
		{
			FevStimul.setTimeEvent(10);
		}


};

#endif

#endif
