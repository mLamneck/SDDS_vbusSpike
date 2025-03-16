#ifndef UVBUSPROTOCOL_H
#define UVBUSPROTOCOL_H

#include "uPlatform.h"
#include <uMemoryUtils.h>
#include "uTypedef.h"

class TvbusProtocoll{
	public:
		/**  */
		static constexpr dtypes::uint8 fLAST_MSG = 0x80;

		/** fields in ctrl byte */
		static constexpr dtypes::uint8 ctrl_addrSizeMask = 0x03;
		static constexpr dtypes::uint8 ctrl_arrayFlag = 0x20;

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

		//Bootloader Functions
		static constexpr auto bl_notification 		= 0x60;
		static constexpr auto bl_enterProgMode 		= 0x62;
		static constexpr auto bl_enterProgModeReq 	= bl_enterProgMode | request;
		static constexpr auto bl_read 				= 0x64;
		static constexpr auto bl_read_req			= bl_read | request;
		static constexpr auto bl_write 				= 0x66;
		static constexpr auto bl_write_req			= bl_write | request;
		static constexpr auto bl_error 				= 0x68;

		//bootloader error codes
		static constexpr auto bl_err_invAddr 		= 1;
		static constexpr auto bl_err_invCS 			= 2;
		static constexpr auto bl_err_invPacketSize	= 3;
		static constexpr auto bl_err_invProgMode	= 4;
		static constexpr auto bl_err_progFailed		= 5;

		/** error codes */

		static constexpr auto err_serverBusy 		= 0xFF;
		static constexpr auto err_invalidPort 		= 0xFE;
		static constexpr auto err_invalidPath 		= 0xFD;

		static constexpr auto err_invalidLinkTime	= 0xF0;

		static constexpr auto err_noMorePorts 		= 0xEB;


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


template <
    typename _t_prot_addr,
    typename _t_prot_port,
	int max_msg_length
>
class TvbusProtStream : public sdds::memUtils::TbufferStream{
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
			sdds::memUtils::TbufferStream::init(_buffer,c_max_msg_length);
			Fbuffer = static_cast<uint8*>(_buffer);
			FreadPos = HEADER_SIZE;
			FwritePos = HEADER_SIZE;
			FsendPending = false;
		}

		void init(uint8* _buffer, TmessageSize _size){
			init(_buffer);
			FbytesAvailableForRead = _size;
		}

		//constexpr void ctrl(const t_prot_ctrl _val) { writeValToOfs<OFS_CTRL>(_val); }
		constexpr void ctrl(const t_prot_ctrl _val) { writeValToOfs<OFS_CTRL>(_val | (sizeof(t_prot_addr)-1) ); }
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
			ctrl(0);
			destiny(_destiny);
			source(_source);
			port(_port);
			func(_func);
		}

		constexpr void setHeader(const t_prot_addr _destiny, const t_prot_port _port, const t_prot_func _func){
			ctrl(0);
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

/**
 * toDo: make msgSize dependend on Tuart somehow. Now it's independend and we get a problem
 * If we change the buffer size in Tuart to a lower number compared to the 32 here.
 */
typedef TvbusProtStream<
	dtypes::uint8, 
	dtypes::uint8,
	32
> Tvbus485ProtStream;

#endif
