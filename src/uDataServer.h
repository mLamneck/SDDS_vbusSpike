#ifndef UDATASERVER_H
#define UDATASERVER_H

#include "uCommThread.h"
#include "uVbusProtocol.h"
#include "uObjectPool.h"
#include "uConnections.h"

template <class TprotStream>
class TdataServer : public TmenuHandle, public TcommThread<TcommThreadDefs::ID_DATASERVER>{
	private:
		typedef typename TprotStream::t_prot_port Tport;
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef typename TprotStream::t_path_entry TpathEntry; 
		typedef typename TprotStream::t_path_length TpathLength; 
		typedef typename TprotStream::t_prot_msgCnt TmsgCnt; 
		typedef typename TprotStream::BinLocator TbinLocator;
		typedef typename TprotStream::t_prot_type t_prot_type; 
		typedef vbusSpike::TtypeST<TprotStream> TypeST; 		
		typedef vbusSpike::Tconnection<TprotStream> Tconnection;
	public:
		TypeST FtypeST;
		void init(Tthread* _thread){
			initEvent(FtypeST.Fevent,_thread);
			setMsgRequest(&FtypeST.Fevent,true);
			//FtypeST.Fevent.setMsgRequest(true);
		}

		//get owning thread of TdataServer for late initialization of events i.e. in linkRequest
		Tthread* owner(){ return FtypeST.Fevent.owner(); }

		void startTypeThread(TypeST& _typeST, const Taddr _clientAddr, const Tport _clientPort, TbinLocator& _l){
			_typeST.FtypeClientAddr = _clientAddr;
			_typeST.FtypeClientPort = _clientPort;
			_typeST.FtypeCurrItem = _l.firstItem();
			_typeST.FtypeLastItem = _l.lastItem();
			_typeST.FtypeCurrIdx = _l.firstItemIdx();
			_typeST.FtypeMsgCnt = 0;
			trigger(_typeST.Fevent);
		}

		/*		
		20 80 
		09 00	firstItemIdx 
		01 00 05 65 6E 75 6D 32	//type/opt/name 
		00	//typeExt reserved 
		16	//bufferSize 
		00	//idx 
		02 6F 6E 03 6F 66 66 04 69 64 6C 65 04 68 65 61 74 04 63 6F 6F 6C 
		*/
		bool writeEnums(TprotStream& _msg, TypeST& _typeST, TenumBase* en){
			_msg.writeByte(0x01);			//typeId of enum = 0x01 on bus protocol
			_msg.writeByte(en->option());
			if (_typeST.FtypeEnumIdx == 0){
				_msg.writeString(en->name());
			}
			else{
				_msg.writeByte(0);
			}

			//int enBufferSize = en->enumBufferSize()-1;	//don't count \0 of last string for size
			//const char* pEnumBuf = en->enumBuffer();
			auto enumInfo = en->enumInfo();
			int enBufferSize = enumInfo.bufferSize-1;	//don't count \0 of last string for size
			const char* pEnumBuf = enumInfo.buffer;
			_msg.writeByte(0);		//typExt (future reserved)
			_msg.writeByte(enBufferSize);
			_msg.writeByte(_typeST.FtypeEnumIdx);

			while (_typeST.FtypeEnumIdx < enBufferSize){
				char c = pEnumBuf[_typeST.FtypeEnumIdx];
				if (c=='\0'){
					int enLength = strlen(pEnumBuf + _typeST.FtypeEnumIdx + 1);
					if (!_msg.writeByte(enLength)) return false;
				}
				else if (!_msg.writeByte(c)) return false;
				_typeST.FtypeEnumIdx++;
			}

			_typeST.FtypeEnumIdx = 0;
			return true;
		}

		void handleTypeThread(TprotStream& _msg, TypeST& _typeST){
			Tdescr* d = _typeST.FtypeCurrItem;
			if (!d) return;

			_msg.setHeader(_typeST.FtypeClientAddr,_typeST.FtypeClientPort,TvbusProtocoll::ds_type);
			_msg.writeVal(_typeST.FtypeMsgCnt++);
			_msg.writeVal(_typeST.FtypeCurrIdx);
			bool typeComplete = true;
			auto type = d->type(); 
			switch(type){
				case sdds::Ttype::ENUM: 
					typeComplete = writeEnums(_msg,_typeST,static_cast<TenumBase*>(d));
					break;
				case sdds::Ttype::STRING: 
					type = sdds::Ttype::STRUCT;
				default:
					_msg.writeVal(type);
					_msg.writeVal(d->option());
					_msg.writeString(d->name());
			}				
			if (typeComplete){
				if (_typeST.FtypeCurrItem != _typeST.FtypeLastItem){
					_typeST.FtypeCurrItem = d->next();
					_typeST.FtypeCurrIdx++;
				}
				else
					_typeST.FtypeCurrItem = nullptr;
			}
			if(_typeST.FtypeCurrItem) 
				_typeST.Fevent.setTimeEvent(10);
			else _msg.msgCnt(_msg.msgCnt() | TvbusProtocoll::fLAST_MSG);

			_msg.setSendPending();
		}

		/**
		 * @brief write a Ttime value into binary stream 
		 * 
		 * @param _msg 
		 * @param _time 
		 * @return true 
		 * @return false 
		 */
		bool writeValueToStream(TprotStream& _msg, const Ttime* _time){
			if (_msg.spaceAvailableForWrite() < 6) return false;
			dtypes::TdateTime t = *_time;
			//convert internal time in (s and us since epoch) into protocoll time (unit 50us since epoch)
			dtypes::int64 protTime = t.tv_sec * 20000 + t.tv_usec/50;
			_msg.writeBytesNoCheck(&protTime,6); 
			return true;
		}
		
		bool writeValueToStruct(TprotStream& _msg, Ttime* _time){
			dtypes::uint64 ticks = 0;
			if (!_msg.readBytes(&ticks,6)) return false;
			dtypes::TdateTime t;
			t.tv_sec = ticks/20000;
			t.tv_usec = ticks%20000;
			memcpy(_time->pValue(),&t,sizeof(t));
			return true;
		}
		
		bool writeValueToStream(TprotStream& _msg, Tdescr* _val){
			auto typeId = _val->typeId();

			if (typeId >= sdds::typeIds::first_compose_type)
			return _msg.writeWord(1);

			if (typeId == sdds::typeIds::time)
				return writeValueToStream(_msg,static_cast<Ttime*>(_val));
			
			return _msg.writeBytes(_val->pValue(),_val->valSize(),true);
		}


		bool writeValueToStruct(TprotStream& _msg, Tdescr* _val){
			auto typeId = _val->typeId();

			/** skip struct and arrays */
			if (typeId >= sdds::typeIds::first_compose_type)
				return _msg.readOfs(2);

			if (typeId == sdds::typeIds::time){
				if (_val->readonly())
					return _msg.readOfs(6);
				return writeValueToStruct(_msg,static_cast<Ttime*>(_val));
			}

			auto valSize = _val->valSize();
			if (_val->readonly())
				return _msg.readOfs(valSize);
			return _msg.readBytes(_val->pValue(),valSize);
		}

		void writeValuesToStruct(TprotStream& _msg, TmenuHandle* s, int _firstItemIdx, Tport _port = 0){
			int nItemsWritten = 0;
			auto curr = s->get(_firstItemIdx);
			while (curr){
				if (!writeValueToStruct(_msg,curr)) break;
				curr->callbacks()->emit();
				nItemsWritten++;
				curr = curr->next();
			}
			s->events()->signal(_firstItemIdx,nItemsWritten,_port);
		}

		/**
		 * @brief handle message with func = ds_type_req
		 * 
		 * @param _msg streaming object
		 * @param _root ptr to the root object
		 */
		void handleTypeRequest(TprotStream& _msg,  TmenuHandle* _root){
			Tport clientPort;
			if (!_msg.readVal(clientPort)) return;

			//reply with error if server is busy			
			if (FtypeST.FtypeCurrItem) return  _msg.buildErrMsg(TvbusProtocoll::err_serverBusy,clientPort);

			TbinLocator l;
			switch (l.locate(_msg,_root))
			{
				case TbinLocator::Tresult::isStruct:
					startTypeThread(FtypeST,_msg.source(),clientPort,l);
					break;
				
				case TbinLocator::Tresult::isArray: {
					_msg.setReturnHeader(clientPort);
					_msg.writeMsgCnt(TvbusProtocoll::fLAST_MSG);
					_msg.writePathEntry(0);
					Tdescr* d = l.result(); 
					_msg.writeVal(d->typeId());
					_msg.writeVal(d->option());
					_msg.writeString(d->name());
					_msg.setSendPending();
					break;
				}
				
				//reply with error in case of failure
				default:
					_msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
			}
		}

		/**
		 * @brief handle message with func = ds_link_req
		 * 
		 * @param _msg streaming object
		 * @param _root ptr to the root object
		 * @param _conn connection associated with the server port in the message 
		 */
		void handleLinkRequest(TprotStream& _msg,  TmenuHandle* _root, Tconnection* _conn){
			//reply with error for wrong path
			TbinLocator l;
			if (l.locate(_msg,_root) == TbinLocator::Tresult::isInvalid) 
				return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,_conn->clientPort());

			dtypes::uint8 linkTime = 0;
			if (!_msg.readVal(linkTime)) return _msg.buildErrMsg(TvbusProtocoll::err_invalidLinkTime,_conn->clientPort());

			/**
			 * toDo:
			 * implement linkTime
			 */
			if (linkTime != 1) return _msg.buildErrMsg(TvbusProtocoll::err_invalidLinkTime,_conn->clientPort());

 			_conn->setupLink(owner(),_msg.port(),l);
			setMsgRequest(_conn->FobjEvent.event(),true);
			_conn->FobjEvent.signal(l.firstItemIdx(),l.lastItemIdx());
			_conn->FdataThread.resetBusy();
		}

		/**
		 * @brief handle message with funf = ds_link and write data to structure associated with the given port number
		 * 
		 * @param _msg streaming object
		 * @param _root ptr to the root object
		 * @param _conn connection associated with the server port in the message 
		 */
		void handleLinkData(TprotStream& _msg,  TmenuHandle* _root, Tconnection* _conn){
			TpathEntry firstIdx = 0;
			TmsgCnt msgCnt = 0;
			if (!_msg.readVal(msgCnt)) return;
			if (!_msg.readVal(firstIdx)) return;
			auto observedObj = _conn->FobjEvent.observedObj();
			if (observedObj->isStruct())
				writeValuesToStruct(_msg,static_cast<Tstruct*>(observedObj)->value(),firstIdx,_msg.port());
			else{
				arrayToDo();
			}
		}

		/**
		 * @brief handle message with func = ds_fpdw_req and write data to structure associated with the given path
		 * 
		 * @param _msg streaming object
		 * @param _root ptr to the root object
		 */
		void handleFPDW(TprotStream& _msg, TmenuHandle* _root){
			Tport clientPort;
			if (!_msg.readVal(clientPort)) return;

			//reply with error for wrong path
			TbinLocator l;
			switch (l.locate(_msg,_root)){
				case TbinLocator::Tresult::isStruct:
					writeValuesToStruct(_msg,l.menuHandle(),l.firstItemIdx());
					break;
				
				case TbinLocator::Tresult::isArray:{
					arrayToDo();
					/**
					 * arrayToDo:
					 * 
					 * at the moment we only partially handle strings. We have to implement
					 * generic array interface first in uTypedef.h in order to do it right.
					 */
					Tstring* d = static_cast<Tstring*>(l.result());
					if (d->readonly()) return;
					TpathEntry arrayLen;
					if (!_msg.readVal(arrayLen)) return;
					if (arrayLen > 0 && _msg.bytesAvailableForRead() < 1) return;
					if (1==1){
						//this doesn't work for long strings if we get multiple messages
						auto str = _msg.readString();
						d->setValue(str);
					} else{
						//this is not tested
						d->setValue(l.firstItemIdx(),_msg);
					}
					break;
				}
				
				default:
					return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
			}

			_msg.setReturnHeader(clientPort);
			_msg.setSendPending();
		}

		/**
		 * @brief handle a message with serverPort != 0
		 * 
		 * @param _msg streaming object
		 * @param _root ptr to the root object
		 * @param _conn connection associated with the server port in the message 
		 * @return ThandleMessageRes 
		 */
		ThandleMessageRes handleMessage(TprotStream& _msg, TmenuHandle* _root, Tconnection* _conn){
			switch (_msg.func()){
				case TvbusProtocoll::ds_link: 
					handleLinkData(_msg,_root,_conn);
					break;

				case TvbusProtocoll::ds_link_req: 
					handleLinkRequest(_msg,_root,_conn);
					break;
				
				default : return ThandleMessageRes::notMyBusiness; 
			}
			return ThandleMessageRes::handled;
		}
		
		/**
		 * @brief handle a message with port = 0 
		 * 
		 * @param _msg streaming object
		 * @param _root ptr to the root object
		 * @return ThandleMessageRes 
		 */
		ThandleMessageRes handleMessage(TprotStream& _msg, TmenuHandle* _root){
			switch (_msg.func()){
				case TvbusProtocoll::ds_type_req: 
					handleTypeRequest(_msg,_root);
					break;

				case TvbusProtocoll::ds_fpdw_req:
					handleFPDW(_msg,_root);
					break;

				case TvbusProtocoll::ds_fpdr_req:
					break;
				
				default : return ThandleMessageRes::notMyBusiness; 
			}
			return ThandleMessageRes::handled;
		}

		void dataThreadNextRun(TprotStream& _msg, Tconnection* _conn, TpathEntry _currIdx, TpathEntry _lastIdx, dtypes::uint8 _msgCnt){
			if (_msgCnt == 0 && _conn->FdataThread.busy()) return;
			_conn->FdataThread.FmsgCnt = _msgCnt+1;
			_conn->FdataThread.FcurrIdx = _currIdx;
			_conn->FdataThread.FlastIdx = _lastIdx;
			setMsgRequest(&_conn->FdataThread.Fevent,true);
			_conn->FdataThread.Fevent.setTimeEvent(10);
			_msg.msgCnt(_msgCnt);
			_msg.setSendPending();
		}

		/**
		 * @brief write binary data to the given stream starting form currIdx to lastIdx until there's no
		 * more space or the job is done
		 * 
		 * @param _msg 
		 * @param _conn 
		 * @param currIdx 
		 * @param lastIdx 
		 * @param _msgCnt 
		 * @param _struct
		 * @return true if all data is written
		 * @return false if there's some data left and we have to run it again
		 */
		bool builLinkDataMsgStruct(TprotStream& _msg, Tconnection* _conn, TpathEntry currIdx, TpathEntry lastIdx, dtypes::uint8 _msgCnt, TmenuHandle* _struct){
			auto mh = _struct;
			auto curr = mh->get(currIdx);
			if (!curr) return true;
			
			do{
				if (!writeValueToStream(_msg,curr)){
					dataThreadNextRun(_msg,_conn,currIdx,lastIdx,_msgCnt);
					return false; //don't stop dataThread
				}
				if (currIdx++ >= lastIdx) break;
				curr = curr->next();
			} while(curr);

			_msg.setSendPending();
			return true;
		}
		
		/**
		 * @brief write binary data to the given stream starting form currIdx to lastIdx until there's no
		 * more space or the job is done
		 * 
		 * @param _msg 
		 * @param _conn 
		 * @param currIdx 
		 * @param lastIdx 
		 * @param _msgCnt 
		 * @return true if all data is written
		 * @return false if there's some data left and we have to run it again
		 */
		bool builLinkDataMsg(TprotStream& _msg, Tconnection* _conn, TpathEntry _currIdx, TpathEntry _lastIdx, dtypes::uint8 _msgCnt){
			_msg.setHeader(_conn->clientAddr(),_conn->clientPort(),TvbusProtocoll::ds_link);
			_msg.writeMsgCnt(_msgCnt | TvbusProtocoll::fLAST_MSG);				
			_msg.writeVal(_currIdx);							//itemIdx (ofs in array buffer)				

			auto observedObj = _conn->FobjEvent.observedObj();
			if (observedObj->isStruct()) {
				return builLinkDataMsgStruct(_msg,_conn,_currIdx,_lastIdx,_msgCnt,static_cast<Tstruct*>(observedObj)->value());
			}
			else if (observedObj->type() == sdds::Ttype::STRING) {
				Tstring* str = static_cast<Tstring*>(observedObj);			

				_msg.ctrl(TvbusProtocoll::ctrl_arrayFlag);
				//special treatment for strings due to nullterminated strings in C we have to adjust things
				auto strLen = str->length();
				auto arrayLen = strLen;
				auto arrayPtr = str->c_str();						//this is an invalid ptr but will be corrected by adding currIdx != 0
				_msg.writePathEntry(arrayLen+1);					//arraySize (including length of str)
				int bufElementIdx;
				if (_currIdx == 0){
					_msg.writeByte(strLen);							//this is the first byte of the array of char
					if (strLen == 0){
						_msg.setSendPending();
						return true;
					}
					_currIdx=1;
					bufElementIdx = 0;
				} else bufElementIdx = _currIdx-1;

				/**
				 * from now on this code is valid for all array types
				 * - arrayPtr points to the start of the array. 
				 * - bufElementIdx is the idx of the next element to be transmitted in the array pointed to by arrayPtr
				 * - currIdx is the idx of the element pointed to by arrayPtr in terms of protocoll (usually the same as bufElementIdx but not for strings)
				 * - arrayLen is the number of elements available in the array
				 */	

				/**
				 * this can only happen in the following scenario:
				 * 		the size has beed decreased since last run of dataThread i.e. a dataThread has been started
				 * 		for the string "012345678901234567890123456789" and we have transmitted the first bytes "01234567890123456789012".
				 * 		After 10ms we come here again. In the meanwhile the string was set to "xyz". This has triggered a message with "xyz"
				 * 		and we can just stop the thread here without sending a message.
				 * 		toDo: what happens if the length has been decreased but we still have to send multiple messages?
				 */
				if (bufElementIdx >= arrayLen) return true;			//no message and kill dataThread
				
				auto valSize = observedObj->valSize();
				auto nElmentsToTransmit = arrayLen-bufElementIdx;
				arrayPtr += bufElementIdx * valSize;
				int n = _msg.spaceAvailableForWrite()/valSize;		//how many elements can fit into buffer
				if (nElmentsToTransmit > n){
					dataThreadNextRun(_msg,_conn,_currIdx+n,_lastIdx,_msgCnt);
					_msg.writeBytesNoCheck(arrayPtr,n*valSize);
					return false;	//don't stop dataThread
				} 
				_msg.writeBytesNoCheck(arrayPtr,nElmentsToTransmit*valSize);
				_msg.setSendPending();
				return true;
			}
			return true;
		}

		/**
		 * @brief execute thread for port 0
		 * 
		 * @param _ev 
		 * @param _msg 
		 * @return constexpr TexecRes 
		 */
		constexpr void execute(Tevent* _ev, TprotStream& _msg){
			if (_ev == &FtypeST.Fevent)
				handleTypeThread(_msg,FtypeST);
		}

		/**
		 * @brief execute thread with associated connection
		 * 
		 * @param _ev 
		 * @param _msg 
		 * @param _conn 
		 * @return constexpr TexecRes 
		 */
		constexpr void execute(Tevent* _ev, TprotStream& _msg, Tconnection* _conn){
			if (_conn->isDataEvent(_ev)){
				auto ev = TobjectEvent::retrieve(_ev);
				builLinkDataMsg(_msg,_conn,ev->first(),ev->last(),0);
			}

			else if (_ev == &_conn->FdataThread.Fevent){
				if (builLinkDataMsg(_msg,_conn,_conn->FdataThread.FcurrIdx,_conn->FdataThread.FlastIdx,_conn->FdataThread.FmsgCnt))
					_conn->FdataThread.resetBusy();
			}
		}
};

#if MARKI_DEBUG_PLATFORM == 1
	#include "../test/uDataServer_test.h"
#endif

#endif
