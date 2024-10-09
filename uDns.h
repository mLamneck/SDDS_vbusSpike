#ifndef UDNS_H
#define UDNS_H

#include "uCommThread.h"
#include "uVbusProtocol.h"
#include "uObjectPool.h"

template <class TprotStream>
class Tdns{
	typedef TcommThreadDefs::ThandleMessageRes ThandleMessageRes;

	private:
		typedef typename TprotStream::t_prot_port Tport;
		typedef typename TprotStream::t_prot_addr Taddr;
		typedef typename TprotStream::t_path_entry t_path_entry; 
		typedef typename TprotStream::t_path_length t_path_length; 
	public:
		void handleDnsReq(TprotStream& _msg,  TmenuHandle* _root){
			Tport clientPort;
			if (!_msg.readVal(clientPort)) return;
			_msg.setReturnHeader(clientPort);

			//scan tree with bin path
			auto pos = _msg.readPos();
			TbinLocator<t_path_length, t_path_entry> l;
			if (!l.locate(_msg,_root)) return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
			_msg.setReadPos(pos);
			
			//move bin path to correct postion in output
			pos = _msg.writePos();
			t_path_length length = 0;
			_msg.readVal(length);
			_msg.writeVal(length);
			_msg.move(length*sizeof(t_path_entry));

			TsubStringRef pathStr = _msg.getText();
			Tokenizer<TsubStringRef> t(pathStr);
			if (!t.hasNext()) return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
			
			bool firstItemFound = false;
			switch(pathStr.curr()){
				case '-' :
					firstItemFound = true;
					_msg.setWritePos(_msg.writePos()-1*sizeof(t_path_entry));
					t.next();
					break;
				case '/' : t.next();
				default:
					_msg.setWritePos(_msg.writePos()-2*sizeof(t_path_entry));	//we need to first and nItems in answer
			}

			Tdescr* d = nullptr;
			TmenuHandle* parent = l.menu();
			if (!firstItemFound){
				do{
					auto token = t.next();
					t_path_entry idx = parent->find(token,d);
					if (!d) return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
					_msg.writeVal(idx);
					if (t.curr() == '-'){
						firstItemFound = true;
						break;
					}
					if (!d->isStruct()) break;
					parent = static_cast<Tstruct*>(d)->value();
				} while (t.hasNext());
			}
			
			//sub/subsub-var
			if (firstItemFound){
				auto token = t.next();
				t_path_entry idx = parent->find(token,d);
				if (!d) return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
				_msg.setReadPos(_msg.writePos()-sizeof(t_path_entry));
				t_path_entry firstIdx = 0;
				_msg.readVal(firstIdx);
				if (idx < firstIdx || t.hasNext()) 
					return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
				_msg.writePathEntry(idx-firstIdx+1);
			}

			//if not firstItemFound d must be assigned and rather be a struct or variable
			//sub
			//sub/
			else if (d->isStruct()){
				_msg.writePathEntry(0);
				_msg.writePathEntry(dtypes::high<t_path_entry>());
			}

			//sub/var
			else{
				if (t.hasNext()) 
					return _msg.buildErrMsg(TvbusProtocoll::err_invalidPath,clientPort);
				_msg.writePathEntry(1);
			}
			
			//finally set length of binary string and reply
			auto p = _msg.writePos();
			length = (p-(pos+sizeof(t_path_length)))/sizeof(t_path_entry);
			_msg.writeValToOfs(pos,length);
			_msg.setSendPending();
		}

		ThandleMessageRes handleMessage(TprotStream& _ps,  TmenuHandle* _root){
			switch (_ps.func()){
				case TvbusProtocoll::dns_req: 
					handleDnsReq(_ps,_root);
					return ThandleMessageRes::handled;
			}
			return ThandleMessageRes::notMyBusiness;
		}

};

#if MARKI_DEBUG_PLATFORM == 1
#include "test/uDns_test.h"
#endif

#endif
