class TvbusStimulSpike : public TvbusSpike485{
	private:
		typename Tstream::TmessageBufferRX FtxBuffer;

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

	public:
		using TvbusSpike485::TvbusSpike485;

		Tevent FevStimul;

		TvbusStimulSpike(TmenuHandle& _root, TuartBase* _stream) 
			: TvbusSpike485(_root,_stream)
		{
				FevStimul.setOwner(this);
				FevStimul.setPriority(2);
				FevStimul.signal();
		}

		void execute(Tevent* _ev) override{
			if (_ev == &FevStimul){
				stimul();
			}
			else{
				TvbusSpike485::execute(_ev);
			}
		};

		void sReadMessage(){
			FtxBuffer.length = Fps.length();
			readMessage(&FtxBuffer);
		}

		void stimul(){				
				//static typename Tuart::TmessageBufferTX FtxBuffer;
				//static TtxBuffer FtxBuffer;
				static int stimul_status = 0;
				static const TprotStream::t_prot_addr servAddr = 0x06;
				static const TprotStream::t_prot_addr clientAddr = 0x05;

				dtypes::uint32 uint32Val = 1;
				switch(stimul_status){
					case 0:
						if (1==2){
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

							//FPDW
							Fps.init(&FtxBuffer.data[0]);
							Fps.setHeader(6,5,0,TvbusProtocoll::ds_fpdw_req);
							Fps.writeByte(14); //client port
							Fps.writeByte(2);
							Fps.writeByte(0);
							Fps.writeByte(2);
							dtypes::uint32 val = 10;
							Fps.writeVal(val);
							Fps.init(&FtxBuffer.data[0],Fps.length());
							handleMessage();

							stimul_status = 1;
							FevStimul.setTimeEvent(10);

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
						stimul_status = 1;

					case 1:
						//first give an ID per dhcp set
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(0xFF,0xCC,0,TvbusProtocoll::dhcp_set);
						Fps.writeByte(servAddr);
						Fps.writeString("NUCLEO1");
						Fps.init(&FtxBuffer.data[0],Fps.length());
						Fdhcp.handleMessage(Fps);
						stimul_status = 20;
						FevStimul.setTimeEvent(10);

						//open port
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(servAddr,clientAddr,0,TvbusProtocoll::port_open);
						Fps.writeByte(1);//client port
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(servAddr,clientAddr,0,TvbusProtocoll::port_open);
						Fps.writeByte(2);//client port
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						
						/*
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(servAddr,clientAddr,0,TvbusProtocoll::ds_type_req);
						Fps.writeByte(13);	//client port
						Fps.writeByte(3);
						Fps.writeByte(0);
						Fps.writeByte(0);
						Fps.writeByte(255);
						//Fps.init(&FtxBuffer.data[0],Fps.length());
						FtxBuffer.length = Fps.length();
						readMessage(&FtxBuffer);
						*/
						stimul_status = 2;
						FevStimul.setTimeEvent(100);
						return;
								
					case 2:
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,0,TvbusProtocoll::ds_fpdw_req);
						Fps.writeByte(14); //client port
						Fps.writeByte(3);
						Fps.writeByte(3);
						Fps.writeByte(0);
						Fps.writeByte(255);
						//string "01"
						Fps.writeByte(3);
						Fps.writeByte(2);
						Fps.writeByte(0x30);
						Fps.writeByte(0x31);
						sReadMessage();
						
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(servAddr,clientAddr,0x10,TvbusProtocoll::ds_link_req);
						Fps.writeByte(3);
						Fps.writeByte(6);
						Fps.writeByte(0);
						Fps.writeByte(255);
						Fps.writeByte(1);//link time
						FtxBuffer.length = Fps.length();
						readMessage(&FtxBuffer);

						stimul_status = 3;
						FevStimul.setTimeEvent(100);
						return;

						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(servAddr,clientAddr,0x11,TvbusProtocoll::ds_link_req);
						Fps.writeByte(2);
						Fps.writeByte(0);
						Fps.writeByte(255);
						Fps.writeByte(1);//link time
						sReadMessage();
						return;

						setTimeEvent(100);
						stimul_status = 2;
						break;

					case 3:
						FtxBuffer.length = parseStr(&FtxBuffer.data[0],"00 01 02 00 25 02 03 06 01 01 64 00");
						readMessage(&FtxBuffer);	
						return;

					//FPDW
					case 20:
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,0,TvbusProtocoll::ds_fpdw_req);
						Fps.writeByte(14); //client port
						Fps.writeByte(2);
						Fps.writeByte(0);
						Fps.writeByte(2);
						uint32Val = 10;
						Fps.writeVal(uint32Val);
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						stimul_status = 21;
						FevStimul.setTimeEvent(10);
						break;
					
					//type request
					case 21:{
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,0,TvbusProtocoll::ds_type_req);
						Fps.writeByte(13);	//client port
						Fps.writeByte(2);
						Fps.writeByte(0);
						Fps.writeByte(255);
						auto len = Fps.length();
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						stimul_status = 30;	
						break;
					}
					
					case 30:
						//message to set val to 12 that failed in reality
						 
						//link data
						Fps.init(&FtxBuffer.data[0]);
						Fps.setHeader(6,5,10,TvbusProtocoll::ds_link);
						Fps.writeByte(1);		//firstItemIdx;
						uint32Val = 15;
						Fps.writeVal(uint32Val);
						Fps.init(&FtxBuffer.data[0],Fps.length());
						handleMessage();
						break;
				}
		}
};