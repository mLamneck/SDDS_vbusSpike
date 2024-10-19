#ifndef UVBUSPROTOCOL_H
#define UVBUSPROTOCOL_H

#include "uPlatform.h"

class TvbusProtocoll{
	public:
		/**  */
		static constexpr dtypes::uint8 fLAST_MSG = 0x80;

		/** func */
		
		static constexpr auto request = 0x01;

		static constexpr auto dhcp_firstFunc = 0x00;
		static constexpr auto dhcp_queryReq = 0x00 | request;		
		static constexpr auto dhcp_set = 0x02;
		static constexpr auto dhcp_req = 0x02 | request;
		static constexpr auto dhcp_whoIs = 0x04;
		static constexpr auto dhcp_whoIsReq = 0x04 | request;
		static constexpr auto dhcp_ka = 0x06;
		static constexpr auto dhcp_imServer = 0x08;
		static constexpr auto dhcp_lastFunc = 0x09;

		static constexpr auto port_open = 0x0A | request;
		static constexpr auto port_close = 0x0C | request;
		static constexpr auto port_ka = 0x0E;

		static constexpr auto dns = 0x18;
		static constexpr auto dns_req = dns | request;

		//server funtions
		static constexpr auto ds_type = 0x20;
		static constexpr auto ds_type_req = 0x20 | request;
		static constexpr auto ds_link = 0x22;
		static constexpr auto ds_link_req = ds_link | request;
		static constexpr auto ds_fpdw = 0x24;
		static constexpr auto ds_fpdw_req = ds_fpdw | request;
		static constexpr auto ds_fpdr = 0x26;
		static constexpr auto ds_fpdr_req = ds_fpdr | request;

		static constexpr auto func_error = 0xFE;

		/** error codes */

		static constexpr auto err_serverBusy 	= 0xFF;
		static constexpr auto err_invalidPort 	= 0xFE;
		static constexpr auto err_invalidPath 	= 0xFD;

		static constexpr auto err_noMorePorts 	= 0xEB;


/*
.equ	NoError					=	0
.equ    ErrServerBusy			=	255		;0xFF
.equ    ErrInvalidPort    		=  	254		;0xFE
.equ    ErrInvalidPath    		=   253		;0xFD
.equ	ErrNoConnAvail	  		=	252		;0xFC
.equ	ErrOutOfMemory	  		=	251		;0xFB
.ifndef ErrNoMoreEvents
.equ	ErrNoMoreEvents			=	250		;0xFA
;.equ	ErrNoMorePorts			= 	236		;0xEB
.endif
.equ	ErrPointerNil			=	249		;0xF9
.equ	ErrWrongArrayAccess		=	248		;0xF8
.equ	ErrOutOfData		    =	247		;0xF7
.equ	ErrEOF					=	246		;0xF6
.equ	ErrNoneOfMyBusiness		=	245		;0xF5
.equ	ErrNotAllowed			=	244		;0xF4
.equ	ErrTimeout				=	243		;0xF3
.ifndef ErrInvalidEvID
.equ	ErrInvalidEvID			=	242		;0xF2
.endif
.equ	ErrServerNotInstalled	=	241		;0xF1
.equ	ErrWrongLinkTime		=	240		;0xF0
.equ	ErrPortAccessDenied		=	239		;0xEF		port/id don´t match in close port request
.equ	ErrNoHandler			=	238		;0xEE
.equ	ErrHashMismatch			= 	237		;0xED

.equ	ErrNoMorePorts			= 	236		;0xEC
*/
};

class TbufferStream{
	typedef dtypes::uint8 uint8;
	protected:
		dtypes::uint8* Fbuffer = nullptr;
		int FbufferSize = 0;
		int FbytesAvailableForRead = 0;
		int FreadPos = 0;
		int FwritePos = 0;
	public:
		constexpr dtypes::uint8* buffer() { return Fbuffer; }

		int writePos() { return FwritePos; }
		void setWritePos(int _val) { FwritePos = _val; }
		int readPos() { return FreadPos; }
		void setReadPos(int _val) { FreadPos = _val; }

		void init(dtypes::uint8* _buffer, int _size, int _availableForRead = 0){
			Fbuffer = static_cast<uint8*>(_buffer);
			FbufferSize = _size;
			FbytesAvailableForRead = _availableForRead;
			FreadPos = 0;
			FwritePos = 0;
		}

		void move(int _nBytes){
			memcpy(&Fbuffer[FwritePos], &Fbuffer[FreadPos], _nBytes);
			FreadPos+=_nBytes;
			FwritePos+=_nBytes;
		}

	 	int bytesAvailableForRead() { return FbytesAvailableForRead - FreadPos; }
		
		bool hasSpaceFor(const uint8 _size){
			return ((FwritePos + _size) < FbufferSize);
		}

		int spaceAvailableForWrite() { return FbufferSize - FwritePos; }
		
		bool writeByte(const uint8 _byte){ return writeVal(_byte); }
		bool writeWord(const dtypes::uint16 _val){ return writeVal(_val); }
		
		void writeBytesNoCheck(const void* _ptr, int _n){
			memcpy(&Fbuffer[FwritePos], _ptr, _n);
			FwritePos += _n;
		}

		int writeBytes(const void* _ptr, int _n, bool _skipIfdoesntFit = false){
			auto spaceAvailable = spaceAvailableForWrite();
			if (_n > spaceAvailable){
				if (_skipIfdoesntFit) return 0;
				_n = spaceAvailable;
			} 
			memcpy(&Fbuffer[FwritePos], _ptr, _n);
			FwritePos += _n;
			return _n;
		}

		int readBytes(void* _dst, int _n){
			if (bytesAvailableForRead() < _n) return 0;
			memcpy(_dst, &Fbuffer[FreadPos], _n);
			FreadPos += _n;
			return _n;
		}

		template <typename T>
		bool writeVal(const T _value){
			if (!hasSpaceFor(sizeof(T))) return false;
			/*
			 * *reinterpret_cast<T*> leads to hard fault on STM32 because of memory alignment
			 * -> use memcpy instead
			 */
			//*reinterpret_cast<T*>(&Fbuffer[FwritePos]) = _value;
			memcpy(&Fbuffer[FwritePos], &_value, sizeof(T));
			FwritePos += sizeof(T);
			return true;
		}

		template <typename T>
		bool readVal(T& _value){
			if (bytesAvailableForRead() < (int)sizeof(T)) return false;
			memcpy(&_value,&Fbuffer[FreadPos], sizeof(T));
			FreadPos += sizeof(T);
			return true;
		}

		bool readOfs(dtypes::uint8 _ofs){
			if (_ofs >= bytesAvailableForRead()) return false;
			FreadPos += _ofs;
			return true;
		}

		template <typename T, int OFS>
		void writeValToOfs(const T _val){
			memcpy(&Fbuffer[OFS], &_val, sizeof(T));
		};

		template <int OFS>
		constexpr void writeValToOfs(const dtypes::uint8 _val){
			Fbuffer[OFS]=_val;
		}

		template <typename T>		
		void writeValToOfs(int _ofs, const T _val){
			memcpy(&Fbuffer[_ofs], &_val, sizeof(T));
		}

		void writeValToOfs(int _ofs, const dtypes::uint8 _val){
			Fbuffer[_ofs] =  _val;
		}

		void writeByteToOfs(int _ofs, const dtypes::uint8 _val){
			Fbuffer[_ofs] =  _val;
		}

		template <typename T, int OFS>
		T readValFromOfs() {
			T value;
			memcpy(&value, &Fbuffer[OFS], sizeof(T));
			return value;
		}

		template <int OFS>
		constexpr dtypes::uint8 readValFromOfs() {
			return Fbuffer[OFS];
		}
};

template <
    typename _t_prot_addr,
    typename _t_prot_port,
	int max_msg_length
>
class TvbusProtStream : public TbufferStream{
	public:
		typedef TvbusProtStream<
			_t_prot_addr,
			_t_prot_port,
			max_msg_length
		> TprotStream;

		/* typedefs for protocoll */
		typedef dtypes::uint8 uint8;
		typedef uint8 t_prot_ctrl;
		typedef _t_prot_addr t_prot_addr;
		typedef _t_prot_port t_prot_port;
		typedef dtypes::uint8 t_path_length;
		typedef dtypes::uint8 t_path_entry;
		typedef uint8 t_prot_func;
		typedef dtypes::uint16 t_prot_cs;
		typedef dtypes::uint8 t_prot_error;
		typedef dtypes::uint8 t_prot_msgCnt;
		typedef dtypes::uint8 t_prot_type;
		static int constexpr PATH_ENTRY_MAX = dtypes::high<t_path_entry>();
		
		typedef TbinLocator<t_path_length, t_path_entry> BinLocator;

		typedef uint8 TbufferPos;
		typedef TbufferPos TmessageSize;

		/* others*/
		static int constexpr c_max_msg_length = max_msg_length;
	
		static int constexpr OFS_CTRL = 0;
		static int constexpr OFS_DEST = OFS_CTRL + sizeof(t_prot_ctrl);
		static int constexpr OFS_SRC = OFS_DEST + sizeof(t_prot_addr);
		static int constexpr OFS_PORT = OFS_SRC + sizeof(t_prot_addr);
		static int constexpr OFS_FUNC = OFS_PORT + sizeof(t_prot_port);
		static int constexpr HEADER_SIZE = OFS_FUNC + sizeof(t_prot_func);

		static int constexpr OFS_MSG_CNT = HEADER_SIZE;
		static int constexpr OFS_NTH_ITEM = OFS_MSG_CNT + sizeof(t_prot_msgCnt);

		//toDo: implement for extended protocolls
		constexpr static t_prot_addr ADDR_BROADCAST() { return 0xFF; }
		constexpr static bool isBroadcast(const t_prot_addr _addr){
			return (_addr == ADDR_BROADCAST());
		}

	private:
		//flag to be set after writing a message into buffer.
		bool FsendPending = false;
	public:
		constexpr bool sendPending(){ return FsendPending; }
		constexpr void setSendPending(){ FsendPending = true; }		

		constexpr void init(uint8* _buffer){
			TbufferStream::init(_buffer,c_max_msg_length);
			Fbuffer = static_cast<uint8*>(_buffer);
			FreadPos = HEADER_SIZE;
			FwritePos = HEADER_SIZE;
			FsendPending = false;
		}

		void init(uint8* _buffer, TmessageSize _size){
			init(_buffer);
			FbytesAvailableForRead = _size;
		}

		constexpr void ctrl(const t_prot_ctrl _val) { writeValToOfs<OFS_CTRL>(_val); }
		constexpr t_prot_ctrl ctrl() { return readValFromOfs<t_prot_ctrl, OFS_CTRL>(); }

		constexpr void destiny(const t_prot_addr _val) { writeValToOfs<OFS_DEST>(_val); }
		constexpr t_prot_addr destiny() { return readValFromOfs<t_prot_addr, OFS_DEST>(); }

		constexpr void source(const t_prot_addr _val) { writeValToOfs<OFS_SRC>(_val); }
		constexpr t_prot_addr source() { return readValFromOfs<t_prot_addr, OFS_SRC>(); }

		constexpr void port(const t_prot_port _val) { writeValToOfs<OFS_PORT>(_val); }
		constexpr t_prot_port port() { return readValFromOfs<t_prot_port, OFS_PORT>(); }

		constexpr void func(const t_prot_func _val) { writeValToOfs<OFS_FUNC>(_val); }
		constexpr t_prot_func func() { return readValFromOfs<t_prot_func, OFS_FUNC>(); }

		constexpr void msgCnt(const t_prot_msgCnt _val) { writeValToOfs<OFS_MSG_CNT>(_val); }
		constexpr t_prot_msgCnt msgCnt() { return readValFromOfs<t_prot_msgCnt, OFS_MSG_CNT>(); }

		constexpr bool writeCs(const t_prot_cs _val){ return writeVal(_val); }
		constexpr bool writePort(const t_prot_port _val){ return writeVal(_val); }
		constexpr bool writeMsgCnt(const t_prot_msgCnt _val){ return writeVal(_val); }

		bool writePathEntry(const t_path_entry _val){ return writeVal(_val); }

		constexpr TsubStringRef readString(){
			TsubStringRef s;
			dtypes::uint8 len;
			if (!readVal(len)) return s;
			if (len > bytesAvailableForRead()) return s;
			s.init((char*)&Fbuffer[FreadPos],len);
			FreadPos+=len;
			return s;
		}

		constexpr TsubStringRef getText(){
			TsubStringRef s;
			s.init((char*)&Fbuffer[FreadPos],bytesAvailableForRead());
			return s;
		}

		constexpr bool writeString(const char* _str, uint8 strLen, bool _withLength = true){
			if (_withLength){
				if (!hasSpaceFor(strLen+1)) return false;
				Fbuffer[FwritePos++] = strLen;
			}
			memcpy(&Fbuffer[FwritePos],_str,strLen);
			FwritePos += strLen;
			return true;
		}

		constexpr bool writeString(const char* _str, bool _withLength = true){ return writeString(_str,strlen(_str),_withLength); }
		constexpr bool writeString(TsubStringRef& _str, bool _withLength = true){ return writeString(_str.c_str(),_str.length(),_withLength); }

		constexpr void setReturnHeader(const t_prot_port _port=0){
			destiny(source());
			//source(_src);
			port(_port);
			func(func() & (~TvbusProtocoll::request));
		}

		constexpr void setHeader(const t_prot_addr _destiny, const t_prot_addr _source, const t_prot_port _port, const t_prot_func _func){
			ctrl(sizeof(t_prot_addr)-1);
			destiny(_destiny);
			source(_source);
			port(_port);
			func(_func);
		}

		constexpr void setHeader(const t_prot_addr _destiny, const t_prot_port _port, const t_prot_func _func){
			ctrl(sizeof(t_prot_addr)-1);
			destiny(_destiny);
			port(_port);
			func(_func);
		}

		constexpr void buildErrMsg(const t_prot_error _err, const t_prot_port _port){
			setHeader(source(),destiny(),_port,TvbusProtocoll::func_error);
			FwritePos = HEADER_SIZE;
			writeVal(_err);
			setSendPending();
		}

		constexpr TbufferPos length() { return FwritePos; }
};

typedef TvbusProtStream<
	dtypes::uint8, 
	dtypes::uint8,
	32
> Tvbus485ProtStream; 


/*
		static constexpr auto c_maxMsgLen = dt::c_maxMsgLen;

		static constexpr auto prot_request = 0x01;

		static constexpr auto dhcp_set = 0x02;
		static constexpr auto dhcp_req = 0x02 || prot_request;

		static constexpr auto prot_portOpen = 0x0A || prot_request;
		static constexpr auto prot_portClose = 0x0C || prot_request;
*/

#endif
