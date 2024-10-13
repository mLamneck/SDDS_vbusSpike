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
		TypeST FtypeST;
	public:
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
			trigger(_typeST.Fevent);
		}

		bool writeEnums(TprotStream& _msg, TypeST& _typeST, TenumBase* en){
			_msg.writeVal(0x01);			//typeId of enum = 0x01 on bus protocol
			_msg.writeVal(en->option());
			if (_typeST.FtypeEnumIdx == 0 && _typeST.FtypeEnumPos == 0){
				_msg.writeString(en->name());
			}
			else{
				_msg.writeByte(0);
			}
			_msg.writeByte(0);		//typExt (future reserved)

			//remind position of nBytesWritten and jmp over
			int nBytesPos = _msg.writePos();
			_msg.writeByte(0);

			int bytesWritten = 0;
			bool complete = true;
			for (int i = _typeST.FtypeEnumIdx; i<en->enumCnt(); i++){
				const char* enStr = en->getEnum(i);
				int len = strlen(enStr);
				if (_typeST.FtypeEnumPos == 0){
					if (!_msg.writeByte(len)) {
						complete = false;
						break;
					}
					_typeST.FtypeEnumPos++;
					bytesWritten++;
				}
				enStr += _typeST.FtypeEnumPos;
				int n = _msg.writeBytes(enStr,len);
				if (n < len){
					_typeST.FtypeEnumPos += n;
					bytesWritten += n;
					complete = false;
					break;
				}
				_typeST.FtypeEnumPos = 0;
				_typeST.FtypeEnumIdx++;
			}

			_msg.writeByteToOfs(nBytesPos,bytesWritten);
			return complete;
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

		bool writeValueToStream(TprotStream& _msg, Tdescr* _val){
			auto typeId = _val->typeId();

			if (typeId >= sdds::typeIds::first_compose_type)
			return _msg.writeWord(1);

			if (typeId == sdds::typeIds::time)
				return writeValueToStream(_msg,static_cast<Ttime*>(_val));
			
			return _msg.writeBytes(_val->pValue(),_val->valSize(),true);
		}

		bool writeValueToStruct(TprotStream& _msg, Tdescr* _val){
			auto valSize = _val->valSize();
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

			//reply with error for wrong path
			TbinLocator l;
			if (!l.locate(_msg,_root)) return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
			startTypeThread(FtypeST,_msg.source(),clientPort,l);
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
			if (!l.locate(_msg,_root)) return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,_conn->clientPort());

			/*
			_conn->setupLink(owner(),l.menu(),_ps.port());
			l.menu()->events()->push_first(&_conn->FobjEvent);
			*/
			_conn->setupLink(owner(),_msg.port(),l);
			setMsgRequest(_conn->FobjEvent.event(),true);
			_conn->FobjEvent.signal(l.firstItemIdx(),l.lastItemIdx());
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
			if (!_msg.readVal(firstIdx)) return;
			writeValuesToStruct(_msg,_conn->FobjEvent.Fstruct,firstIdx,_msg.port());
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
			if (!l.locate(_msg,_root)) return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
			
			//toDo: check for array
			writeValuesToStruct(_msg,l.menu(),l.firstItemIdx());
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
		bool builLinkDataMsg(TprotStream& _msg, Tconnection* _conn, TpathEntry currIdx, TpathEntry lastIdx, dtypes::uint8 _msgCnt){
			auto curr = _conn->FobjEvent.Fstruct->get(currIdx);
			if (!curr) return true;
			
			_msg.setHeader(_conn->clientAddr(),_conn->clientPort(),TvbusProtocoll::ds_link);
			_msg.writeMsgCnt(_msgCnt | TvbusProtocoll::fLAST_MSG);
			_msg.writeVal(currIdx);

			do{
				if (!writeValueToStream(_msg,curr)){
					if (_msgCnt == 0 && _conn->FdataThread.busy())
						return false;
					_conn->FdataThread.FmsgCnt = _msgCnt+1;
					_conn->FdataThread.FcurrIdx = currIdx;
					_conn->FdataThread.FlastIdx = lastIdx;
					setMsgRequest(&_conn->FdataThread.Fevent,true);
					_conn->FdataThread.Fevent.setTimeEvent(10);
					_msg.msgCnt(_msgCnt);
					_msg.setSendPending();
					return false;
				}
				if (currIdx++ >= lastIdx) break;
				curr = curr->next();
			} while(curr);

			_msg.setSendPending();
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

#endif
