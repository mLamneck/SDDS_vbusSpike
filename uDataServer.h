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
		typedef typename TprotStream::t_path_entry t_path_entry; 
		typedef typename TprotStream::t_path_length t_path_length; 
		typedef typename TprotStream::t_prot_msgCnt TmsgCnt; 
		typedef typename TprotStream::BinLocator TbinLocator;
		typedef typename TprotStream::t_prot_type t_prot_type; 
		typedef vbusSpike::TtypeST<TprotStream> TypeST; 		
		typedef vbusSpike::Tconnection<Taddr,Tport> Tconnection;

		TypeST FtypeST;
	public:
		void init(Tthread* _thread){
			initEvent(FtypeST.Fevent,_thread);
			FtypeST.Fevent.setMsgRequest(true);
		}

		void startTypeThread(TypeST& _typeST, const Taddr _clientAddr, const Tport _clientPort, TbinLocator& _l){
			_typeST.FtypeClientAddr = _clientAddr;
			_typeST.FtypeClientPort = _clientPort;
			_typeST.FtypeCurrItem = _l.firstItem();
			_typeST.FtypeLastItem = _l.lastItem();
			_typeST.FtypeCurrIdx = _l.firstItemIdx();
			_typeST.Fevent.trigger();
		}

		bool writeEnums(TprotStream& _msg, TypeST& _typeST, TenumBase* en){
			_msg.writeVal(0x01);
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

		TexecRes handleTypeThread(TprotStream& _msg, TypeST& _typeST){
			Tdescr* d = _typeST.FtypeCurrItem;
			if (!d) return TexecRes::noMessage;

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
			else _msg.msgCnt(_msg.msgCnt() | 0x80);
			_msg.setSendPending();
			return TexecRes::sendMessage;
		}

		void handleTypeRequest(TprotStream& _ps,  TmenuHandle* _root){
			Tport clientPort;
			if (!_ps.readVal(clientPort)) return;

			//reply with error if server is busy			
			if (FtypeST.FtypeCurrItem) return  _ps.buildErrMsg(TvbusProtocoll::err_serverBusy,clientPort);

			//reply with error for wrong path
			TbinLocator l;
			if (!l.locate(_ps,_root)) return _ps.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);

			startTypeThread(FtypeST,_ps.source(),clientPort,l);
		}

		void handleLinkRequest(TprotStream& _ps,  TmenuHandle* _root, Tconnection* _conn){
			//reply with error for wrong path
			TbinLocator l;
			if (!l.locate(_ps,_root)) return _ps.buildErrMsg(TvbusProtocoll::err_invalidPath,_conn->clientPort());

			//toDo
			_conn->setupLink(FtypeST.Fevent.owner());
		}

		constexpr TexecRes execute(Tevent* _ev, TprotStream& _msg){
			if (_ev == &FtypeST.Fevent)
				return handleTypeThread(_msg,FtypeST);
			return TexecRes::noMessage;
		}

		ThandleMessageRes handleMessage(TprotStream& _ps, TmenuHandle* _root, Tconnection* _conn){
			switch (_ps.func()){
				case TvbusProtocoll::ds_link_req: 
					handleLinkRequest(_ps,_root,_conn);
					break;
				
				default : return ThandleMessageRes::notMyBusiness; 
			}
			return ThandleMessageRes::handled;
		}
		
		ThandleMessageRes handleMessage(TprotStream& _ps, TmenuHandle* _root){
			switch (_ps.func()){
				case TvbusProtocoll::ds_type_req: 
					handleTypeRequest(_ps,_root);
					break;

				case TvbusProtocoll::ds_fpdr_req:
					break;

				case TvbusProtocoll::ds_fpdw_req:
					break;
				
				default : return ThandleMessageRes::notMyBusiness; 
			}
			return ThandleMessageRes::handled;
		}

};

#endif
