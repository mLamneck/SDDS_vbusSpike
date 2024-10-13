#include "uTypedef.h"
#include "uMultask.h"
#include "uUart.h"
#include "uVbusProtocol.h"
#include "uCommThread.h"
#include "uDhcp.h"
#include "uConnections.h"
#include "uDns.h"
#include "uDataServer.h"

template <class _TprotStream, class _Tstream>
class TvbusSpikeBase : public Tthread{
	public:
		typedef _TprotStream TprotStream;

		typedef Tdhcp<TprotStream> Dhcp;
		typedef Tconnections<TprotStream> Connections;
		typedef Tdns<TprotStream> Dns;
		typedef TdataServer<TprotStream> DataServer;

		typedef TcommThreadDefs::ThandleMessageRes ThandleMessageRes; 
		typedef typename _Tstream::TmessageBufferTX TtxBuffer;
		typedef typename TprotStream::t_prot_cs t_prot_cs; 
		typedef typename TprotStream::t_prot_port Tport;
		constexpr static int nCOMM_THREADS = 1;

	protected:
		//handshake events with _Tstream
		Tevent FevRx;
		Tevent FevTxIdle;
		_Tstream* Fstream = nullptr;

		TmenuHandle* Froot = nullptr;
		t_prot_cs Fcs = 0;
		
		TprotStream Fps;

		Dhcp Fdhcp;
		Connections Fconnections;
		Dns Fdns;
		DataServer FdataServer;

	public:
		/*
		 * Event priorities:
		 * 	- events producing messages 0
		 *  - events for reading messages = 1
		 *  - events for txIdle = 2
		 * 
		 * -> if stream is busy with a message, ignore all producers
		 * -> if a message needs to be answered and stream is busy, ignore incoming messages as well
		 * -> on txIdle handle pending events and if there are no more reset task priority to 0
		 */
		TvbusSpikeBase(TmenuHandle& _root, _Tstream* _stream)
				: FevRx(this,1)
				, FevTxIdle(this,2)		//higher priority for txIdle event
				, Fstream(_stream)
				, Froot(&_root)
			{
				Fstream->begin(&FevRx,&FevTxIdle);
				Fdhcp.init(this);
				Fconnections.init(this);
				FdataServer.init(this);
			}

	protected:
		//TtxBuffer FtxBuffer;
		
		bool appliedToNet = false;
		bool FtxBusy = false;
		bool txBusy() { return FtxBusy; } void setTxBusy(bool _val) { FtxBusy = _val; }
		virtual void sendMessage(dtypes::uint8* _buf, int size) = 0;
		void sendMessage() {
			Fps.source(Fdhcp.myAddr());
			sendMessage(Fps.buffer(),Fps.length());
		};
		void checkSendMessage(){ if (Fps.sendPending()) sendMessage(); }

		void handleMessage(){
			auto port = Fps.port();
			if (port == 0){
				if (Fdhcp.handleMessage(Fps) == ThandleMessageRes::handled) return;
				if (Fconnections.handleMessage(Fps) == ThandleMessageRes::handled) return;
				if (Fdns.handleMessage(Fps,Froot) == ThandleMessageRes::handled) return;
				if (FdataServer.handleMessage(Fps,Froot) == ThandleMessageRes::handled) return;

			} else{
				auto conn = Fconnections.getConnection(port,Fps.source());
				if (!conn) return Fps.buildErrMsg(TvbusProtocoll::err_invalidPort,port);

				if (FdataServer.handleMessage(Fps,Froot,conn) == ThandleMessageRes::handled) return;
				if (Fconnections.handleMessage(Fps,conn) == ThandleMessageRes::handled) return;
			};
		}
		
		virtual bool readMessage(Tuart::TmessageBufferRX* _msg){
			Fps.init(&_msg->data[0],_msg->length);
			handleMessage();			
			if (Fps.sendPending()){
				if (txBusy()){
					_msg->containsAnswer = true;
					_msg->length = Fps.length();
					setPriority(2,false);	//don't handle messages until we are idle again
					return false;
				}
				sendMessage();
			}
			return true;
		}

		/**
		 * @brief readMessages
		 * Priority management: If a message is given to the stream object the priority is set to 1. 
		 * If there's an answer to be send while txBusy, it is cached into the received message buffer 
		 * and the priority is raised to 2.
		 * 
		 * @return true if all messages have been read and there is no answer to be send
		 * @return false if there is a cached message that needs to be send first
		 */
		bool readMessages(){
			auto msg = Fstream->getMessage();
			if (!msg) return true;

			if (msg->containsAnswer){
				if (txBusy()) return false;
				sendMessage(&msg->data[0],msg->length);
				Fstream->ackMessage(msg);
			}
			
			msg = Fstream->getMessage();
			while (msg){
				if (!readMessage(msg)) return false;
				Fstream->ackMessage(msg);
				msg = Fstream->getMessage();
			}
			return true;
		}

		template <class Thandler>
		constexpr void handleEvMsgRequest(Thandler& _handler, Tevent* _ev){
			if (_handler.execute(_ev,Fps) == Thandler::TexecRes::sendMessage)
				sendMessage();
		}

		/**
		 * @brief this function is called from execute or handleTxIdle
		 * 
		 * @param _ev can be FevRx or events from sub threads
		 */
		void handleEvent(Tevent* _ev){
			if (_ev == &FevRx){
				readMessages();
				return;
			}
			
			auto msgRequest = TcommThreadDefs::msgRequest(_ev);
			auto id = TcommThreadDefs::eventCommId(_ev);
			if (msgRequest){
				/*
				 * this should never happen
				 */
				if (txBusy()){
					FtaskQ.push_first(_ev);	//handle event on next txIdle
					return;
				}
				auto buf = Fstream->getTxBuffer();
				Fps.init(buf->data);

				/**
				 * Deal with events associated with a connection:
				 * 
				 * for now Dataserver is the only one that deals with connections so we don't
				 * have to check for the ID to match
				 */
				auto portNum = TcommThreadDefs::eventPort(_ev);
				if (portNum > 0){
					auto conn = Fconnections.getConnection(portNum);
					if (!conn) return;
					FdataServer.execute(_ev,Fps,conn);
					return checkSendMessage();
				}


				else{
					switch (id)
					{
						case DataServer::ID: return handleEvMsgRequest(FdataServer,_ev);
						case Connections::ID: return handleEvMsgRequest(Fconnections,_ev);
						case Dhcp::ID: return handleEvMsgRequest(Fdhcp,_ev);
					}
				}
			}
			else{
				switch (id)
				{
					case Dhcp::ID: return Fdhcp.execute(_ev);
				}
			}
		}

		void handleTxIdle(){
			setTxBusy(false);
		
			//get out if we are busy again
			if (!readMessages()) return;

			do{
				auto ev = FtaskQ.pop();
				if (ev){
					handleEvent(ev);
					//handleCommthreadEvent(static_cast<TcommEvent*>(ev));
				}
				else{
					setPriority(0,false);
					break;
				}
			} while(!txBusy());
		}

		void virtual exec_applyToNet(Tevent* _ev) = 0;

#if MARKI_DEBUG_PLATFORM == 1
		void parseStr(const char* _in){
			auto pEnd = _in + strlen(_in);
			while (_in < pEnd) {
				_in += 2;
				dtypes::string byteString(_in,_in+2);
				uint8_t b = static_cast<uint8_t>(
					stoi(byteString, nullptr, 16));
				Fps.writeByte(b);
				_in+=3;
			}
		}
#endif

		void execute(Tevent* _ev){
			if (_ev == &FevTxIdle){
				return handleTxIdle();
			}
			else if (isTaskEvent(_ev)){
#if MARKI_DEBUG_PLATFORM == 1
				static typename _Tstream::TmessageBufferRX FtxBuffer;
				if (1==1){
					if (1==1){
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(0xFF,0xFF,0,TvbusProtocoll::dhcp_req);
						parseStr("0x7E 0xC1 0x0F 0x4D 0x6F 0x74 0x6F 0x34 0x38 0x35 0x47 0x2D 0x30 0x30 0x31 0x32 0x35 0x39");               
						FtxBuffer.length = Fps.length();
						readMessage(&FtxBuffer);
					}

					Fps.init(&FtxBuffer.data[0]);
					Fps.setHeader(0xFF,0xCC,0,TvbusProtocoll::dhcp_set);
					Fps.writeByte(6);
					Fps.writeString("NUCLEO1");
					Fps.init(&FtxBuffer.data[0],Fps.length());
					Fdhcp.handleMessage(Fps);

					if (1 == 2){
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(0xFF,0xCC,0,TvbusProtocoll::ds_type_req);
						Fps.writeByte(13);	//client port
						Fps.writeByte(3);
						Fps.writeByte(1);
						Fps.writeByte(0);
						Fps.writeByte(0xFF);
						auto len = Fps.length();
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						Fps.init(&FtxBuffer.data[0],len);
						handleMessage();						
					}

					for (int i=0; i<4; i++){
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,0,TvbusProtocoll::port_open);
						Fps.writeByte(1+i);//client port
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
					}

					for (int i=0; i<4; i++){
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,0,TvbusProtocoll::port_open);
						Fps.writeByte(1+i);//client port
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
					}

					Fps.init(&FtxBuffer.data[0]);
					Fps.setHeader(6,5,10,TvbusProtocoll::ds_link_req);
					Fps.writeByte(2);
					Fps.writeByte(0);
					Fps.writeByte(0xFF);
					Fps.writeByte(0);//link time
					Fps.init(&FtxBuffer.data[0],Fps.length());
					handleMessage();

					//close it
					if (1==2){
						for (int i=0; i<=1; i++){
							Fps.init(&FtxBuffer.data[0]);
							Fps.setHeader(2,1,Connections::FIRST_PORT+i,TvbusProtocoll::port_close);
							Fps.writeByte(2-i);//client port
							Fps.init(&FtxBuffer.data[0],Fps.length());
							handleMessage();
						}
					}

					//Fstream->debugReadMessage(&FtxBuffer.data[0],Fps.length());
					//Fdhcp.handleMessage(FpsRead);
				}
#endif
			}
			else{
				handleEvent(_ev);
			}
		}
};

class TvbusSpike485 : public TvbusSpikeBase<Tvbus485ProtStream,Tuart>{
	using TvbusSpikeBase::TvbusSpikeBase;

	public:
		TvbusSpike485(TmenuHandle& _root, Tuart* _stream) 
			: TvbusSpikeBase<Tvbus485ProtStream,Tuart>(_root,_stream)
		{
		}

	private:

	protected:
		void sendMessage(dtypes::uint8* _buf, int _size) override{
			//disable all events but TxIdle and timeout
			setPriority(1,false);
			setTxBusy(true);
			//setTimeEvent(3000);
			Fstream->write(_buf,_size);
		}

		void exec_applyToNet(Tevent* _ev) override {
		}
};


#if MARKI_DEBUG_PLATFORM == 1
class TvbusStimulSpike : public TvbusSpike485{
	public:
		using TvbusSpike485::TvbusSpike485;

		template<class _Tstream>
		TvbusStimulSpike(TmenuHandle& _root, _Tstream* _stream)
			: TvbusSpike485(_root,_stream)
		{
			_stream->setMyAddr(7);
		}

		void sendMessage(dtypes::uint8* _buf, int size) override{

		}

	void virtual exec_applyToNet(Tevent* _ev){};
};
#endif
